#pragma once

#include <RmlUi/Core.h>
#include <SDL3/SDL_events.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "nav_types.hpp"

#include "Z2AudioLib/Z2SeMgr.h"

namespace dusk::ui {
class Document;

using clock = std::chrono::steady_clock;

enum class DocumentScope : u8 {
    None,
    CommandConsole,
    Prelaunch,
    Window,
    MenuBar,
    Overlay,
    TouchControls,
    GraphicsTuner,
};

struct Toast {
    Rml::String type;
    Rml::String title;
    Rml::String content;
    clock::duration duration;
    Rml::String modId;
};

// Button clicked/pressed
constexpr u32 kSoundClick = Z2SE_SY_CURSOR_OK;
// "Play" button clicked/pressed
constexpr u32 kSoundPlay = Z2SE_SY_ITEM_COMBINE_ON;
// Input binding changed
constexpr u32 kSoundBindingChanged = Z2SE_SY_ITEM_SET_X;

// Menu button pressed (open/close menu bar or hide/show the active window)
constexpr u32 kSoundMenuOpen = Z2SE_SY_MENU_SUB_IN;
constexpr u32 kSoundMenuClose = Z2SE_SY_MENU_SUB_OUT;

// Window opened/closed
constexpr u32 kSoundWindowOpen = Z2SE_SY_MENU_NEXT;
constexpr u32 kSoundWindowClose = Z2SE_SY_MENU_BACK;

// Window tab changed
constexpr u32 kSoundTabChanged = Z2SE_SY_MENU_CURSOR_COMMON;

// Item within menu focused
constexpr u32 kSoundItemFocus = Z2SE_SY_CURSOR_ITEM;
// Item changed (e.g. number input left/right)
constexpr u32 kSoundItemChange = Z2SE_SY_NAME_CURSOR;
// Item enabled ("On")
constexpr u32 kSoundItemEnable = Z2SE_SUBJ_VIEW_IN;
// Item disabled ("Off")
constexpr u32 kSoundItemDisable = Z2SE_SUBJ_VIEW_OUT;

// Achievement unlocked
constexpr u32 kSoundAchievementUnlock = Z2SE_NAVI_FLY;
// Warning prompt
constexpr u32 kSoundWarning = Z2SE_SY_COW_GET_IN;

struct Insets {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    bool operator==(const Insets& other) const noexcept {
        return top == other.top && right == other.right && bottom == other.bottom &&
               left == other.left;
    }
};

bool initialize() noexcept;
void shutdown() noexcept;

void handle_event(const SDL_Event& event) noexcept;
void update() noexcept;

Document& push_document(
    std::unique_ptr<Document> doc, bool show = true, bool passive = false) noexcept;

namespace detail {
Document& pop_to_or_push_document(bool (*matches)(Document&),
    const std::function<std::unique_ptr<Document>()>& create,
    const std::function<void(Document&)>& configure);
}

template <typename T, typename Configure, typename... Args>
T& pop_to_or_push(Configure&& configure, Args&&... args) {
    return static_cast<T&>(detail::pop_to_or_push_document(
        [](Document& document) { return dynamic_cast<T*>(&document) != nullptr; },
        [&]() -> std::unique_ptr<Document> {
            return std::make_unique<T>(std::forward<Args>(args)...);
        },
        [&](Document& document) {
            std::invoke(std::forward<Configure>(configure), static_cast<T&>(document));
        }));
}

void bring_document_to_front(Document& doc) noexcept;
bool register_scoped_styles(DocumentScope scope, std::string id, const std::string& rcss) noexcept;
void unregister_scoped_styles(DocumentScope scope, std::string_view id) noexcept;
void apply_scoped_styles(Document& doc) noexcept;
void uncover_top_document() noexcept;
Document* find_document(DocumentScope scope) noexcept;
void close_all_documents() noexcept;
bool any_document_visible() noexcept;
bool is_prelaunch_open() noexcept;
bool game_obscured_below(const Document& doc) noexcept;
Document* top_document() noexcept;

std::filesystem::path resource_path(const std::filesystem::path& filename) noexcept;
std::string escape(std::string_view str) noexcept;
Rml::Element* append(Rml::Element* parent, const Rml::String& tag) noexcept;
Rml::Element* append_text(Rml::Element* parent, const Rml::String& text) noexcept;
Rml::Element* append_text_element(
    Rml::Element* parent, const Rml::String& tag, const Rml::String& text) noexcept;
void clear_children(Rml::Element* parent) noexcept;
void set_text_content(Rml::Element* parent, const Rml::String& text) noexcept;

NavCommand map_nav_event(const Rml::Event& event) noexcept;
Insets safe_area_insets(Rml::Context* context) noexcept;

void push_toast(Toast toast) noexcept;
std::deque<Toast>& get_toasts() noexcept;
void show_menu_notification() noexcept;
bool consume_menu_notification_request() noexcept;

const char* battery_icon(SDL_PowerState state, int level) noexcept;
const char* connection_state_icon(SDL_JoystickConnectionState state) noexcept;

void apply_scale() noexcept;

}  // namespace dusk::ui
