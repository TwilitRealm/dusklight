#include "dusk/action_bindings.h"

#include "dusk/settings.h"
#include "dusk/ui/ui.hpp"

#include <aurora/input.hpp>
#include <aurora/pad.hpp>

#include <optional>

namespace dusk {
namespace {

using aurora::binding::ControlId;
using aurora::binding::PhysicalInput;

constexpr size_t kActionCount = static_cast<size_t>(ActionBinds::COUNT);

constexpr std::array<const char*, kActionCount> kControlNames{
    "dusklight.first_person_camera",
    "dusklight.call_midna",
    "dusklight.open_map",
    "dusklight.toggle_minimap",
    "dusklight.open_menu",
    "dusklight.turbo",
};

const std::array<ControlId, kActionCount>& action_controls() {
    static const auto sControls = [] {
        std::array<ControlId, kActionCount> controls{};
        for (size_t i = 0; i < kActionCount; ++i) {
            controls[i] = aurora::binding::register_control({
                .name = kControlNames[i],
                .kind = aurora::binding::ControlKind::Button,
            });
        }
        return controls;
    }();
    return sControls;
}

std::optional<size_t> action_index(ControlId control) {
    const auto& controls = action_controls();
    for (size_t i = 0; i < controls.size(); ++i) {
        if (controls[i] == control) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<PhysicalInput> physical_input(int button, bool keyboard) {
    if (keyboard) {
        if (button >= PAD_KEY_MOUSE_X2 && button <= PAD_KEY_MOUSE_LEFT) {
            return PhysicalInput{
                .control = PhysicalInput::MouseButton{.button = static_cast<uint8_t>(-button - 1)},
            };
        }
        if (button >= 0 && button < SDL_SCANCODE_COUNT) {
            return PhysicalInput{
                .control = PhysicalInput::Key{.scancode = static_cast<SDL_Scancode>(button)},
            };
        }
        return std::nullopt;
    }
    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
        return PhysicalInput{
            .control =
                PhysicalInput::GamepadButton{.button = static_cast<SDL_GamepadButton>(button)},
        };
    }
    return std::nullopt;
}

struct PortActions {
    aurora::binding::State state;
    // Presses since the last read, so a press and release between reads isn't lost.
    std::array<bool, kActionCount> latched{};
    std::array<int, kActionCount> published{};
    bool keyboardActive = false;
    bool synced = false;
};

std::array<PortActions, PAD_CHANMAX> sPorts;
aurora::input::LayerId sLayer = aurora::input::kInvalidLayerId;

std::array<std::array<ActionBindPressData, kActionCount>, PAD_CHANMAX> actionPressData{};

struct VirtualActionBindData {
    bool pressed = false;
    bool available = false;
};

std::array<std::array<VirtualActionBindData, kActionCount>, PAD_CHANMAX> virtualActionData{};

PortActions& synced_port(u32 port) {
    auto& actions = sPorts[port];
    if (auto set = aurora::pad::binding_set(port); set != actions.state.bindings()) {
        (void)actions.state.set_bindings(std::move(set));
    }
    return actions;
}

aurora::input::EventResult on_event(const aurora::input::InputEvent& event, void*) {
    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        auto& actions = synced_port(port);
        const auto& set = actions.state.bindings();
        if (set == nullptr || !set->references(event.source.id)) {
            continue;
        }
        for (const auto& change : actions.state.process(event).changes) {
            if (change.reason != aurora::binding::ControlChange::Reason::Input ||
                change.value < 0.5f || change.previousValue >= 0.5f)
            {
                continue;
            }
            if (const auto index = action_index(change.control)) {
                actions.latched[*index] = true;
            }
        }
    }
    return aurora::input::EventResult::Pass;
}

}  // namespace

ActionBindsMap& getActionBinds() {
    static ActionBindsMap actionBinds = {
        {ActionBinds::FIRST_PERSON_CAMERA, {&getSettings().actionBindings.firstPersonCamera, "First Person Camera"}},
        {ActionBinds::CALL_MIDNA,          {&getSettings().actionBindings.callMidna,         "Call Midna"}},
        {ActionBinds::OPEN_MAP_SCREEN,     {&getSettings().actionBindings.openMapScreen,     "Open Map Screen"}},
        {ActionBinds::TOGGLE_MINIMAP,      {&getSettings().actionBindings.toggleMinimap,     "Toggle Minimap"}},
        {ActionBinds::OPEN_DUSKLIGHT_MENU, {&getSettings().actionBindings.openDusklightMenu, "Open Dusklight Menu"}},
        {ActionBinds::TURBO_SPEED_BUTTON,  {&getSettings().actionBindings.turboSpeedButton,  "Turbo Speed Button"}},
    };
    return actionBinds;
}

bool isActionBound(ActionBinds action, u32 port) {
    auto& actionBinds = getActionBinds();
    // Check to make sure action is properly bound
    if (!actionBinds.contains(action)) {
        return false;
    }

    if (port < PAD_CHANMAX && virtualActionData[port][static_cast<int>(action)].available) {
        return true;
    }

    return getActionBindButton(action, port) != PAD_NATIVE_BUTTON_INVALID;
}

bool isActionBoundAnyPort(ActionBinds action) {
    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        if (isActionBound(action, port)) {
            return true;
        }
    }
    return false;
}

ControlId getActionControl(ActionBinds action) {
    return action_controls()[static_cast<size_t>(action)];
}

void syncActionBindings() {
    if (sLayer == aurora::input::kInvalidLayerId) {
        sLayer = aurora::input::register_layer({
            .label = "dusklight.actions",
            .priority = aurora::input::kGameLayerPriority,
            .onEvent = on_event,
        });
    }

    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        u32 count = 0;
        const bool keyboard = PADGetKeyButtonBindings(port, &count) != nullptr;
        std::array<int, kActionCount> buttons{};
        for (size_t i = 0; i < kActionCount; ++i) {
            buttons[i] = getActionBindButton(static_cast<ActionBinds>(i), port);
        }

        auto& actions = sPorts[port];
        if (actions.synced && actions.keyboardActive == keyboard && actions.published == buttons) {
            continue;
        }
        actions.synced = true;
        actions.keyboardActive = keyboard;
        actions.published = buttons;

        std::vector<aurora::binding::Binding> bindings;
        for (size_t i = 0; i < kActionCount; ++i) {
            if (const auto input = physical_input(buttons[i], keyboard)) {
                bindings.push_back({.input = *input, .target = action_controls()[i]});
            }
        }
        aurora::pad::set_action_bindings(port, std::move(bindings));
    }
}

void updateActionBindings() {
    syncActionBindings();

    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        auto& actions = synced_port(port);
        for (size_t i = 0; i < kActionCount; ++i) {
            auto& pressData = actionPressData[port][i];
            const auto& virtualAction = virtualActionData[port][i];
            pressData.pressedPrevFrame = pressData.pressedCurFrame;
            pressData.pressedCurFrame =
                actions.state.value(action_controls()[i]) >= 0.5f || actions.latched[i] ||
                (virtualAction.available && virtualAction.pressed && !ui::any_document_visible());
        }
        actions.latched = {};
    }
}

void setVirtualActionBind(ActionBinds action, u32 port, bool pressed, bool available) {
    if (port >= PAD_CHANMAX) {
        return;
    }
    virtualActionData[port][static_cast<int>(action)] = {
        .pressed = pressed,
        .available = available,
    };
}

void clearVirtualActionBind(ActionBinds action, u32 port) {
    if (port >= PAD_CHANMAX) {
        return;
    }
    virtualActionData[port][static_cast<int>(action)] = {};
}

void clearAllVirtualActionBinds() {
    virtualActionData = {};
}

bool getActionBindTrig(ActionBinds action, u32 port) {
    return actionPressData[port][static_cast<int>(action)].pressedCurFrame &&
          !actionPressData[port][static_cast<int>(action)].pressedPrevFrame;
}

bool getActionBindHold(ActionBinds action, u32 port) {
    return actionPressData[port][static_cast<int>(action)].pressedCurFrame &&
           actionPressData[port][static_cast<int>(action)].pressedPrevFrame;
}

bool getActionBindHoldAnyPort(ActionBinds action) {
    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        if (getActionBindHold(action, port)) {
            return true;
        }
    }
    return false;
}

int getActionBindButton(ActionBinds action, u32 port) {
    return (*getActionBinds()[action].configVars)[port];
}
}
