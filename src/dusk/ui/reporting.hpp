#pragma once

#if BOREALIS_HAS_SENTRY

#include "ui.hpp"

#include <borealis/ui/component.hpp>
#include <borealis/ui/window.hpp>

#include <memory>
#include <vector>

namespace dusk::ui {

class CrashReportWindow : public WindowSmall {
public:
    CrashReportWindow();

    bool focus() override;

protected:
    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override;

private:
    std::vector<std::unique_ptr<Component>> mButtons;
};

}  // namespace dusk::ui

#endif
