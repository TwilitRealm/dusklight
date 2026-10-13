#include "ui.hpp"

#include "command_console.hpp"
#include "drop_install_modal.hpp"
#include "icon_provider.hpp"
#include "mod_texture_provider.hpp"
#include "prelaunch.hpp"
#include "remote_texture_provider.hpp"
#include "saves_window.hpp"

#include "Z2AudioLib/Z2SeMgr.h"
#include "dusk/action_bindings.h"
#include "dusk/config.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/mods/updates.hpp"
#include "m_Do/m_Do_audio.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_power.h>
#include <absl/container/flat_hash_set.h>
#include <aurora/rmlui.hpp>
#include <borealis/io.hpp>
#include <borealis/ui/document.hpp>
#include <borealis/ui/input.hpp>
#include <fmt/format.h>
#include <tracy/Tracy.hpp>

#include <filesystem>
#include <utility>

namespace dusk::ui {
namespace {

bool sInitialized = false;
std::deque<Toast> sToasts;
bool sMenuNotificationRequested = false;
std::vector<std::filesystem::path> sDroppedPackages;

struct PendingDrop {
    borealis::Task<std::vector<DropPackage>> inspection;
};

std::vector<PendingDrop> sPendingDrops;

// Sometimes gamepads can connect and disconnect quickly, especially during
// connection negotiation. In this case, we'll receive an _ADDED event for a
// disconnected gamepad. Storing IDs here lets use only show disconnected
// notifications for gamepads that we sent a connected notification for.
absl::flat_hash_set<SDL_JoystickID> sConnectedGamepads;

u32 nav_sound_effect(NavSound sound) noexcept {
    switch (sound) {
    case NavSound::Click:
        return Z2SE_SY_CURSOR_OK;
    case NavSound::Play:
        return Z2SE_SY_ITEM_COMBINE_ON;
    case NavSound::BindingChanged:
        return Z2SE_SY_ITEM_SET_X;
    case NavSound::MenuOpen:
        return Z2SE_SY_MENU_SUB_IN;
    case NavSound::MenuClose:
        return Z2SE_SY_MENU_SUB_OUT;
    case NavSound::WindowOpen:
        return Z2SE_SY_MENU_NEXT;
    case NavSound::WindowClose:
        return Z2SE_SY_MENU_BACK;
    case NavSound::TabChanged:
        return Z2SE_SY_MENU_CURSOR_COMMON;
    case NavSound::ItemFocus:
        return Z2SE_SY_CURSOR_ITEM;
    case NavSound::ItemChange:
        return Z2SE_SY_NAME_CURSOR;
    case NavSound::ItemEnable:
        return Z2SE_SUBJ_VIEW_IN;
    case NavSound::ItemDisable:
        return Z2SE_SUBJ_VIEW_OUT;
    case NavSound::AchievementUnlock:
        return Z2SE_NAVI_FLY;
    case NavSound::Warning:
        return Z2SE_SY_COW_GET_IN;
    case NavSound::None:
    default:
        return 0;
    }
}

void play_menu_sound(NavSound sound) {
    if (const u32 effect = nav_sound_effect(sound); effect != 0) {
        mDoAud_seStartMenu(effect);
    }
}

// A bound menu action replaces the R + Start chord.
void sync_input_settings() {
    bool menuBound = false;
    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        menuBound = menuBound || getActionBindButton(ActionBinds::OPEN_DUSKLIGHT_MENU, port) !=
                                     static_cast<int>(PAD_NATIVE_BUTTON_INVALID);
    }
    const borealis::ui::input::Settings settings{
        .menuControl = getActionControl(ActionBinds::OPEN_DUSKLIGHT_MENU),
        .menuChord = !menuBound,
        .menuTap = true,
    };
    const auto& current = borealis::ui::input::settings();
    if (current.menuControl != settings.menuControl || current.menuChord != settings.menuChord ||
        current.menuTap != settings.menuTap)
    {
        borealis::ui::input::apply_settings(settings);
    }
}

}  // namespace

bool initialize() noexcept {
    if (sInitialized) {
        return true;
    }
    if (!borealis::ui::initialize()) {
        return false;
    }

    load_font("FiraSans-Regular.ttf", true);
    load_font("FiraSans-Bold.ttf");
    load_font("FiraSansCondensed-Regular.ttf");
    load_font("FiraSansCondensed-Bold.ttf");
    load_font("AlegreyaSC-Regular.ttf");
    load_font("AlegreyaSC-Bold.ttf");
    load_font("MaterialSymbolsRounded-Regular.ttf");
    load_font("NotoMono-Regular.ttf");

    set_nav_sound_handler(&play_menu_sound);
    sync_input_settings();
    CommandConsole::register_shortcut();

    register_icon_texture_provider();
    register_mod_texture_provider();
    register_remote_texture_provider();
    Rml::StyleSheetSpecification::RegisterProperty("mod-icon-tint", "transparent", false)
        .AddParser("color");
    Rml::StyleSheetSpecification::RegisterProperty("mod-icon-background", "transparent", false)
        .AddParser("color");
    sInitialized = true;
    return true;
}

void shutdown() noexcept {
    mods::updates::shutdown();
    mods::queue::shutdown();
    for (auto& drop : sPendingDrops) {
        drop.inspection.cancel();
    }
    sPendingDrops.clear();
    sDroppedPackages.clear();
    unregister_remote_texture_provider();
    unregister_mod_texture_provider();
    unregister_icon_texture_provider();
    CommandConsole::unregister_shortcut();
    borealis::ui::shutdown();
    sConnectedGamepads.clear();
    sInitialized = false;
}

const char* battery_icon(SDL_PowerState state, int level) noexcept {
    if (state == SDL_POWERSTATE_UNKNOWN || state == SDL_POWERSTATE_NO_BATTERY) {
        return "e1a6";  // Battery Unknown
    }
    if (state == SDL_POWERSTATE_ERROR) {
        return "f7ea";  // Battery Error
    }
    if (state == SDL_POWERSTATE_CHARGED || level == 100) {
        return "e1a4";  // Battery Full
    }
    if (state == SDL_POWERSTATE_CHARGING) {
        if (level >= 90) {
            return "f0a7";  // Battery Charging 90
        }
        if (level >= 80) {
            return "f0a6";  // Battery Charging 80
        }
        if (level >= 60) {
            return "f0a5";  // Battery Charging 60
        }
        if (level >= 50) {
            return "f0a4";  // Battery Charging 50
        }
        if (level >= 30) {
            return "f0a3";  // Battery Charging 30
        }
        if (level >= 20) {
            return "f0a2";  // Battery Charging 20
        }
        return "e1a3";  // Battery Charging Full (we use it as empty)
    }
    if (level >= 90) {
        return "ebd2";  // Battery 6 Bar
    }
    if (level >= 80) {
        return "ebd4";  // Battery 5 Bar
    }
    if (level >= 60) {
        return "ebe2";  // Battery 4 Bar
    }
    if (level >= 50) {
        return "ebdd";  // Battery 3 Bar
    }
    if (level >= 30) {
        return "ebe0";  // Battery 2 Bar
    }
    if (level >= 20) {
        return "ebd9";  // Battery 1 Bar
    }
    return "e19c";  // Battery Alert
}

const char* connection_state_icon(SDL_JoystickConnectionState state) noexcept {
    switch (state) {
    case SDL_JOYSTICK_CONNECTION_WIRELESS:
        return "e1a7";
    case SDL_JOYSTICK_CONNECTION_WIRED:
        return "e1e0";
    default:
        return nullptr;
    }
}

void handle_event(const SDL_Event& event) noexcept {
    if (!aurora::rmlui::is_initialized()) {
        return;
    }

    if (event.type == SDL_EVENT_DROP_BEGIN) {
        sDroppedPackages.clear();
    } else if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr) {
        sDroppedPackages.push_back(borealis::io::fs_path_from_utf8(event.drop.data));
    } else if (event.type == SDL_EVENT_DROP_COMPLETE) {
        if (!sDroppedPackages.empty()) {
            auto paths = std::exchange(sDroppedPackages, {});
            std::erase_if(paths, [](const std::filesystem::path& path) {
                const auto extension = Rml::StringUtilities::ToLower(
                    borealis::io::fs_path_to_string(path.extension()));
                if (extension != ".gci" && extension != ".raw" && extension != ".dusksave") {
                    return false;
                }
                import_save_location(borealis::io::fs_path_to_string(path));
                return true;
            });
            if (!paths.empty()) {
                sPendingDrops.push_back({
                    borealis::spawn([paths = std::move(paths)](borealis::TaskContext& context) {
                        return inspect_drop_packages(paths, context);
                    }),
                });
            }
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        auto* gamepad = SDL_GetGamepadFromID(event.gdevice.which);
        if (SDL_GamepadConnected(gamepad)) {
            if (getSettings().game.enableControllerToasts) {
                const char* name = SDL_GetGamepadName(gamepad);
                Rml::String content = fmt::format("<span>{}</span>", name ? name : "[Unknown]");
                Rml::String title = "Device Connected";
                if (const char* icon =
                        connection_state_icon(SDL_GetGamepadConnectionState(gamepad)))
                {
                    title = fmt::format(
                        "<row><span>{}</span> <icon class=\"connection\">&#x{};</icon></row>",
                        title, icon);
                }
                int batteryLevel = -1;
                const auto powerState = SDL_GetGamepadPowerInfo(gamepad, &batteryLevel);
                if (powerState != SDL_POWERSTATE_UNKNOWN) {
                    content = fmt::format(
                        "<row>{}</row><row class=\"muted\"><icon class=\"battery\">&#x{};</icon>",
                        content, battery_icon(powerState, batteryLevel));
                    if (batteryLevel > -1) {
                        content = fmt::format("{}&nbsp;<span>{}%</span>", content, batteryLevel);
                    }
                    content += "</row>";
                }
                push_toast({
                    .type = "controller",
                    .title = title,
                    .content = content,
                    .duration = std::chrono::seconds(4),
                });
            }
            sConnectedGamepads.insert(event.gdevice.which);
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED &&
               sConnectedGamepads.contains(event.gdevice.which))
    {
        if (getSettings().game.enableControllerToasts) {
            const char* name = SDL_GetGamepadNameForID(event.gdevice.which);
            push_toast({
                .type = "controller",
                .title = "Device Disconnected",
                .content = name ? name : "[Unknown]",
                .duration = std::chrono::seconds(4),
            });
        }
        sConnectedGamepads.erase(event.gdevice.which);
    }
    borealis::ui::handle_event(event);
}

bool is_prelaunch_open() noexcept {
    const auto* prelaunch = find_document(kScopePrelaunch);
    return prelaunch != nullptr && prelaunch->active();
}

void update() noexcept {
    ZoneScopedN("Dusk UI update");
    mods::queue::update();
    mods::updates::update();
    syncActionBindings();
    sync_input_settings();
    if (!aurora::rmlui::is_initialized()) {
        return;
    }

    update_remote_texture_provider();
    for (size_t index = 0; index < sPendingDrops.size();) {
        auto& pending = sPendingDrops[index];
        if (!pending.inspection.ready()) {
            ++index;
            continue;
        }
        try {
            if (auto packages = pending.inspection.try_take(); packages && !packages->empty()) {
                if (auto* current = top_document()) {
                    current->cover();
                }
                push_document(std::make_unique<DropInstallModal>(std::move(*packages)));
            }
        } catch (const std::exception& exception) {
            push_toast({
                .type = "warning",
                .title = "Could not inspect packages",
                .content = exception.what(),
                .duration = std::chrono::seconds{5},
            });
        }
        sPendingDrops.erase(sPendingDrops.begin() + static_cast<std::ptrdiff_t>(index));
    }
    borealis::ui::update();
}

void push_toast(Toast toast) noexcept {
    sToasts.push_back(std::move(toast));
}

std::deque<Toast>& get_toasts() noexcept {
    return sToasts;
}

void show_menu_notification() noexcept {
    sMenuNotificationRequested = true;
}

bool consume_menu_notification_request() noexcept {
    const bool requested = sMenuNotificationRequested;
    sMenuNotificationRequested = false;
    return requested;
}

void apply_scale() noexcept {
    set_user_scale(getSettings().video.uiScale.getValue());
}

}  // namespace dusk::ui
