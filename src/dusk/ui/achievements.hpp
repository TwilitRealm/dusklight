#pragma once

#include "dusk/achievements.h"
#include "ui.hpp"

#include <borealis/ui/window.hpp>

#include <vector>

namespace dusk::ui {

class AchievementsWindow : public Window {
public:
    AchievementsWindow();
    void update() override;

private:
    void updateTotal();
    std::vector<Achievement> mSnapshot;
    Rml::Element* mTotalEl = nullptr;
};

}  // namespace dusk::ui
