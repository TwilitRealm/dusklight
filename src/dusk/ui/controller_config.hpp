#pragma once

#include "window.hpp"
#include "dusk/config_var.hpp"

#include <pad.h>
#include <vector>
#include <variant>

namespace dusk::ui {

class ControllerConfigWindow : public Window {
public:
    ControllerConfigWindow();

    void update() override;
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

    //BUILT-IN PRESETS

    struct DevicePreset {
        Rml::String name;
        std::vector<std::pair<PADButton, u32>> buttonMappings;
        
        // Gamepad save PADSignedNativeAxis; Keyboard Save Scancode (s32)
        std::vector<std::pair<PADAxis, std::variant<PADSignedNativeAxis, s32>>> axisMappings;
    };

    const std::vector<DevicePreset> kDevicePresets = {
        {
            "Nintendo Switch Pro Controller",
            // Buttons
            {
                { PAD_BUTTON_A, SDL_GAMEPAD_BUTTON_SOUTH },             // B -> A Gamecube =)
                { PAD_BUTTON_B, SDL_GAMEPAD_BUTTON_WEST },              // Y -> B Gamecube
                { PAD_BUTTON_X, SDL_GAMEPAD_BUTTON_EAST },              // A -> X Gamecube
                { PAD_BUTTON_Y, SDL_GAMEPAD_BUTTON_NORTH },             // X -> Y Gamecube
                { PAD_BUTTON_START, SDL_GAMEPAD_BUTTON_START },         // + -> Start
                { PAD_TRIGGER_Z, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },   // R -> Z

                // D-Pad
                { PAD_BUTTON_UP, SDL_GAMEPAD_BUTTON_DPAD_UP },
                { PAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN },
                { PAD_BUTTON_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT },
                { PAD_BUTTON_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT }
            },
            // Axis
            {
                // Control Stick
                { PAD_AXIS_LEFT_X_POS, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_POSITIVE } },
                { PAD_AXIS_LEFT_X_NEG, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_NEGATIVE } },
                { PAD_AXIS_LEFT_Y_POS, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_POSITIVE } },
                { PAD_AXIS_LEFT_Y_NEG, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_NEGATIVE } },
                // C Stick
                { PAD_AXIS_RIGHT_X_POS, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_POSITIVE } },
                { PAD_AXIS_RIGHT_X_NEG, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_NEGATIVE } },
                { PAD_AXIS_RIGHT_Y_POS, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_POSITIVE } },
                { PAD_AXIS_RIGHT_Y_NEG, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_NEGATIVE } },
                // Triggers
                { PAD_AXIS_TRIGGER_L, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_SIGN_POSITIVE } },
                { PAD_AXIS_TRIGGER_R, PADSignedNativeAxis{ SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_SIGN_POSITIVE } }
            }
        },
        {
            // This is what i think is a good layout for keyboard
            "Keyboard",

            {
                { PAD_BUTTON_A, SDL_SCANCODE_J },
                { PAD_BUTTON_B, SDL_SCANCODE_K },
                { PAD_BUTTON_X, SDL_SCANCODE_L },
                { PAD_BUTTON_Y, SDL_SCANCODE_I },
                { PAD_BUTTON_START, SDL_SCANCODE_SPACE },
                { PAD_TRIGGER_Z, SDL_SCANCODE_LSHIFT },
                
                // D-Pad
                { PAD_BUTTON_UP, SDL_SCANCODE_T },
                { PAD_BUTTON_DOWN, SDL_SCANCODE_G },
                { PAD_BUTTON_LEFT, SDL_SCANCODE_F },
                { PAD_BUTTON_RIGHT, SDL_SCANCODE_H }
            },
            // Axis to Keyboard Buttons =)
            {
                // Control Stick
                { PAD_AXIS_LEFT_X_NEG, static_cast<s32>(SDL_SCANCODE_A) },
                { PAD_AXIS_LEFT_X_POS, static_cast<s32>(SDL_SCANCODE_D) },
                { PAD_AXIS_LEFT_Y_NEG, static_cast<s32>(SDL_SCANCODE_W) },
                { PAD_AXIS_LEFT_Y_POS, static_cast<s32>(SDL_SCANCODE_S) },
                // C Stick
                { PAD_AXIS_RIGHT_X_NEG, static_cast<s32>(SDL_SCANCODE_LEFT) },
                { PAD_AXIS_RIGHT_X_POS, static_cast<s32>(SDL_SCANCODE_RIGHT) },
                { PAD_AXIS_RIGHT_Y_NEG, static_cast<s32>(SDL_SCANCODE_UP) },
                { PAD_AXIS_RIGHT_Y_POS, static_cast<s32>(SDL_SCANCODE_DOWN) },
                // Triggers
                { PAD_AXIS_TRIGGER_L, static_cast<s32>(SDL_SCANCODE_Q) },
                { PAD_AXIS_TRIGGER_R, static_cast<s32>(SDL_SCANCODE_E) }
            }
        }
    };

    void apply_preset(int port, const DevicePreset& preset);
    void build_port_tab(Rml::Element* content, int port);
    void render_page(class Pane& pane, int port, Page page);
    void refresh_controller_page();
    void poll_pending_binding();
    void finish_pending_binding(int completedPort);
    void unmap_pending_binding();
    bool capture_active() const;
    bool pending_input_neutral() const;
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
    bool mPendingBindingArmed = false;
    bool mSuppressNavigationUntilNeutral = false;
    int mSuppressNavigationPort = -1;
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
