#pragma once
#include "ui.hpp"

#include <borealis/ui/window.hpp>

namespace dusk::ui {

class SettingsWindow : public Window {
public:
    SettingsWindow(bool prelaunch = false);

    void update() override;
    void hide(bool close) override;

protected:
    bool mPrelaunch;
};

}  // namespace dusk::ui