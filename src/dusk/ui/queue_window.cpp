#include "queue_window.hpp"

#include "button.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/queue.hpp"
#include "fmt/format.h"
#include "format.hpp"
#include "mod_texture_provider.hpp"
#include "package_row.hpp"
#include "pane.hpp"
#include "remote_texture_provider.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::ui {
namespace {

void set_icon_button(Button& button, const Rml::String& glyph, const Rml::String& label,
    Rml::String& currentGlyph, Rml::String& currentLabel) {
    if (currentGlyph != glyph) {
        clear_children(button.root());
        append_text(append(button.root(), "icon"), glyph);
        currentGlyph = glyph;
    }
    if (currentLabel != label) {
        button.root()->SetAttribute("aria-label", label);
        button.root()->SetAttribute("title", label);
        currentLabel = label;
    }
}

class QueueRow final : public PackageRow {
public:
    QueueRow(Rml::Element* parent, std::string id) : PackageRow{parent}, mId{std::move(id)} {
        auto* actions = actions_root();
        auto pause = std::make_unique<Button>(actions, "");
        mPause = pause.get();
        mPause->root()->SetClass("icon-action", true);
        mPause->on_pressed([this] {
            const auto item = mods::queue::find(mId);
            if (!item) {
                return;
            }
            switch (item->state) {
            case mods::queue::State::Paused:
                mods::queue::resume(mId);
                break;
            case mods::queue::State::Failed:
            case mods::queue::State::InstallFailed:
                mods::queue::retry(mId);
                break;
            default:
                mods::queue::pause(mId);
                break;
            }
        });
        mChildren.push_back(std::move(pause));

        auto cancel = std::make_unique<Button>(actions, "");
        mCancel = cancel.get();
        mCancel->root()->SetClass("icon-action", true);
        mCancel->on_pressed([this] {
            const auto item = mods::queue::find(mId);
            if (!item) {
                return;
            }
            if (mods::queue::is_terminal(item->state)) {
                if (item->state == mods::queue::State::InstallFailed) {
                    auto& loader = mods::ModLoader::instance();
                    if (const auto* mod = loader.find_mod(item->modId);
                        mod != nullptr && loader.can_uninstall(*mod))
                    {
                        loader.request_uninstall(item->modId);
                    }
                }
                mods::queue::clear(mId);
                return;
            }
            mods::queue::cancel(mId);
        });
        set_icon_button(*mCancel, "\uE5CD", "Cancel", mCancelGlyph, mCancelLabel);
        mChildren.push_back(std::move(cancel));
        update();
    }

    void update() override {
        const auto item = mods::queue::find(mId);
        if (!item) {
            mRoot->SetProperty("display", "none");
            return;
        }
        mRoot->SetProperty("display", "flex");
        const float progress =
            item->total == 0 ?
                0.0f :
                std::clamp(static_cast<float>(item->completed) / static_cast<float>(item->total),
                    0.0f, 1.0f);
        std::string detail;
        if (item->completed != 0 || item->state == mods::queue::State::Downloading ||
            item->state == mods::queue::State::Paused)
        {
            detail =
                fmt::format("{} / {}", format_bytes(item->completed), format_bytes(item->total));
        } else {
            detail = format_bytes(item->total);
        }
        if (!item->message.empty() && (item->state == mods::queue::State::Failed ||
                                          item->state == mods::queue::State::InstallFailed))
        {
            detail = item->message;
        } else if (!item->message.empty()) {
            detail = fmt::format("{} · {}", detail, item->message);
        }
        auto status = state_label(*item);
        if (item->state == mods::queue::State::Installed) {
            status.clear();
            detail = fmt::format("Installed · {}", format_bytes(item->total));
        }
        const std::optional<float> progressValue =
            item->state == mods::queue::State::Installed ||
                    item->state == mods::queue::State::Canceled ?
                std::nullopt :
                std::optional{progress};
        set_package(item->name, item->version, std::move(status), detail,
            queue_state_class(item->state), progressValue);

        std::string iconSource;
        if (item->icon && !item->icon->url.empty()) {
            iconSource =
                remote_image_source(item->icon->url, item->icon->width, item->icon->height);
        } else if (const auto* mod = mods::ModLoader::instance().find_mod(item->modId);
            mod != nullptr && !mod->metadata.iconPath.empty())
        {
            iconSource = mod_image_source(*mod, mod->metadata.iconPath);
        }
        set_icon(std::move(iconSource));

        const bool pauseVisible = (!item->local && item->state == mods::queue::State::Queued) ||
                                  item->state == mods::queue::State::Downloading ||
                                  item->state == mods::queue::State::Paused ||
                                  item->state == mods::queue::State::Retrying ||
                                  item->state == mods::queue::State::Failed ||
                                  item->state == mods::queue::State::InstallFailed;
        mPause->root()->SetProperty("display", pauseVisible ? "flex" : "none");
        if (item->state == mods::queue::State::Paused) {
            set_icon_button(*mPause, "\uE037", "Resume", mPauseGlyph, mPauseLabel);
        } else if (item->state == mods::queue::State::Failed ||
                   item->state == mods::queue::State::InstallFailed)
        {
            set_icon_button(*mPause, "\uE5D5", "Retry", mPauseGlyph, mPauseLabel);
        } else {
            set_icon_button(*mPause, "\uE034", "Pause", mPauseGlyph, mPauseLabel);
        }

        mCancel->root()->SetProperty(
            "display", item->state == mods::queue::State::Handoff ? "none" : "flex");
        set_icon_button(*mCancel, "\uE5CD",
            mods::queue::is_terminal(item->state) ? "Clear" : "Cancel", mCancelGlyph, mCancelLabel);
        Component::update();
    }

private:
    std::string mId;
    Button* mPause = nullptr;
    Button* mCancel = nullptr;
    Rml::String mPauseGlyph;
    Rml::String mPauseLabel;
    Rml::String mCancelGlyph;
    Rml::String mCancelLabel;
};

}  // namespace

QueueWindow::QueueWindow(std::string focusId)
    : Modal{Props{
          .title = "Download queue",
          .actions =
              {
                  ModalAction{"Close", [](Modal& modal) { modal.pop(); }, {}},
                  ModalAction{"Pause all", [](Modal&) { mods::queue::pause_all(); },
                      [] { return !mods::queue::has_active_items(); }},
                  ModalAction{"Clear finished", [](Modal&) { mods::queue::clear_finished(); },
                      [] { return mods::queue::item_count() == mods::queue::active_count(); }},
              },
          .variant = "install-queue",
          .icon = "download",
      }},
      mFocusId{std::move(focusId)} {
    content_pane();
    refresh_queue();
}

void QueueWindow::update() {
    if (!mItemIds.empty() && mods::queue::item_count() == 0) {
        pop();
        return;
    }
    refresh_queue();
    Modal::update();
}

void QueueWindow::refresh_queue() {
    const auto queueItems = mods::queue::items();
    std::vector<std::string> ids;
    ids.reserve(queueItems.size());
    size_t active = 0;
    for (const auto& item : queueItems) {
        ids.push_back(item.id);
        if (!mods::queue::is_terminal(item.state)) {
            ++active;
        }
    }
    if (ids != mItemIds) {
        auto& pane = content_pane();
        pane.clear();
        mItemIds = std::move(ids);
        if (queueItems.empty()) {
            pane.add_text("No installs.");
        } else {
            for (const auto& item : queueItems) {
                auto& row = pane.add_child<QueueRow>(item.id);
                if (!mFocusId.empty() && item.id == mFocusId) {
                    row.focus();
                    mFocusId.clear();
                }
            }
        }
    }
    set_body_text(fmt::format("{} active · {} total", active, queueItems.size()));
}

}  // namespace dusk::ui
