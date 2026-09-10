#include "mods_window.hpp"

#include "format.hpp"
#include "logs_window.hpp"
#include "mod_browser.hpp"
#include "mod_texture_provider.hpp"
#include "modal.hpp"
#include "mods/svc/http.h"
#include "pane.hpp"
#include "queue_window.hpp"

#include <borealis/http.hpp>

#include "dusk/mod_loader.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/mods/svc/net.hpp"
#include "dusk/mods/svc/ui.hpp"

#include "m_Do/m_Do_audio.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <cstddef>

#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::ui {
namespace {

struct ModStatus {
    const char* badgeClass = "";
    const char* text = "";
};

ModStatus mod_status(const mods::LoadedMod& mod) {
    if (mod.loadFailed) {
        return {"failed", "Failed"};
    }
    if (mod.active) {
        return {"active", "Active"};
    }
    if (mod.suspendedByProvider) {
        return {"suspended", "Suspended"};
    }
    return {"", "Disabled"};
}

bool mod_uses_network(const mods::LoadedMod& mod) {
    return std::ranges::any_of(
        mod.manifestInfo.imports, [](const mods::ModManifestInfo::Import& serviceImport) {
            return mods::svc::is_network_service(serviceImport.id);
        });
}

class ModListEntry : public FluentComponent<ModListEntry> {
public:
    ModListEntry(Rml::Element* parent, const mods::LoadedMod& mod)
        : FluentComponent{append(parent, "mod-entry")} {
        auto* icon = append(mRoot, "mod-icon");
        if (!mod.metadata.iconPath.empty()) {
            auto* image = append(icon, "img");
            image->SetAttribute("src", mod_image_source(mod, mod.metadata.iconPath));
        }

        const auto status = mod_status(mod);
        auto* info = append(mRoot, "mod-info");
        auto* heading = append(info, "header");
        append_text(append(heading, "b"), mod.metadata.name);
        append_text(append(heading, "small"), fmt::format("v{}", mod.metadata.version));
        auto* sub = append(info, "small");
        append_text(sub, fmt::format("{} - ", mod.metadata.author));
        auto* statusElement = append(sub, "mod-status");
        if (status.badgeClass[0] != '\0') {
            statusElement->SetClass(status.badgeClass, true);
        }
        append_text(statusElement, status.text);
        if (mod_uses_network(mod)) {
            append_text(append(sub, "mod-network"), "Network");
        }
        append_text(append(info, "p"), snippet(mod.metadata.description, 90));
        mRoot->SetClass("inactive", !mod.active);
        mRoot->SetClass("failed", mod.loadFailed);

        on_nav_command([this](Rml::Event&, NavCommand cmd) {
            if (cmd == NavCommand::Confirm) {
                mRoot->DispatchEvent(Rml::EventId::Submit, {});
                return true;
            }
            return false;
        });
    }
};

class BrowseModsEntry final : public FluentComponent<BrowseModsEntry> {
public:
    BrowseModsEntry(Rml::Element* parent, std::function<void()> onOpen)
        : FluentComponent{append(parent, "mod-entry")} {
        mRoot->SetClass("browser", true);
        append(mRoot, "mod-icon");

        auto* info = append(mRoot, "mod-info");
        auto* heading = append(info, "header");
        append_text(append(heading, "b"), "Browse online mods");
        append_text(append(info, "p"), "Discover and install mods from the community.");

        on_nav_command([callback = std::move(onOpen)](Rml::Event&, NavCommand cmd) {
            if (cmd != NavCommand::Confirm) {
                return false;
            }
            callback();
            return true;
        });
    }
};

class InstallQueueEntry final : public FluentComponent<InstallQueueEntry> {
public:
    InstallQueueEntry(Rml::Element* parent, std::function<void()> onOpen)
        : FluentComponent{append(parent, "mod-entry")} {
        mRoot->SetClass("installs", true);
        append(mRoot, "mod-icon");

        auto* info = append(mRoot, "mod-info");
        auto* heading = append(info, "header");
        append_text(append(heading, "b"), "Installs");
        mSummary = append(info, "small");
        mProgress = append(info, "progress");

        on_nav_command([callback = std::move(onOpen)](Rml::Event&, NavCommand cmd) {
            if (cmd != NavCommand::Confirm) {
                return false;
            }
            callback();
            return true;
        });
        update();
    }

    void update() override {
        const auto current = mods::queue::first_active();
        const auto activeCount = mods::queue::active_count();
        const auto totalCount = mods::queue::item_count();

        if (!current) {
            set_text_content(mSummary, fmt::format("0 active · {} total", totalCount));
            mProgress->SetProperty("display", "none");
        } else {
            const float progress = current->total == 0 ?
                                       0.0f :
                                       std::clamp(static_cast<float>(current->completed) /
                                                      static_cast<float>(current->total),
                                           0.0f, 1.0f);
            set_text_content(
                mSummary, fmt::format("{} in queue · {:.0f}%", activeCount, progress * 100.0f));
            mProgress->SetAttribute("value", progress);
            mProgress->SetProperty("display", "block");
        }
        Component::update();
    }

private:
    Rml::Element* mSummary = nullptr;
    Rml::Element* mProgress = nullptr;
};

class ModDetailHeader : public FluentComponent<ModDetailHeader> {
public:
    ModDetailHeader(Rml::Element* parent, const mods::LoadedMod& mod,
        std::function<void()> onShowLogs, std::function<void()> onUninstall)
        : FluentComponent{append(parent, "mod-header")} {
        const bool hasBanner = !mod.metadata.bannerPath.empty();
        mRoot->SetClass(hasBanner ? "has-banner" : "no-banner", true);
        if (hasBanner) {
            mRoot->SetProperty("decorator", fmt::format(R"(image("{}" cover center center))",
                                                mod_image_source(mod, mod.metadata.bannerPath)));
        }

        auto* actions = append(mRoot, "mod-actions");
        const std::string modId = mod.metadata.id;
        if (mod.activation_failed()) {
            make_button(actions, "Retry").on_pressed([modId] {
                mods::ModLoader::instance().request_reactivate(modId);
            });
            make_button(actions, "Disable").on_pressed([modId] {
                mods::ModLoader::instance().request_disable(modId);
            });
        } else if (mod.is_enabled()) {
            if (!mod.nativeInPlace) {
                make_button(actions, "Reload").on_pressed([modId] {
                    mods::ModLoader::instance().request_reload(modId);
                });
            }
            make_button(actions, "Disable").on_pressed([modId] {
                mods::ModLoader::instance().request_disable(modId);
            });
        } else {
            make_button(actions, "Enable").on_pressed([modId] {
                mods::ModLoader::instance().request_enable(modId);
            });
        }
        make_button(actions, "Logs").on_pressed(std::move(onShowLogs));
        if (mods::ModLoader::instance().can_uninstall(mod)) {
            make_button(actions, "Uninstall").on_pressed(std::move(onUninstall));
        }

        listen(Rml::EventId::Keydown, [this](Rml::Event& event) {
            const auto cmd = map_nav_event(event);
            if (cmd != NavCommand::Left && cmd != NavCommand::Right) {
                return;
            }
            int index = -1;
            for (int i = 0; i < static_cast<int>(mButtons.size()); ++i) {
                if (mButtons[i]->contains(event.GetTargetElement())) {
                    index = i;
                    break;
                }
            }
            if (index == -1) {
                return;
            }
            const int next = index + (cmd == NavCommand::Right ? 1 : -1);
            if (next >= 0 && next < static_cast<int>(mButtons.size()) && mButtons[next]->focus()) {
                mDoAud_seStartMenu(kSoundItemFocus);
                event.StopPropagation();
            }
        });
    }

    bool focus() override {
        for (auto* button : mButtons) {
            if (button->focus()) {
                return true;
            }
        }
        return false;
    }

private:
    Button& make_button(Rml::Element* parent, Rml::String text) {
        auto button = std::make_unique<Button>(parent, std::move(text));
        Button& ref = *button;
        mChildren.emplace_back(std::move(button));
        mButtons.push_back(&ref);
        return ref;
    }

    std::vector<Button*> mButtons;
};

}  // namespace

ModsWindow::ModsWindow() : Window{Props{.tabBar = false, .styleSheets = {"res/rml/mods.rcss"}}} {
    mRoot->SetClass("mods", true);

    refresh_snapshot();
    mQueueItemCount = mods::queue::item_count();

    set_content([this](Rml::Element* content) { build_content(content); });
}

void ModsWindow::build_content(Rml::Element* content) {
    mEntries.clear();
    mEntryMods.clear();
    mBrowserEntry = nullptr;

    auto& listPane = add_child<Pane>(content, Pane::Type::Controlled);
    listPane.root()->SetClass("mod-list", true);
    auto& detailPane = add_child<Pane>(content, Pane::Type::Uncontrolled);
    detailPane.root()->SetClass("mod-detail", true);

    bool hasUtilityEntries = false;
    if (borealis::http::available()) {
        auto& browse =
            listPane.add_child<BrowseModsEntry>([this] { push(std::make_unique<ModBrowser>()); });
        mBrowserEntry = &browse;
        hasUtilityEntries = true;
        listPane.register_control(browse, detailPane, [this](Pane& pane) {
            mBrowserSelected = true;
            mSelectedMod = nullptr;
            mSelectedModId.clear();
            mark_current_entry();
        });
    }

    if (mQueueItemCount != 0) {
        listPane.add_child<InstallQueueEntry>([this] { push(std::make_unique<QueueWindow>()); });
        hasUtilityEntries = true;
    }

    const bool hasInstalledMods = !mods::ModLoader::instance().mods().empty();
    if (hasUtilityEntries && hasInstalledMods) {
        append(listPane.root(), "mod-list-separator");
    }

    if (!hasInstalledMods) {
        listPane.add_text("No mods installed.");
        mSelectedMod = nullptr;
        mSelectedModId.clear();
        if (borealis::http::available()) {
            mBrowserSelected = true;
        }
        mark_current_entry();
        return;
    }

    for (auto& trackedMod : mods::ModLoader::instance().mods()) {
        auto& entry = listPane.add_child<ModListEntry>(trackedMod);
        mEntries.push_back(&entry);
        mEntryMods.push_back(&trackedMod);
        listPane.register_control(entry, detailPane, [this, tracked = &trackedMod](Pane& pane) {
            mBrowserSelected = false;
            mSelectedMod = tracked;
            mSelectedModId = tracked->metadata.id;
            pane.clear();
            build_detail(pane, *tracked);
            mark_current_entry();
        });
    }

    if (mBrowserSelected && mBrowserEntry != nullptr) {
        mSelectedMod = nullptr;
        mSelectedModId.clear();
    } else {
        mSelectedMod = nullptr;
        if (!mSelectedModId.empty()) {
            const auto selected = std::ranges::find_if(
                mEntryMods, [this](const auto* mod) { return mod->metadata.id == mSelectedModId; });
            if (selected != mEntryMods.end()) {
                mSelectedMod = *selected;
            }
        }
        if (mSelectedMod == nullptr) {
            mSelectedMod = mEntryMods.front();
            mSelectedModId = mSelectedMod->metadata.id;
        }
        build_detail(detailPane, *mSelectedMod);
    }
    mark_current_entry();
}

void ModsWindow::build_detail(Pane& pane, mods::LoadedMod& mod) {
    pane.root()->SetAttribute("mod-id", mod.metadata.id);
    pane.add_child<ModDetailHeader>(
        mod, [this, id = mod.metadata.id] { push(std::make_unique<LogsWindow>(id)); },
        [this, tracked = &mod] { confirm_uninstall(*tracked); });

    auto* title = append(pane.root(), "mod-title");
    append_text(title, fmt::format("{} ", mod.metadata.name));
    append_text(append(title, "small"), fmt::format("v{}", mod.metadata.version));
    if (mod.loadFailed || mod.suspendedByProvider) {
        const auto status = mod_status(mod);
        append_text(title, "\u00a0");
        auto* badge = append(title, "status-badge");
        badge->SetClass(status.badgeClass, true);
        append_text(badge, status.text);
    }
    if (mod_uses_network(mod)) {
        append_text(title, "\u00a0");
        auto* badge = append(title, "status-badge");
        badge->SetClass("network", true);
        append_text(badge, "Network");
    }
    append_text(append(pane.root(), "mod-author"), fmt::format("by {}", mod.metadata.author));

    if (mod.loadFailed && !mod.failureReason.empty()) {
        auto* row = append(pane.root(), "mod-info-row");
        auto* label = append(row, "b");
        label->SetClass("failed", true);
        append_text(label, "Reason");
        append_text(append(row, "span"), mod.failureReason);
    } else if (mod.suspendedByProvider) {
        std::vector<std::string_view> providers;
        for (const auto& edge : mod.dependencies) {
            if (edge.required && edge.mod != nullptr && !edge.mod->active) {
                providers.push_back(edge.mod->metadata.name);
            }
        }
        auto* row = append(pane.root(), "mod-info-row");
        append_text(append(row, "b"), "Waiting on");
        append_text(append(row, "span"), fmt::format("{}", fmt::join(providers, ", ")));
    }

    std::vector<std::string_view> activeDependents;
    for (const auto& edge : mod.dependents) {
        if (edge.mod != nullptr && edge.mod->active) {
            activeDependents.push_back(edge.mod->metadata.name);
        }
    }
    if (mod.active && !activeDependents.empty()) {
        append_text(append(pane.root(), "mod-restart-note"),
            fmt::format(
                "Disabling or reloading also restarts: {}", fmt::join(activeDependents, ", ")));
    }

    if (!mod.metadata.description.empty()) {
        auto* description = append(pane.root(), "mod-description");
        append_text(description, mod.metadata.description);
    }

    if (mod.active) {
        mods::svc::ui_build_mods_panels(mod, pane);
    }
}

void ModsWindow::confirm_uninstall(const mods::LoadedMod& mod) {
    std::string body = "The mod package will be removed. Settings and saved data are kept.";
    std::vector<std::string_view> dependents;
    for (const auto& edge : mod.dependents) {
        if (!edge.required || edge.mod == nullptr) {
            continue;
        }
        dependents.push_back(edge.mod->metadata.name);
    }
    if (!dependents.empty()) {
        body = fmt::format(
            "{} Required dependents will be suspended: {}.", body, fmt::join(dependents, ", "));
    }

    push(std::make_unique<Modal>(Modal::Props{
        .title = fmt::format("Uninstall {}?", mod.metadata.name),
        .bodyText = std::move(body),
        .actions =
            {
                ModalAction{"Cancel", [](Modal& modal) { modal.pop(); }, {}},
                ModalAction{"Uninstall",
                    [id = mod.metadata.id](Modal& modal) {
                        mods::ModLoader::instance().request_uninstall(id);
                        modal.pop();
                    },
                    {}},
            },
        .variant = "danger",
        .icon = "warning",
    }));
}

void ModsWindow::refresh_snapshot() {
    mSnapshot.clear();
    auto& loader = mods::ModLoader::instance();
    mLoaderGeneration = loader.generation();
    for (auto& trackedMod : loader.mods()) {
        mSnapshot.push_back({
            .mod = &trackedMod,
            .active = trackedMod.active,
            .loadFailed = trackedMod.loadFailed,
            .enabled = trackedMod.is_enabled(),
            .suspended = trackedMod.suspendedByProvider,
            .cacheGeneration = trackedMod.cacheGeneration,
        });
    }
}

void ModsWindow::mark_current_entry() {
    if (mBrowserEntry != nullptr) {
        mBrowserEntry->root()->SetClass("current", mBrowserSelected);
    }
    for (size_t i = 0; i < mEntries.size(); ++i) {
        mEntries[i]->root()->SetClass("current", mEntryMods[i] == mSelectedMod);
    }
}

void ModsWindow::update() {
    auto& loader = mods::ModLoader::instance();
    bool dirty = loader.generation() != mLoaderGeneration;
    if (dirty) {
        mSelectedMod = nullptr;
        refresh_snapshot();
    } else {
        for (auto& snapshot : mSnapshot) {
            const auto& mod = *snapshot.mod;
            if (mod.active != snapshot.active || mod.loadFailed != snapshot.loadFailed ||
                mod.is_enabled() != snapshot.enabled ||
                mod.suspendedByProvider != snapshot.suspended ||
                mod.cacheGeneration != snapshot.cacheGeneration)
            {
                snapshot.active = mod.active;
                snapshot.loadFailed = mod.loadFailed;
                snapshot.enabled = mod.is_enabled();
                snapshot.suspended = mod.suspendedByProvider;
                snapshot.cacheGeneration = mod.cacheGeneration;
                dirty = true;
            }
        }
    }
    const auto queueItemCount = mods::queue::item_count();
    if (queueItemCount != mQueueItemCount) {
        mQueueItemCount = queueItemCount;
        dirty = true;
    }
    if (dirty) {
        auto* focused = mDocument != nullptr ? mDocument->GetFocusLeafNode() : nullptr;
        bool hadContentFocus = false;
        for (auto* node = focused; node != nullptr; node = node->GetParentNode()) {
            if (node == mContentRoot) {
                hadContentFocus = true;
                break;
            }
        }
        rebuild_content();
        if (hadContentFocus) {
            if (mBrowserSelected && mBrowserEntry != nullptr) {
                mBrowserEntry->focus();
            } else {
                for (size_t i = 0; i < mEntryMods.size(); ++i) {
                    if (mEntryMods[i] == mSelectedMod) {
                        mEntries[i]->focus();
                        break;
                    }
                }
            }
        }
    }

    if (mSelectedMod != nullptr && mSelectedMod->active) {
        mods::svc::ui_update_mods_panels(*mSelectedMod);
    }

    Window::update();
}

}  // namespace dusk::ui
