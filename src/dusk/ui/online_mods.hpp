#pragma once

#include "ui.hpp"

#include <string>
#include <unordered_set>

namespace borealis::ui {
class Document;
class Pane;
}  // namespace borealis::ui

namespace dusk::ui {

void build_online_mods(
    Pane& pane, Document& document, std::unordered_set<std::string>& expandedChangelogs);
}  // namespace dusk::ui
