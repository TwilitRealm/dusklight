#pragma once

#include "ui.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "dusk/mod_loader.hpp"

#include <borealis/ui/context_menu.hpp>
#include <borealis/ui/window.hpp>

namespace borealis::ui {
class Pane;
}  // namespace borealis::ui

namespace dusk::ui {

class ModsWindow : public Window {
public:
    ModsWindow();
    void hide(bool close) override;
    void update() override;
    bool focus() override;
    bool select_mod(std::string_view id);
    void select_online(std::string queueId = {});

private:
    enum class Selection { Online, Mod };

    struct ModSnapshot {
        mods::LoadedMod* mod = nullptr;
        bool active = false;
        bool loadFailed = false;
        bool enabled = false;
        bool suspended = false;
        u32 cacheGeneration = 0;
    };

    void build_content(Rml::Element* content);
    void build_online(Pane& pane);
    Component* selected_utility() const;
    void build_detail(Pane& pane, mods::LoadedMod& mod);
    void confirm_uninstall(const mods::LoadedMod& mod);
    std::vector<ContextMenu::Item> mod_actions(const mods::LoadedMod& mod, bool contextMenu);
    void refresh_snapshot();
    void mark_current_entry();

    std::vector<ModSnapshot> mSnapshot;
    std::vector<Component*> mEntries;
    std::vector<mods::LoadedMod*> mEntryMods;
    Component* mOnlineEntry = nullptr;
    mods::LoadedMod* mSelectedMod = nullptr;
    std::string mSelectedModId;
    uint64_t mLoaderGeneration = 0;
    std::vector<std::pair<std::string, bool>> mQueueItems;
    std::string mFocusQueueId;
    Selection mSelection = Selection::Online;
    uint64_t mUpdateGeneration = 0;
    std::unordered_set<std::string> mExpandedChangelogs;
    bool mFocusSelectedMod = false;
    ContextMenu::Binding mContextMenu;
};

void show_online_mods(std::string queueId = {});

}  // namespace dusk::ui
