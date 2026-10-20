#pragma once

#include "dusk/mod_loader.hpp"

#include <functional>
#include <string>
#include <vector>

namespace borealis::ui {
class Pane;
}  // namespace borealis::ui

namespace dusk::mods::svc {

void ui_build_mods_panels(LoadedMod& mod, borealis::ui::Pane& pane);
void ui_update_mods_panels(LoadedMod& mod);

struct ModMenuTabEntry {
    std::string label;
    std::function<void()> onSelected;
};

std::vector<ModMenuTabEntry> ui_mod_menu_tabs();

}  // namespace dusk::mods::svc
