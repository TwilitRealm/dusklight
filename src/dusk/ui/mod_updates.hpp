#pragma once

#include "ui.hpp"

#include <string>
#include <string_view>
#include <unordered_set>

namespace borealis::ui {
class Component;
class Pane;
}  // namespace borealis::ui

namespace dusk::ui {

void set_mod_update_badge(Component& component, std::string_view label = "Mods");
void build_mod_updates(Pane& pane, std::unordered_set<std::string>& expanded);
void enqueue_mod_update(std::string_view id);
}  // namespace dusk::ui
