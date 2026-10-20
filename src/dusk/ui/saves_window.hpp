#pragma once

#include "ui.hpp"

#include <borealis/ui/window.hpp>

#include <cstdint>
#include <string>

namespace borealis::ui {
class Pane;
}  // namespace borealis::ui

namespace dusk::ui {

class SavesWindow final : public Window {
public:
    SavesWindow();
    void update() override;

private:
    void build_content(Rml::Element* content);

    std::string mSaveName;
    uint64_t mGeneration = 0;
};

void add_save_files_control(Pane& leftPane, Pane& rightPane);
void import_save_location(std::string location);

}  // namespace dusk::ui
