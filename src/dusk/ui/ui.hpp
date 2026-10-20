#pragma once

#include <RmlUi/Core.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_power.h>
#include <borealis/ui/ui.hpp>

#include <deque>

namespace dusk::ui {
using namespace borealis::ui;

inline constexpr DocumentScope kScopeCommandConsole = kScopeUser;
inline constexpr DocumentScope kScopePrelaunch = kScopeUser + 1;
inline constexpr DocumentScope kScopeOverlay = kScopeUser + 2;
inline constexpr DocumentScope kScopeTouchControls = kScopeUser + 3;
inline constexpr DocumentScope kScopeGraphicsTuner = kScopeUser + 4;

struct Toast {
    Rml::String type;
    Rml::String title;
    Rml::String content;
    clock::duration duration;
    Rml::String modId;
};

bool initialize() noexcept;
void shutdown() noexcept;

void handle_event(const SDL_Event& event) noexcept;
void update() noexcept;

bool is_prelaunch_open() noexcept;

void push_toast(Toast toast) noexcept;
std::deque<Toast>& get_toasts() noexcept;
void show_menu_notification() noexcept;
bool consume_menu_notification_request() noexcept;

const char* battery_icon(SDL_PowerState state, int level) noexcept;
const char* connection_state_icon(SDL_JoystickConnectionState state) noexcept;

void apply_scale() noexcept;

}  // namespace dusk::ui
