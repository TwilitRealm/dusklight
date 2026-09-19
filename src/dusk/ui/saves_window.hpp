#pragma once

#include "window.hpp"

#include <cstdint>
#include <string>

namespace dusk::ui {

class Pane;

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
