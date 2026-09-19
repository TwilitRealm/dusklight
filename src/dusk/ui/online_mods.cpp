#include "online_mods.hpp"

#include "bool_button.hpp"
#include "document.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/settings.h"
#include "format.hpp"
#include "icon_button.hpp"
#include "mod_browser.hpp"
#include "mod_texture_provider.hpp"
#include "mod_updates.hpp"
#include "nav_group.hpp"
#include "package_row.hpp"
#include "pane.hpp"
#include "remote_texture_provider.hpp"

#include <algorithm>
#include <borealis/http.hpp>
#include <fmt/format.h>

namespace dusk::ui {
namespace {
using mods::queue::is_completed;
using mods::queue::is_failed;
using mods::queue::State;

class QueueRow final : public NavGroup {
public:
    QueueRow(Rml::Element* parent, const mods::queue::Item& item)
        : NavGroup{append(parent, "queue-row"), {.layout = Layout::Horizontal}}, mId{item.id} {
        mRoot->SetAttribute("queue-id", mId);
        auto& row = add_child<PackageRow>();
        mRow = &row;
        auto& pause = add_existing_item<IconButton>(
            row.actions_root(), IconButton::Props{.icon = "pause", .label = "Pause"});
        mPause = &pause;
        pause.root()->SetClass("compact", true);
        // The update action keeps focus when its package becomes a download.
        pause.root()->SetAttribute("focus-key", "mod-action-" + item.modId);
        pause.on_pressed([this] {
            if (const auto item = mods::queue::find(mId)) {
                if (item->state == State::Paused) {
                    mods::queue::resume(mId);
                } else if (is_failed(item->state)) {
                    mods::queue::retry(mId);
                } else {
                    mods::queue::pause(mId);
                }
            }
        });
        auto& cancel = add_existing_item<IconButton>(
            row.actions_root(), IconButton::Props{.icon = "close", .label = "Cancel"});
        mCancel = &cancel;
        cancel.root()->SetClass("compact", true);
        cancel.root()->SetAttribute("focus-key", "queue-clear-" + item.modId);
        cancel.on_pressed([this] {
            if (const auto item = mods::queue::find(mId)) {
                if (mods::queue::is_terminal(item->state)) {
                    mods::queue::clear(mId);
                } else {
                    mods::queue::cancel(mId);
                }
            }
        });
        update();
    }

    void update() override {
        const auto item = mods::queue::find(mId);
        if (!item) {
            return;
        }
        const bool failed = is_failed(item->state);
        std::string detail = format_bytes(item->total);
        if (failed) {
            detail = item->message;
        } else if (item->completed || item->state == State::Downloading ||
                   item->state == State::Paused)
        {
            detail =
                fmt::format("{} / {}", format_bytes(item->completed), format_bytes(item->total));
        } else if (!item->message.empty()) {
            detail += " · " + item->message;
        }
        std::optional<float> progress;
        if (!mods::queue::is_terminal(item->state) && item->total) {
            progress = std::clamp(static_cast<float>(item->completed) / item->total, 0.0f, 1.0f);
        }
        auto status = state_label(*item);
        if (item->state == State::Installed) {
            status.clear();
            detail = "Installed · " + format_bytes(item->total);
        }
        const auto version = item->previousVersion.empty() ?
                                 item->version :
                                 fmt::format("{} → {}", item->previousVersion, item->version);
        mRow->set_package(
            item->name, version, status, detail, queue_state_class(item->state), progress);
        std::string iconSource;
        if (item->icon && !item->icon->url.empty()) {
            iconSource =
                remote_image_source(item->icon->url, item->icon->width, item->icon->height);
        } else if (const auto* mod = mods::ModLoader::instance().find_mod(item->modId);
            mod && !mod->metadata.iconPath.empty())
        {
            iconSource = mod_image_source(*mod, mod->metadata.iconPath);
        }
        mRow->set_icon(std::move(iconSource));

        const bool canPause = (!item->local && item->state == State::Queued) ||
                              item->state == State::Downloading || item->state == State::Paused ||
                              item->state == State::Retrying || failed;
        set_display(
            mPause->root(), canPause ? Rml::Style::Display::Flex : Rml::Style::Display::None);
        mPause->set_icon(failed                       ? "refresh" :
                         item->state == State::Paused ? "play_arrow" :
                                                        "pause");
        mPause->set_label(failed ? "Retry" : item->state == State::Paused ? "Resume" : "Pause");
        set_display(mCancel->root(),
            item->state == State::Handoff ? Rml::Style::Display::None : Rml::Style::Display::Flex);
        mCancel->set_label(failed ? "Dismiss" : is_completed(item->state) ? "Clear" : "Cancel");
        Component::update();
    }

private:
    std::string mId;
    PackageRow* mRow = nullptr;
    IconButton* mPause = nullptr;
    IconButton* mCancel = nullptr;
};

class DownloadsHeader final : public NavGroup {
public:
    explicit DownloadsHeader(Rml::Element* parent)
        : NavGroup{append(parent, "online-section-heading"), {.layout = Layout::Horizontal}} {
        append_text(append(mRoot, "h2"), "Downloads & installs");
        auto& pause = add_item<Button>("Pause all");
        mPause = &pause;
        pause.root()->SetAttribute("focus-key", "downloads-pause-all");
        pause.on_pressed([this] {
            if (mResume) {
                for (const auto& item : mods::queue::items()) {
                    if (item.state == State::Paused) {
                        mods::queue::resume(item.id);
                    }
                }
            } else {
                mods::queue::pause_all();
            }
        });
        update();
    }

    void update() override {
        bool canPause = false;
        bool canResume = false;
        for (const auto& item : mods::queue::items()) {
            canPause |=
                !item.local && (item.state == State::Queued || item.state == State::Downloading ||
                                   item.state == State::Retrying);
            canResume |= item.state == State::Paused;
        }
        mResume = !canPause && canResume;
        mPause->set_text(mResume ? "Resume all" : "Pause all");
        set_display(mPause->root(),
            canPause || canResume ? Rml::Style::Display::Block : Rml::Style::Display::None);
        Component::update();
    }

private:
    Button* mPause = nullptr;
    bool mResume = false;
};

class CompletedHeader final : public NavGroup {
public:
    explicit CompletedHeader(Rml::Element* parent)
        : NavGroup{append(parent, "online-section-heading"), {.layout = Layout::Horizontal}} {
        mRoot->SetClass("completed", true);
        append_text(append(mRoot, "h2"), "Completed");
        auto& clear = add_item<Button>("Clear");
        clear.root()->SetAttribute("focus-key", "completed-clear");
        clear.on_pressed([] {
            for (const auto& item : mods::queue::items()) {
                if (is_completed(item.state)) {
                    mods::queue::clear(item.id);
                }
            }
        });
    }
};
}  // namespace

void build_online_mods(
    Pane& pane, Document& document, std::unordered_set<std::string>& expandedChangelogs) {
    auto& browse = pane.add_button("");
    browse.root()->SetClass("browse-mods-action", true);
    browse.root()->SetAttribute("focus-key", "online-browse");
    auto* copy = append(browse.root(), "browse-copy");
    append_text(append(copy, "h2"), "Browse online mods");
    append_text(append(copy, "p"),
        "Discover texture packs, gameplay mods, custom models, and more from the community.");
    append_text(append(browse.root(), "icon"), material_icon("arrow_forward"));
    browse.set_disabled(!borealis::http::available());
    browse.on_pressed([&document] { document.push(std::make_unique<ModBrowser>()); });

    const auto items = mods::queue::items();
    const auto count =
        std::ranges::count_if(items, [](const auto& item) { return is_completed(item.state); });
    if (items.size() > count) {
        pane.add_child<DownloadsHeader>();
        for (const auto& item : items) {
            if (!is_completed(item.state)) {
                pane.add_child<QueueRow>(item);
            }
        }
    }
    build_mod_updates(pane, expandedChangelogs);
    if (count > 0) {
        pane.add_child<CompletedHeader>();
        for (const auto& item : items) {
            if (is_completed(item.state)) {
                pane.add_child<QueueRow>(item);
            }
        }
    }
    auto& automatic = pane.add_child<BoolButton>(BoolButton::Props{
        .key = "Check for updates",
        .getValue = [] { return getSettings().backend.checkForModUpdates.getValue(); },
        .setValue =
            [](bool value) {
                if (value == getSettings().backend.checkForModUpdates.getValue()) {
                    return;
                }
                getSettings().backend.checkForModUpdates.setValue(value);
                config::save();
            },
        .isModified =
            [] {
                return getSettings().backend.checkForModUpdates.getValue() !=
                       getSettings().backend.checkForModUpdates.getDefaultValue();
            },
    });
    automatic.root()->SetAttribute("focus-key", "online-auto-check");
}
}  // namespace dusk::ui
