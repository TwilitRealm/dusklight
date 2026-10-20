#pragma once

#include "dusk/config_var.hpp"
#include "ui.hpp"

#include <aurora/binding.hpp>
#include <borealis/ui/pane.hpp>
#include <borealis/ui/window.hpp>

#include <pad.h>

namespace dusk::ui {

class ControllerConfigWindow : public Window {
public:
    ControllerConfigWindow();
    ~ControllerConfigWindow() override;

    void hide(bool close) override;

private:
    enum class Page {
        Controller,
        Buttons,
        Triggers,
        Sticks,
        Rumble,
        Actions,
    };

    void build_port_tab(Rml::Element* content, int port);
    void render_page(Pane& pane, int port, Page page);
    void refresh_controller_page();
    void start_capture();
    void handle_captured_input(const aurora::binding::PhysicalInput& input);
    void finish_pending_binding(int completedPort);
    void unmap_pending_binding();
    bool capture_active() const;
    Rml::String pending_button_label() const;
    Rml::String pending_axis_label() const;
    void cancel_pending_binding();
    void finish_pending_key_binding();
    Rml::String pending_key_label() const;
    void stop_rumble_test();

    Page mPage = Page::Controller;
    Pane* mRightPane = nullptr;
    int mActivePort = 0;
    int mPendingPort = -1;
    PADButtonMapping* mPendingButtonMapping = nullptr;
    PADAxisMapping* mPendingAxisMapping = nullptr;
    int mPendingKeyButton = -1;
    int mPendingKeyAxis = -1;
    bool mRumbleTestActive = false;
    int mRumbleTestPort = -1;
    ActionBindConfigVar* mPendingActionBinding = nullptr;
};

Rml::String native_button_name(SDL_Gamepad* gamepad, u32 buttonUntyped);

}  // namespace dusk::ui
