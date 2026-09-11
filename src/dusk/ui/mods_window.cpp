#include "mods_window.hpp"

#include "format.hpp"
#include "icon_button.hpp"
#include "logs_window.hpp"
#include "mod_browser.hpp"
#include "mod_texture_provider.hpp"
#include "modal.hpp"
#include "mods/svc/http.h"
#include "pane.hpp"
#include "queue_window.hpp"

#include <borealis/http.hpp>

#include "dusk/data.hpp"
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
#include <optional>
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

enum class ModAction {
    Retry,
    Reload,
    Enable,
    Disable,
    Logs,
    OpenFolder,
    Uninstall,
};

struct ModActionInfo {
    ModAction action;
    const char* text;
    const char* icon;
};

std::vector<ModActionInfo> available_mod_actions(const mods::LoadedMod& mod) {
    std::vector<ModActionInfo> actions;
    if (mod.activation_failed()) {
        actions.push_back({ModAction::Retry, "Retry", "replay"});
        actions.push_back({ModAction::Disable, "Disable", "pause"});
    } else if (mod.is_enabled()) {
        if (!mod.nativeInPlace) {
            actions.push_back({ModAction::Reload, "Reload", "refresh"});
        }
        actions.push_back({ModAction::Disable, "Disable", "pause"});
    } else {
        actions.push_back({ModAction::Enable, "Enable", "play_arrow"});
    }
    actions.push_back({ModAction::Logs, "Logs", "notes"});
    if (data::manager().capabilities().canOpenFolder) {
        actions.push_back({ModAction::OpenFolder, "Open folder", "folder_open"});
    }
    if (mods::ModLoader::instance().can_uninstall(mod)) {
        actions.push_back({
            ModAction::Uninstall,
            mod.hasBundledCopy ? "Remove update" : "Uninstall",
            "delete",
        });
    }
    return actions;
}

class ModListEntry : public FluentComponent<ModListEntry> {
public:
    ModListEntry(Rml::Element* parent, const mods::LoadedMod& mod)
        : FluentComponent{append(parent, "mod-entry")} {
        mRoot->SetAttribute("mod-id", mod.metadata.id);
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
    ModDetailHeader(
        Rml::Element* parent, const mods::LoadedMod& mod, std::vector<ContextMenu::Item> items)
        : FluentComponent{append(parent, "mod-header")} {
        mRoot->SetAttribute("mod-id", mod.metadata.id);
        const bool hasBanner = !mod.metadata.bannerPath.empty();
        mRoot->SetClass(hasBanner ? "has-banner" : "no-banner", true);
        mRoot->SetClass("inactive", !mod.active);
        if (hasBanner) {
            auto* image = append(mRoot, "mod-header-image");
            image->SetProperty("decorator", fmt::format(R"(image("{}" cover center center))",
                                                mod_image_source(mod, mod.metadata.bannerPath)));
        }

        auto* actions = append(mRoot, "mod-actions");
        for (auto& item : items) {
            auto& button = make_button(actions, item);
            button.on_pressed(std::move(item.onPressed));
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
    Button& make_button(Rml::Element* parent, const ContextMenu::Item& item) {
        auto button = std::make_unique<IconButton>(
            parent, IconButton::Props{.icon = item.icon, .label = item.text});
        Button& ref = *button;
        mChildren.emplace_back(std::move(button));
        mButtons.push_back(&ref);
        return ref;
    }

    std::vector<Button*> mButtons;
};

}  // namespace

ModsWindow::ModsWindow()
    : Window{Props{.tabBar = false, .styleSheets = {"res/rml/mods.rcss"}}},
      mContextMenu{*this, mRoot, "mod-entry, mod-header", [this](Rml::Element* target) {
                       const auto id = target->GetAttribute<Rml::String>("mod-id", "");
                       auto* mod = mods::ModLoader::instance().find_mod(id);
                       return mod != nullptr ? mod_actions(*mod, true) :
                                               std::vector<ContextMenu::Item>{};
                   }} {
    mRoot->SetClass("mods", true);

    refresh_snapshot();
    mQueueItemCount = mods::queue::item_count();

    set_content([this](Rml::Element* content) { build_content(content); });
}

void ModsWindow::hide(bool close) {
    mContextMenu.dismiss();
    Window::hide(close);
}

bool ModsWindow::select_mod(std::string_view id) {
    if (mods::ModLoader::instance().find_mod(id) == nullptr) {
        return false;
    }
    mContextMenu.dismiss();
    mSelectedModId = id;
    mSelectedMod = nullptr;
    mBrowserSelected = false;
    mFocusSelectedMod = true;
    refresh_snapshot();
    mQueueItemCount = mods::queue::item_count();
    rebuild_content();
    return true;
}

bool ModsWindow::focus() {
    if (mFocusSelectedMod) {
        mDocument->UpdateDocument();
        for (size_t i = 0; i < mEntryMods.size(); ++i) {
            if (mEntryMods[i]->metadata.id == mSelectedModId && mEntries[i]->focus()) {
                mEntries[i]->set_selected(true);
                mFocusSelectedMod = false;
                return true;
            }
        }
    }
    return Window::focus();
}

std::vector<ContextMenu::Item> ModsWindow::mod_actions(
    const mods::LoadedMod& mod, bool contextMenu) {
    std::vector<ContextMenu::Item> items;
    for (const auto& info : available_mod_actions(mod)) {
        if (!contextMenu && info.action == ModAction::OpenFolder) {
            continue;
        }
        items.push_back({
            .text = info.text,
            .icon = info.icon,
            .onPressed =
                [this, id = mod.metadata.id, action = info.action] {
                    auto& loader = mods::ModLoader::instance();
                    auto* current = loader.find_mod(id);
                    if (current == nullptr) {
                        return;
                    }
                    const auto actions = available_mod_actions(*current);
                    if (std::ranges::none_of(
                            actions, [action](const auto& info) { return info.action == action; }))
                    {
                        return;
                    }
                    switch (action) {
                    case ModAction::Retry:
                        loader.request_reactivate(id);
                        break;
                    case ModAction::Reload:
                        loader.request_reload(id);
                        break;
                    case ModAction::Enable:
                        loader.request_enable(id);
                        break;
                    case ModAction::Disable:
                        loader.request_disable(id);
                        break;
                    case ModAction::Logs:
                        push(std::make_unique<LogsWindow>(id));
                        break;
                    case ModAction::Uninstall:
                        confirm_uninstall(*current);
                        break;
                    case ModAction::OpenFolder: {
                        const auto folder = current->fromDirectory ? current->modPath :
                                                                     current->modPath.parent_path();
                        if (!data::manager().open_folder(folder)) {
                            push(std::make_unique<Modal>(Modal::Props{
                                .title = "Could not open folder",
                                .bodyText =
                                    "The mod folder could not be opened in the file browser.",
                                .actions = {{"OK", [](Modal& modal) { modal.pop(); }, {}}},
                            }));
                        }
                        break;
                    }
                    }
                },
            .destructive = info.action == ModAction::Uninstall,
            .separatorBefore = info.action == ModAction::Uninstall,
        });
    }
    return items;
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
    pane.add_child<ModDetailHeader>(mod, mod_actions(mod, false));

    auto* title = append(pane.root(), "mod-title");
    append_text(title, fmt::format("{} ", mod.metadata.name));
    append_text(append(title, "small"), fmt::format("v{}", mod.metadata.version));
    if (mod_uses_network(mod)) {
        append_text(title, "\u00a0");
        auto* badge = append(title, "status-badge");
        badge->SetClass("network", true);
        append_text(badge, "Network");
    }
    auto* author = append(pane.root(), "mod-author");
    append_text(author, fmt::format("by {}\u00a0·\u00a0", mod.metadata.author));
    const auto status = mod_status(mod);
    auto* badge = append(author, "status-badge");
    if (status.badgeClass[0] != '\0') {
        badge->SetClass(status.badgeClass, true);
    }
    append_text(badge, status.text);

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
    const std::string action = mod.hasBundledCopy ? "Remove update" : "Uninstall";
    std::string body = mod.hasBundledCopy ?
                           "Installed mod will be reverted back to the bundled version. Settings "
                           "and saved data are kept." :
                           "Installed mod will be removed. Settings and saved data are kept.";
    std::vector<std::string_view> dependents;
    for (const auto& edge : mod.dependents) {
        if (!edge.required || edge.mod == nullptr) {
            continue;
        }
        dependents.push_back(edge.mod->metadata.name);
    }
    if (!dependents.empty()) {
        body = fmt::format("{} Required dependents: {}.", body, fmt::join(dependents, ", "));
    }

    push(std::make_unique<Modal>(Modal::Props{
        .title = mod.hasBundledCopy ? fmt::format("Revert {}?", mod.metadata.name) :
                                      fmt::format("Uninstall {}?", mod.metadata.name),
        .bodyText = std::move(body),
        .actions =
            {
                ModalAction{"Cancel", [](Modal& modal) { modal.pop(); }, {}},
                ModalAction{action,
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
        mContextMenu.dismiss();
        const auto previousModId = mSelectedModId;
        std::optional<Rml::Property> previousBannerFilter;
        if (auto* image = mContentRoot->QuerySelector("mod-header-image")) {
            previousBannerFilter = *image->GetProperty(Rml::PropertyId::Filter);
        }
        auto* list = mContentRoot->QuerySelector("pane.mod-list");
        const float listScrollTop = list != nullptr ? list->GetScrollTop() : 0.0f;
        auto* focused = mDocument != nullptr ? mDocument->GetFocusLeafNode() : nullptr;
        bool hadContentFocus = false;
        for (auto* node = focused; node != nullptr; node = node->GetParentNode()) {
            if (node == mContentRoot) {
                hadContentFocus = true;
                break;
            }
        }
        rebuild_content();
        mDocument->UpdateDocument();
        if (hadContentFocus) {
            if (mBrowserSelected && mBrowserEntry != nullptr) {
                mBrowserEntry->root()->Focus(true);
            } else {
                for (size_t i = 0; i < mEntryMods.size(); ++i) {
                    if (mEntryMods[i] == mSelectedMod) {
                        mEntries[i]->root()->Focus(true);
                        break;
                    }
                }
            }
        }
        if (previousBannerFilter && previousModId == mSelectedModId) {
            mDocument->UpdateDocument();
            if (auto* image = mContentRoot->QuerySelector("mod-header-image")) {
                const auto target = *image->GetProperty(Rml::PropertyId::Filter);
                if (*previousBannerFilter != target) {
                    image->SetProperty(Rml::PropertyId::Filter, *previousBannerFilter);
                    image->Animate(Rml::PropertyId::Filter, target, 0.2f,
                        Rml::Tween{Rml::Tween::Cubic, Rml::Tween::InOut}, 1, false);
                }
            }
        }
        if (auto* refreshedList = mContentRoot->QuerySelector("pane.mod-list")) {
            refreshedList->SetScrollTop(listScrollTop);
        }
    }

    if (mSelectedMod != nullptr && mSelectedMod->active) {
        mods::svc::ui_update_mods_panels(*mSelectedMod);
    }

    Window::update();
}

}  // namespace dusk::ui
