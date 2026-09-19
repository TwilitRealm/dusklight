#include "mod_updates.hpp"

#include "button.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/updates.hpp"
#include "format.hpp"
#include "icon_button.hpp"
#include "mod_texture_provider.hpp"
#include "nav_group.hpp"
#include "package_row.hpp"
#include "pane.hpp"

#include <algorithm>
#include <borealis/update.hpp>
#include <fmt/format.h>

namespace dusk::ui {
namespace {

class UpdateHeader final : public NavGroup {
public:
    explicit UpdateHeader(Rml::Element* parent)
        : NavGroup{append(parent, "updates-header"), {.layout = Layout::Horizontal}} {
        auto* heading = append(mRoot, "updates-heading");
        append_text(append(heading, "h2"), "Updates");
        mStatus = append(mRoot, "p");
        auto* actions = append(mRoot, "updates-actions");
        auto& all = add_existing_item<Button>(actions, "Update all");
        all.root()->SetClass("update-all", true);
        all.root()->SetAttribute("focus-key", "updates-all");
        all.on_pressed([] {
            const auto result = mods::updates::enqueue_all();
            if (result.skipped) {
                push_toast({.type = "warning",
                    .title = "Some updates could not be queued",
                    .content =
                        fmt::format("{} skipped. {}", result.skipped, escape(result.error))});
            }
        });
        mAll = &all;
        auto& check = add_existing_item<IconButton>(
            heading, IconButton::Props{.icon = "refresh", .label = "Check for updates"});
        check.root()->SetAttribute("focus-key", "updates-check");
        check.on_pressed([] { mods::updates::request_check(); });
        mCheck = &check;
        update();
    }

    void update() override {
        const auto count = mods::updates::actionable_count();
        set_text_content(mStatus, mods::updates::status_text());
        const auto label = count ? fmt::format("Update all ({}) · {}", count,
                                       format_bytes(mods::updates::download_size())) :
                                   "Update all";
        if (mAllLabel != label) {
            ui::clear_children(mAll->root());
            auto* icon = append(mAll->root(), "icon");
            icon->SetAttribute("aria-hidden", "true");
            append_text(icon, material_icon("file_download"));
            append_text(append(mAll->root(), "span"), label);
            mAllLabel = label;
        }
        mAll->set_disabled(count == 0);
        set_display(mAll->root(), count ? Rml::Style::Display::Flex : Rml::Style::Display::None);
        mCheck->set_disabled(mods::updates::state() == mods::updates::State::Checking);
        Component::update();
    }

private:
    Rml::Element* mStatus = nullptr;
    Button* mAll = nullptr;
    Button* mCheck = nullptr;
    Rml::String mAllLabel;
};

class UpdateCard final : public NavGroup {
public:
    UpdateCard(Rml::Element* parent, std::string id, std::unordered_set<std::string>& expanded)
        : NavGroup{append(parent, "update-card"), {.layout = Layout::Horizontal}},
          mId{std::move(id)} {
        auto& row = add_child<PackageRow>();
        mRow = &row;
        auto* actions = row.actions_root();
        auto& action = add_existing_item<IconButton>(
            actions, IconButton::Props{.icon = "file_download", .label = "Update"});
        action.root()->SetClass("compact", true);
        action.root()->SetAttribute("focus-key", "mod-action-" + mId);
        action.on_pressed([this] { enqueue_mod_update(mId); });
        mAction = &action;
        const auto* entry = mods::updates::find(mId);
        if (entry && entry->result.target && !entry->result.target->changelogHtml.empty()) {
            const bool isExpanded = expanded.contains(mId);
            auto& changelog = add_existing_item<IconButton>(
                actions, IconButton::Props{.icon = "description",
                             .label = isExpanded ? "Hide changelog" : "Show changelog"});
            changelog.root()->SetClass("compact", true);
            changelog.root()->SetAttribute("focus-key", "changelog-" + mId);
            mChangelog = append(mRoot, "update-changelog");
            mChangelog->SetInnerRML(entry->result.target->changelogHtml);
            mChangelog->SetProperty("display", isExpanded ? "block" : "none");
            changelog.on_pressed([this, &changelog, &expanded] {
                const bool isExpanded = !expanded.contains(mId);
                if (isExpanded) {
                    expanded.insert(mId);
                } else {
                    expanded.erase(mId);
                }
                mChangelog->SetProperty("display", isExpanded ? "block" : "none");
                changelog.set_label(isExpanded ? "Hide changelog" : "Show changelog");
                changelog.set_selected(isExpanded);
            });
            changelog.set_selected(isExpanded);
        }
        update();
    }

    void update() override {
        const auto* entry = mods::updates::find(mId);
        const auto* local = mods::ModLoader::instance().find_mod(mId);
        if (!entry || !local) {
            return;
        }
        std::string detail = entry->reason;
        std::string stateClass = entry->actionable ? "" : "unavailable";
        if (entry->result.target) {
            if (detail.empty()) {
                detail = format_bytes(entry->result.target->download.size);
            }
            if (entry->result.target->version != entry->result.latestVersion &&
                !entry->result.blockers.empty())
            {
                detail += fmt::format(" · Latest {}: {}", entry->result.latestVersion,
                    entry->result.blockers.front());
            }
        }
        if (entry->result.yankedInstalled) {
            detail += " · The installed release was withdrawn.";
        }
        const auto& version =
            entry->result.target ? entry->result.target->version : entry->result.latestVersion;
        mRow->set_package(local->metadata.name,
            fmt::format("{} → {}", local->metadata.version, version), "", detail, stateClass);
        mRow->set_icon(local->metadata.iconPath.empty() ?
                           "" :
                           mod_image_source(*local, local->metadata.iconPath));
        mAction->set_label(entry->actionable ? "Update" : "Unavailable");
        mAction->set_disabled(!entry->actionable);
        Component::update();
    }

private:
    std::string mId;
    PackageRow* mRow = nullptr;
    IconButton* mAction = nullptr;
    Rml::Element* mChangelog = nullptr;
};

}  // namespace

void enqueue_mod_update(std::string_view id) {
    const auto result = mods::updates::enqueue_update(id);
    if (!result.error.empty()) {
        push_toast({.type = "warning",
            .title = "Could not queue update",
            .content = escape(result.error)});
    }
}

void set_mod_update_badge(Component& component, std::string_view label) {
    auto* badge = component.root()->QuerySelector("update-badge");
    if (badge == nullptr) {
        auto* root = component.root();
        if (root->GetTagName() == "button" || root->GetTagName() == "tab") {
            root->SetClass("mod-update-link", true);
            auto labelElement = root->GetOwnerDocument()->CreateElement("update-label");
            while (root->GetNumChildren() > 0) {
                labelElement->AppendChild(root->RemoveChild(root->GetFirstChild()));
            }
            root->AppendChild(std::move(labelElement));
        }
        badge = append(component.root(), "update-badge");
    }
    const auto count = mods::updates::actionable_count();
    if (badge->GetAttribute<int>("count", -1) == static_cast<int>(count)) {
        return;
    }
    badge->SetAttribute("count", static_cast<int>(count));
    badge->SetProperty("display", count ? "inline-block" : "none");
    set_text_content(badge, fmt::format("{}", count));
    const auto description =
        count ? fmt::format("{} · {} mod updates available", label, count) : std::string{label};
    component.root()->SetAttribute("aria-label", description);
    component.root()->SetAttribute("title", description);
}

void build_mod_updates(Pane& pane, std::unordered_set<std::string>& expanded) {
    pane.add_child<UpdateHeader>();
    for (bool blocked : {false, true}) {
        bool heading = false;
        for (const auto& entry : mods::updates::entries()) {
            const auto* mod = mods::ModLoader::instance().find_mod(entry.result.id);
            if (!mod || !entry.queueKey.empty() ||
                mod->metadata.version != entry.result.installedVersion)
            {
                continue;
            }
            const auto latest = borealis::update::parse_version(entry.result.latestVersion);
            const auto current = borealis::update::parse_version(mod->metadata.version);
            const bool newer =
                latest && current && borealis::update::compare_version(*latest, *current) > 0;
            if (!newer && !entry.result.yankedInstalled) {
                continue;
            }
            if (blocked != !entry.actionable) {
                continue;
            }
            if (blocked && !heading) {
                pane.add_section("Unavailable updates");
                heading = true;
            }
            pane.add_child<UpdateCard>(entry.result.id, expanded);
        }
    }
    for (const auto& mod : mods::ModLoader::instance().mods()) {
        if (!borealis::update::parse_version(mod.metadata.version)) {
            pane.add_text(fmt::format(
                "{}: version '{}' cannot be compared.", mod.metadata.name, mod.metadata.version));
        }
    }
}

}  // namespace dusk::ui
