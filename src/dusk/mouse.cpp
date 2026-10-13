#include "dusk/mouse.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/menu_pointer.h"
#include "dusk/settings.h"

#include <aurora/input.hpp>
#include <imgui.h>

#include <utility>

namespace dusk::mouse {
namespace {
using aurora::input::PointerMode;

constexpr float kMousePixelToRad = 0.0025f;

float s_aim_yaw_rad = 0.0f;
float s_aim_pitch_rad = 0.0f;
float s_camera_yaw_rad = 0.0f;
float s_camera_pitch_rad = 0.0f;
SDL_FPoint s_pending_delta{};
bool s_relative = false;
aurora::input::LayerId s_layer = aurora::input::kInvalidLayerId;

void reset_deltas() {
    s_aim_yaw_rad = s_aim_pitch_rad = 0.0f;
    s_camera_yaw_rad = s_camera_pitch_rad = 0.0f;
}

bool query_mouse_aim_context() {
    return getSettings().game.enableMouseAim.getValue() && dCamera_c::isAimActive();
}

bool want_mouse_capture() {
    return getSettings().game.enableMouseCamera.getValue() || query_mouse_aim_context();
}

bool mouse_input_enabled() {
    const auto& game = getSettings().game;
    return game.enableMouseAim.getValue() || game.enableMouseCamera.getValue();
}

bool imgui_windows_visible() {
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().MetricsRenderWindows > 0;
}

PointerMode layer_pointer_mode(void*) {
    // TODO: move into aurora
    if (imgui_windows_visible()) {
        return PointerMode::Visible;
    }
    if (menu_pointer::active()) {
        if (menu_pointer::enabled()) {
            return PointerMode::Visible;
        }
    } else if (want_mouse_capture()) {
        return PointerMode::Relative;
    }
    return mouse_input_enabled() ? PointerMode::Hidden : PointerMode::AutoHide;
}

aurora::input::EventResult layer_event(const aurora::input::InputEvent& event, void*) {
    using aurora::input::InputEvent;
    if (event.source.kind != aurora::input::InputSource::Kind::Mouse) {
        return aurora::input::EventResult::Pass;
    }
    if (const auto* pointer = event.payload.get_if<InputEvent::PointerChanged>()) {
        if (s_relative && pointer->phase == InputEvent::PointerChanged::Phase::Move) {
            s_pending_delta.x += pointer->delta.x;
            s_pending_delta.y += pointer->delta.y;
        }
    } else if (event.payload.is<InputEvent::Cancelled>()) {
        s_pending_delta = {};
    }
    return aurora::input::EventResult::Pass;
}

void accumulate_deltas(float mx_rel, float my_rel, bool camera_active, bool aim_active) {
    const auto& game = getSettings().game;
    const bool mirror_mode = game.enableMirrorMode.getValue();
    const bool invert_y = game.invertMouseY.getValue();

    if (aim_active) {
        const float aimSens = game.mouseAimSensitivity.getValue();
        s_aim_yaw_rad = -mx_rel * kMousePixelToRad * aimSens;
        s_aim_pitch_rad = my_rel * kMousePixelToRad * aimSens;
        s_aim_yaw_rad = mirror_mode ? -s_aim_yaw_rad : s_aim_yaw_rad;
        s_aim_pitch_rad = invert_y ? -s_aim_pitch_rad : s_aim_pitch_rad;
    } else {
        s_aim_yaw_rad = s_aim_pitch_rad = 0.0f;
    }

    if (camera_active) {
        const float camSens = game.mouseCameraSensitivity.getValue();
        s_camera_yaw_rad = -mx_rel * kMousePixelToRad * camSens;
        s_camera_pitch_rad = -my_rel * kMousePixelToRad * camSens;
        s_camera_yaw_rad = mirror_mode ? -s_camera_yaw_rad : s_camera_yaw_rad;
        s_camera_pitch_rad = invert_y ? -s_camera_pitch_rad : s_camera_pitch_rad;
    } else {
        s_camera_yaw_rad = s_camera_pitch_rad = 0.0f;
    }
}
}  // namespace

void read() {
    if (s_layer == aurora::input::kInvalidLayerId) {
        s_layer = aurora::input::register_layer({
            .label = "dusklight.mouse",
            .priority = aurora::input::kGameLayerPriority,
            .onEvent = layer_event,
            .pointerMode = layer_pointer_mode,
        });
    }

    const bool wasRelative =
        std::exchange(s_relative, aurora::input::pointer_mode() == PointerMode::Relative);
    const SDL_FPoint delta = std::exchange(s_pending_delta, {});
    if (!wasRelative || !s_relative) {
        reset_deltas();
        return;
    }

    const bool aim_active = query_mouse_aim_context();
    const bool camera_active = getSettings().game.enableMouseCamera;
    accumulate_deltas(delta.x, delta.y, camera_active, aim_active);
}

void get_aim_deltas(float& out_yaw, float& out_pitch) {
    out_yaw = s_aim_yaw_rad;
    out_pitch = s_aim_pitch_rad;
}

void get_camera_deltas(float& out_yaw, float& out_pitch) {
    out_yaw = 0.0f;
    out_pitch = 0.0f;

    if (!getSettings().game.enableMouseCamera) {
        return;
    }

    out_yaw = s_camera_yaw_rad;
    out_pitch = s_camera_pitch_rad;
}
}  // namespace dusk::mouse
