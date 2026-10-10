#pragma once

#include "ui.hpp"

#include <borealis/ui/menu_bar.hpp>

namespace dusk::ui {

class MenuBar : public borealis::ui::MenuBar {
public:
    MenuBar();

    void update() override;

protected:
    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override;
    void build_tabs() override;

private:
    Button* mModsButton = nullptr;
};

}  // namespace dusk::ui
