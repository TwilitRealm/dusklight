#include "mods_window.hpp"

#include "clamped_text.hpp"
#include "dusk/mods/updates.hpp"
#include "format.hpp"
#include "icon_button.hpp"
#include "i18n.hpp"
#include "logs_window.hpp"
#include "mod_texture_provider.hpp"
#include "mod_updates.hpp"
#include "modal.hpp"
#include "mods/svc/http.h"
#include "online_mods.hpp"
#include "package_row.hpp"
#include "pane.hpp"

#include <borealis/http.hpp>

#include "dusk/data.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/mods/svc/net.hpp"
#include "dusk/mods/svc/ui.hpp"

#include "m_Do/m_Do_audio.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <tracy/Tracy.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::ui {
namespace {
using i18n::tr_str;

struct ModStatus {
    const char* badgeClass = "";
    std::string text;
};

ModStatus mod_status(const mods::LoadedMod& mod) {
    if (mod.loadFailed) {
        return {"error", tr_str("mods.failed")};
    }
    if (mod.active) {
        return {"success", tr_str("mods.active")};
    }
    if (mod.suspendedByProvider) {
        return {"suspended", tr_str("mods.suspended")};
    }
    return {"", tr_str("mods.disabled")};
}

bool mod_uses_network(const mods::LoadedMod& mod) {
    return std::ranges::any_of(
        mod.manifestInfo.imports, [](const mods::ModManifestInfo::Import& serviceImport) {
            return mods::svc::is_network_service(serviceImport.id);
        });
}

enum class ModAction {
    Update,
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
    if (const auto* update = mods::updates::find(mod.metadata.id); update && update->actionable) {
        actions.push_back({ModAction::Update, tr_str("mods.update").c_str(), "download"});
    }
    if (mod.activation_failed()) {
        actions.push_back({ModAction::Retry, tr_str("mods.retry").c_str(), "replay"});
        actions.push_back({ModAction::Disable, tr_str("mods.disable").c_str(), "pause"});
    } else if (mod.is_enabled()) {
        if (!mod.nativeInPlace) {
            actions.push_back({ModAction::Reload, tr_str("mods.reload").c_str(), "refresh"});
        }
        actions.push_back({ModAction::Disable, tr_str("mods.disable").c_str(), "pause"});
    } else {
        actions.push_back({ModAction::Enable, tr_str("mods.enable").c_str(), "play_arrow"});
    }
    actions.push_back({ModAction::Logs, tr_str("mods.logs").c_str(), "notes"});
    if (data::manager().capabilities().canOpenFolder) {
        actions.push_back({ModAction::OpenFolder, tr_str("mods.open_folder").c_str(), "folder_open"});
    }
    if (mods::ModLoader::instance().can_uninstall(mod)) {
        actions.push_back({
            ModAction::Uninstall,
            (mod.hasBundledCopy ? tr_str("mods.remove_update") : tr_str("mods.uninstall")).c_str(),
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
        mIcon = append(mRoot, "mod-icon");
        mInactive = !mod.active;
        if (!mod.metadata.iconPath.empty()) {
            auto* image = append(mIcon, "mod-icon-image");
            image->SetProperty(
                "decorator", fmt::format(R"(image-effects("{}" fill))",
                                 escape(mod_image_source(mod, mod.metadata.iconPath))));
        }

        const auto status = mod_status(mod);
        auto* info = append(mRoot, "mod-info");
        auto* heading = append(info, "header");
        append_text(append(heading, "b"), mod.metadata.name);
        if (const auto* update = mods::updates::find(mod.metadata.id); update && update->actionable)
        {
            append_text(append(heading, "update-badge"), tr_str("mods.update"));
        }
        if (mod_uses_network(mod)) {
            append_text(append(heading, "mod-network"), tr_str("mods.network_badge"));
        }
        auto* sub = append(info, "mod-meta");
        append_text(append(sub, "mod-author"), mod.metadata.author);
        append_text(append(sub, "span"), "·");
        append_text(append(sub, "mod-version"), fmt::format("v{}", mod.metadata.version));
        append_text(append(sub, "span"), "·");
        auto* statusElement = append(sub, "status-badge");
        if (status.badgeClass[0] != '\0') {
            statusElement->SetClass(status.badgeClass, true);
        }
        append_text(statusElement, status.text);
        mChildren.emplace_back(
            std::make_unique<ClampedText>(append(info, "p"), mod.metadata.description, 2));
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

    void update() override {
        if (mInactive) {
            const auto setGray = [this](const char* source, Rml::PropertyId target) {
                auto color = mIcon->GetProperty(source)->Get<Rml::Colourb>();
                const auto gray = static_cast<Rml::byte>(std::lround(
                    color.red * 0.2126f + color.green * 0.7152f + color.blue * 0.0722f));
                color.red = color.green = color.blue = gray;
                const Rml::Property value{color, Rml::Unit::COLOUR};
                const auto* current = mIcon->GetLocalProperty(target);
                if (current == nullptr || *current != value) {
                    mIcon->SetProperty(target, value);
                }
            };
            setGray("mod-icon-tint", Rml::PropertyId::Color);
            setGray("mod-icon-background", Rml::PropertyId::BackgroundColor);
        }
        Component::update();
    }

private:
    Rml::Element* mIcon = nullptr;
    bool mInactive = false;
};

class OnlineModsEntry final : public FluentComponent<OnlineModsEntry> {
public:
    explicit OnlineModsEntry(Rml::Element* parent) : FluentComponent{append(parent, "mod-entry")} {
        mRoot->SetClass("online", true);
        mRoot->SetAttribute("focus-key", "online-entry");
        append(mRoot, "mod-icon");
        auto* info = append(mRoot, "mod-info");
        auto* heading = append(info, "header");
        append_text(append(heading, "b"), tr_str("mods.online_mods"));
        append(heading, "update-badge");
        append_text(append(info, "small"), tr_str("mods.online_desc"));
        on_nav_command([this](Rml::Event&, NavCommand command) {
            if (command != NavCommand::Confirm) {
                return false;
            }
            mRoot->DispatchEvent(Rml::EventId::Submit, {});
            return true;
        });
        update();
    }

    void update() override {
        set_mod_update_badge(*this, tr_str("mods.online_mods"));
        Component::update();
    }
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
            image->SetProperty(
                "decorator", fmt::format(R"(image-effects("{}" cover))",
                                 escape(mod_image_source(mod, mod.metadata.bannerPath))));
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
        button->root()->SetClass("overlay", true);
        button->root()->SetClass("danger", item.destructive);
        Button& ref = *button;
        mChildren.emplace_back(std::move(button));
        mButtons.push_back(&ref);
        return ref;
    }

    std::vector<Button*> mButtons;
};

}  // namespace

ModsWindow::ModsWindow()
    : Window{Props{
          .tabBar = false,
          .styleSheets = {"res/rml/mod_common.rcss", "res/rml/mods.rcss"},
      }},
      mContextMenu{*this, mRoot, "mod-entry, mod-header", [this](Rml::Element* target) {
                       const auto id = target->GetAttribute<Rml::String>("mod-id", "");
                       auto* mod = mods::ModLoader::instance().find_mod(id);
                       return mod != nullptr ? mod_actions(*mod, true) :
                                               std::vector<ContextMenu::Item>{};
                   }} {
    mRoot->SetClass("mods", true);

    refresh_snapshot();
    mUpdateGeneration = mods::updates::generation();

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
    mSelection = Selection::Mod;
    mFocusSelectedMod = true;
    refresh_snapshot();
    rebuild_content();
    return true;
}

void ModsWindow::select_online(std::string queueId) {
    mSelection = Selection::Online;
    mSelectedMod = nullptr;
    mSelectedModId.clear();
    mFocusQueueId = std::move(queueId);
    rebuild_content();
}

void show_online_mods(std::string queueId) {
    pop_to_or_push<ModsWindow>([&queueId](ModsWindow& window) { window.select_online(queueId); });
}

bool ModsWindow::focus() {
    if (mSelection == Selection::Online && mOnlineEntry) {
        mOnlineEntry->set_selected(true);
    }
    if (!mFocusQueueId.empty()) {
        mDocument->UpdateDocument();
        auto* row =
            mContentRoot->QuerySelector(fmt::format("queue-row[queue-id=\"{}\"]", mFocusQueueId));
        mFocusQueueId.clear();
        if (row) {
            Rml::ElementList actions;
            row->QuerySelectorAll(actions, "button");
            for (auto* action : actions) {
                if (action->IsVisible() && action->Focus()) {
                    row->ScrollIntoView();
                    return true;
                }
            }
        }
    }
    if (auto* utility = selected_utility(); utility != nullptr) {
        const bool focused = utility->focus();
        utility->set_selected(true);
        return focused;
    }
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
                    case ModAction::Update:
                        enqueue_mod_update(id);
                        break;
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
                                .title = tr_str("mods.could_not_open_folder"),
                                .bodyText = tr_str("mods.could_not_open_folder_body"),
                                .actions = {{tr_str("common.ok"),
                                                [](Modal& modal) { modal.pop(); },
                                                {}}},
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

Component* ModsWindow::selected_utility() const {
    return mSelection == Selection::Online ? mOnlineEntry : nullptr;
}

void ModsWindow::build_online(Pane& pane) {
    mRoot->SetClass("image-header", false);
    mSelection = Selection::Online;
    mSelectedMod = nullptr;
    mSelectedModId.clear();
    pane.root()->RemoveAttribute("mod-id");
    build_online_mods(pane, *this, mExpandedChangelogs);
    mark_current_entry();
}

void ModsWindow::build_content(Rml::Element* content) {
    mEntries.clear();
    mEntryMods.clear();
    mOnlineEntry = nullptr;
    mQueueItems.clear();
    for (const auto& item : mods::queue::items()) {
        mQueueItems.emplace_back(item.id, mods::queue::is_completed(item.state));
    }
    auto& listPane = add_child<Pane>(content, Pane::Type::Controlled);
    listPane.root()->SetClass("mod-list", true);
    auto& detailPane = add_child<Pane>(content, Pane::Type::Uncontrolled);
    detailPane.root()->SetClass("mod-detail", true);
    auto& online = listPane.add_child<OnlineModsEntry>();
    mOnlineEntry = &online;
    listPane.register_control(online, detailPane, [this](Pane& pane) { build_online(pane); });
    const bool hasInstalledMods = !mods::ModLoader::instance().mods().empty();
    if (hasInstalledMods) {
        append(listPane.root(), "mod-list-separator");
    }
    for (auto& trackedMod : mods::ModLoader::instance().mods()) {
        auto& entry = listPane.add_child<ModListEntry>(trackedMod);
        mEntries.push_back(&entry);
        mEntryMods.push_back(&trackedMod);
        listPane.register_control(entry, detailPane, [this, tracked = &trackedMod](Pane& pane) {
            mSelection = Selection::Mod;
            mSelectedMod = tracked;
            mSelectedModId = tracked->metadata.id;
            build_detail(pane, *tracked);
            mark_current_entry();
        });
    }
    if (!hasInstalledMods) {
        listPane.add_text(tr_str("mods.no_mods"));
        mSelection = Selection::Online;
    }
    if (selected_utility()) {
        build_online(detailPane);
        mOnlineEntry->set_selected(true);
    } else if (hasInstalledMods) {
        mSelection = Selection::Mod;
        const auto selected = std::ranges::find_if(
            mEntryMods, [this](const auto* mod) { return mod->metadata.id == mSelectedModId; });
        mSelectedMod = selected != mEntryMods.end() ? *selected : mEntryMods.front();
        mSelectedModId = mSelectedMod->metadata.id;
        build_detail(detailPane, *mSelectedMod);
    } else {
        mSelectedMod = nullptr;
        mSelectedModId.clear();
    }
    mark_current_entry();
}

void ModsWindow::build_detail(Pane& pane, mods::LoadedMod& mod) {
    mRoot->SetClass("image-header", !mod.metadata.bannerPath.empty());
    pane.root()->SetAttribute("mod-id", mod.metadata.id);
    pane.add_child<ModDetailHeader>(mod, mod_actions(mod, false));

    auto* title = append(pane.root(), "mod-title");
    append_text(title, fmt::format("{} ", mod.metadata.name));
    append_text(append(title, "small"), fmt::format("v{}", mod.metadata.version));
    if (mod_uses_network(mod)) {
        append_text(title, "\u00a0");
        auto* badge = append(title, "status-badge");
        badge->SetClass("info", true);
        append_text(badge, tr_str("mods.network_badge"));
    }
    auto* author = append(pane.root(), "small");
    append_text(author,
        fmt::format(fmt::runtime(tr_str("mods.by")), fmt::arg("author", mod.metadata.author)));
    append_text(author, "\u00a0·\u00a0");
    const auto status = mod_status(mod);
    auto* badge = append(author, "status-badge");
    if (status.badgeClass[0] != '\0') {
        badge->SetClass(status.badgeClass, true);
    }
    append_text(badge, status.text);

    if (mod.loadFailed && !mod.failureReason.empty()) {
        auto* row = append(pane.root(), "mod-info-row");
        auto* label = append(row, "b");
        label->SetClass("error", true);
        append_text(label, tr_str("mods.reason"));
        append_text(append(row, "span"), mod.failureReason);
    } else if (mod.suspendedByProvider) {
        std::vector<std::string_view> providers;
        for (const auto& edge : mod.dependencies) {
            if (edge.required && edge.mod != nullptr && !edge.mod->active) {
                providers.push_back(edge.mod->metadata.name);
            }
        }
        auto* row = append(pane.root(), "mod-info-row");
        append_text(append(row, "b"), tr_str("mods.waiting_on"));
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
            fmt::format(fmt::runtime(tr_str("mods.also_restarts")),
                fmt::arg("mods", fmt::join(activeDependents, ", "))));
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
    const std::string action =
        mod.hasBundledCopy ? tr_str("mods.remove_update") : tr_str("mods.uninstall");
    std::string body = mod.hasBundledCopy ? tr_str("mods.revert_body") : tr_str("mods.uninstall_body");
    std::vector<std::string_view> dependents;
    for (const auto& edge : mod.dependents) {
        if (!edge.required || edge.mod == nullptr) {
            continue;
        }
        dependents.push_back(edge.mod->metadata.name);
    }
    if (!dependents.empty()) {
        body = fmt::format("{} {}", body,
            fmt::format(fmt::runtime(tr_str("mods.required_dependents")),
                fmt::arg("mods", fmt::join(dependents, ", "))));
    }

    push(std::make_unique<Modal>(Modal::Props{
        .title = mod.hasBundledCopy ?
            fmt::format(fmt::runtime(tr_str("mods.revert_title")),
                fmt::arg("name", mod.metadata.name)) :
            fmt::format(fmt::runtime(tr_str("mods.uninstall_title")),
                fmt::arg("name", mod.metadata.name)),
        .bodyText = std::move(body),
        .actions =
            {
                ModalAction{tr_str("common.cancel"), [](Modal& modal) { modal.pop(); }, {}},
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
    if (mOnlineEntry) {
        mOnlineEntry->root()->SetClass("current", mSelection == Selection::Online);
    }
    for (size_t i = 0; i < mEntries.size(); ++i) {
        mEntries[i]->root()->SetClass("current", mEntryMods[i] == mSelectedMod);
    }
}

void ModsWindow::update() {
    ZoneScopedN("Mod manager update");
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
    if (mUpdateGeneration != mods::updates::generation()) {
        mUpdateGeneration = mods::updates::generation();
        dirty = true;
    }
    std::vector<std::pair<std::string, bool>> queueItems;
    for (const auto& item : mods::queue::items()) {
        queueItems.emplace_back(item.id, mods::queue::is_completed(item.state));
    }
    dirty |= queueItems != mQueueItems;
    if (dirty) {
        mContextMenu.dismiss();
        ZoneScopedN("Mod manager rebuild");
        const auto previousModId = mSelectedModId;
        const auto desaturation = Rml::StyleSheetSpecification::GetPropertyId("image-desaturation");
        std::optional<Rml::Property> previousDesaturation;
        if (auto* image = mContentRoot->QuerySelector("mod-header-image")) {
            previousDesaturation = *image->GetProperty(desaturation);
        }
        auto* list = mContentRoot->QuerySelector("pane.mod-list");
        const float listScrollTop = list != nullptr ? list->GetScrollTop() : 0.0f;
        auto* focused = mDocument != nullptr ? mDocument->GetFocusLeafNode() : nullptr;
        std::string focusKey;
        for (auto* node = focused; node != nullptr && node != mContentRoot;
            node = node->GetParentNode())
        {
            if (node->HasAttribute("focus-key")) {
                focusKey = node->GetAttribute<Rml::String>("focus-key", "");
                break;
            }
        }
        auto* detail = mContentRoot->QuerySelector("pane.mod-detail");
        const float detailScrollTop = detail ? detail->GetScrollTop() : 0.0f;
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
            auto* restored = focusKey.empty() ? nullptr :
                                                mContentRoot->QuerySelector(
                                                    fmt::format("[focus-key=\"{}\"]", focusKey));
            if (restored && restored->IsVisible() && !restored->IsPseudoClassSet("disabled") &&
                restored->Focus(true))
            {
            } else if (auto* utility = selected_utility(); utility != nullptr) {
                auto* fallback = mContentRoot->QuerySelector("queue-row button");
                if (!fallback || !fallback->IsVisible()) {
                    fallback = mContentRoot->QuerySelector("[focus-key=completed-clear]");
                }
                if (!fallback || !fallback->IsVisible()) {
                    fallback = mContentRoot->QuerySelector("[focus-key=online-browse]");
                }
                if (!focusKey.empty() && focusKey != "online-entry" && fallback) {
                    fallback->Focus(true);
                } else {
                    utility->root()->Focus(true);
                }
            } else {
                for (size_t i = 0; i < mEntryMods.size(); ++i) {
                    if (mEntryMods[i] == mSelectedMod) {
                        mEntries[i]->root()->Focus(true);
                        break;
                    }
                }
            }
        }
        if (previousDesaturation && previousModId == mSelectedModId) {
            mDocument->UpdateDocument();
            if (auto* image = mContentRoot->QuerySelector("mod-header-image")) {
                const auto target = *image->GetProperty(desaturation);
                if (*previousDesaturation != target) {
                    image->SetProperty(desaturation, *previousDesaturation);
                    image->Animate(desaturation, target, 0.2f,
                        Rml::Tween{Rml::Tween::Cubic, Rml::Tween::InOut}, 1, false);
                }
            }
        }
        if (auto* refreshedDetail = mContentRoot->QuerySelector("pane.mod-detail")) {
            refreshedDetail->SetScrollTop(detailScrollTop);
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
