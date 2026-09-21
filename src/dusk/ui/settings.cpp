#include "settings.hpp"

#include "bool_button.hpp"
#include "controller_config.hpp"
#include "graphics_tuner.hpp"
#include "menu_bar.hpp"
#include "modal.hpp"
#include "number_button.hpp"
#include "pane.hpp"
#include "prelaunch.hpp"
#include "saves_window.hpp"
#include "touch_controls_editor.hpp"
#include "ui.hpp"

#include "dusk/app_info.hpp"
#include "dusk/audio/DuskAudioSystem.h"
#include "dusk/audio/DuskDsp.hpp"
#include "dusk/config.hpp"
#include "dusk/data.hpp"
#include "dusk/discord_presence.hpp"
#include "dusk/hotkeys.h"
#include "dusk/imgui/ImGuiEngine.hpp"
#include "dusk/language.hpp"
#include "dusk/ui/i18n.hpp"
#include "dusk/livesplit.h"
#include "dusk/presentation.hpp"
#include "dusk/speedrun.h"

#include <aurora/gfx.h>
#include <aurora/lib/window.hpp>
#include <borealis/file_select.hpp>
#include <borealis/io.hpp>
#if BOREALIS_HAS_SENTRY
#include <borealis/sentry.hpp>
#endif
#include <fmt/format.h>
#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <filesystem>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_ANDROID) || defined(__ANDROID__) ||                                             \
    (defined(__APPLE__) && TARGET_OS_IOS && !TARGET_OS_MACCATALYST)
#define TOUCH_CONTROLS_AVAILABLE true
#else
#define TOUCH_CONTROLS_AVAILABLE false
#endif

namespace dusk::ui {
namespace {

// Translated UI strings, resolved lazily so a menu language change is picked up live.
using i18n::tr;
using namespace std::string_view_literals;

Rml::String tr_str(std::string_view key) {
    return Rml::String{i18n::tr_str(key)};
}

constexpr std::array kCardFileTypeKeys = {
    "card_file_type.card_image"sv,
    "card_file_type.gci_folder"sv,
};

Rml::String card_file_type_name(int index) {
    if (index < 0 || index >= static_cast<int>(kCardFileTypeKeys.size())) {
        return tr_str(kCardFileTypeKeys[0]);
    }
    return tr_str(kCardFileTypeKeys[index]);
}

constexpr std::array kFpsOverlayCornerKeys = {
    "settings.fps_corner.top_left"sv,
    "settings.fps_corner.top_right"sv,
    "settings.fps_corner.bottom_left"sv,
    "settings.fps_corner.bottom_right"sv,
};

constexpr std::array kInterpolationModeKeys = {
    "interp_mode.off"sv,
    "interp_mode.capped"sv,
    "interp_mode.unlimited"sv,
};

constexpr std::array kAudioOutputModeKeys = {
    "settings.audio_mode.stereo_speakers"sv,
    "settings.audio_mode.stereo_headphones"sv,
    "settings.audio_mode.surround_5_1"sv,
    "settings.audio_mode.surround_7_1"sv,
};

constexpr std::array kLetterboxModeKeys = {
    "letterbox_mode.off"sv,
    "letterbox_mode.on"sv,
    "letterbox_mode.gameplay_only"sv,
    "letterbox_mode.cutscene_only"sv,
};

constexpr std::array kTouchTargetingKeys = {
    "touch_targeting.hybrid"sv,
    "touch_targeting.hold"sv,
    "touch_targeting.switch"sv,
};

constexpr std::array kMenuScalingModeKeys = {
    "settings.scaling_mode.gamecube"sv,
    "settings.scaling_mode.wii"sv,
    "settings.scaling_mode.dusklight"sv,
};

constexpr std::array kAlwaysGreatspinModeKeys = {
    "settings.greatspin_mode.off"sv,
    "settings.greatspin_mode.after_skill"sv,
    "settings.greatspin_mode.always"sv,
};

constexpr std::array kMagicArmorModeKeys = {
    "settings.armor_mode.normal"sv,
    "settings.armor_mode.on_damage"sv,
    "settings.armor_mode.double_defense"sv,
    "settings.armor_mode.invincible"sv,
    "settings.armor_mode.cosmetic"sv,
};

constexpr std::array kGameLanguageNameKeys = {
    "language.english"sv,
    "language.german"sv,
    "language.french"sv,
    "language.spanish"sv,
    "language.italian"sv,
    "language.japanese"sv,
};

bool try_parse_backend(std::string_view backend, AuroraBackend& outBackend) {
    if (backend == "auto") {
        outBackend = BACKEND_AUTO;
        return true;
    }
    if (backend == "d3d11") {
        outBackend = BACKEND_D3D11;
        return true;
    }
    if (backend == "d3d12") {
        outBackend = BACKEND_D3D12;
        return true;
    }
    if (backend == "metal") {
        outBackend = BACKEND_METAL;
        return true;
    }
    if (backend == "vulkan") {
        outBackend = BACKEND_VULKAN;
        return true;
    }
    if (backend == "opengl") {
        outBackend = BACKEND_OPENGL;
        return true;
    }
    if (backend == "opengles") {
        outBackend = BACKEND_OPENGLES;
        return true;
    }
    if (backend == "webgpu") {
        outBackend = BACKEND_WEBGPU;
        return true;
    }
    if (backend == "null") {
        outBackend = BACKEND_NULL;
        return true;
    }

    return false;
}

std::string_view backend_name(AuroraBackend backend) {
    using namespace std::string_view_literals;
    switch (backend) {
    default:
        return "backend.auto"sv;
    case BACKEND_D3D12:
        return "backend.d3d12"sv;
    case BACKEND_D3D11:
        return "backend.d3d11"sv;
    case BACKEND_METAL:
        return "backend.metal"sv;
    case BACKEND_VULKAN:
        return "backend.vulkan"sv;
    case BACKEND_OPENGL:
        return "backend.opengl"sv;
    case BACKEND_OPENGLES:
        return "backend.opengles"sv;
    case BACKEND_WEBGPU:
        return "backend.webgpu"sv;
    case BACKEND_NULL:
        return "backend.null"sv;
    }
}

std::string_view backend_id(AuroraBackend backend) {
    switch (backend) {
    default:
        return "auto";
    case BACKEND_D3D12:
        return "d3d12";
    case BACKEND_D3D11:
        return "d3d11";
    case BACKEND_METAL:
        return "metal";
    case BACKEND_VULKAN:
        return "vulkan";
    case BACKEND_OPENGL:
        return "opengl";
    case BACKEND_OPENGLES:
        return "opengles";
    case BACKEND_WEBGPU:
        return "webgpu";
    case BACKEND_NULL:
        return "null";
    }
}

std::vector<AuroraBackend> available_backends() {
    std::vector<AuroraBackend> backends;
    backends.emplace_back(BACKEND_AUTO);
    size_t backendCount = 0;
    const AuroraBackend* raw = aurora_get_available_backends(&backendCount);
    for (size_t i = 0; i < backendCount; ++i) {
        // Do not expose NULL
        if (raw[i] != BACKEND_NULL) {
            backends.emplace_back(raw[i]);
        }
    }
    return backends;
}

AuroraBackend configured_backend() {
    AuroraBackend configuredBackend = BACKEND_AUTO;
    const auto configuredId = getSettings().backend.graphicsBackend.getValue();
    if (!try_parse_backend(configuredId, configuredBackend)) {
        configuredBackend = BACKEND_AUTO;
    }
    return configuredBackend;
}

bool is_graphics_backend_restart_pending() {
    return getSettings().backend.graphicsBackend.getValue() !=
           prelaunch_state().initialGraphicsBackend;
}

Rml::String graphics_backend_display_name() {
    if (is_graphics_backend_restart_pending()) {
        return tr_str(backend_name(configured_backend()));
    }
    return tr_str(backend_name(aurora_get_backend()));
}

Rml::String configured_data_path_display_name() {
    const auto path = data::abbreviated_path_string(data::configured_data_path());
    if (path.empty()) {
        return tr_str("common.none");
    }

    auto display = borealis::io::display_name(path);
    if (display.empty()) {
        return path;
    }
    return display;
}

class DataFolderPathText : public Component {
public:
    explicit DataFolderPathText(Rml::Element* parent)
        : Component(append(parent, "data-folder-path")) {
        append_text_element(mRoot, "small", tr_str("settings.prelaunch.current_data_folder"));
        mPath = append(mRoot, "file-path");
    }

    void update() override {
        const Rml::String path = data::abbreviated_path_string(data::configured_data_path());
        if (path != mCurrentPath) {
            set_text_content(mPath, path);
            mCurrentPath = path;
        }
        Component::update();
    }

private:
    Rml::Element* mPath = nullptr;
    Rml::String mCurrentPath;
};

void show_data_folder_error_modal(std::string_view message) {
    auto dismiss = [](Modal& modal) {
        mDoAud_seStartMenu(kSoundWindowClose);
        modal.pop();
    };
    push_document(std::make_unique<Modal>(Modal::Props{
        .title = tr_str("modal.data_folder_error.title"),
        .bodyText = Rml::String{message},
        .actions =
            {
                ModalAction{
                    .label = "OK",
                    .onPressed = dismiss,
                },
            },
        .onDismiss = dismiss,
        .icon = "warning",
    }));
    if (auto* doc = top_document()) {
        doc->focus();
    }
}

void data_folder_dialog_callback(borealis::file_select::Result result) {
    if (result.status == borealis::file_select::Status::Canceled) {
        return;
    }
    if (result.status != borealis::file_select::Status::Selected || result.locations.empty()) {
        show_data_folder_error_modal(tr_str("modal.data_folder_error.picker"));
        return;
    }

    std::string dataPathError;
    if (data::set_custom_data_path(result.locations.front(), &dataPathError)) {
        mDoAud_seStartMenu(kSoundItemChange);
        return;
    }

    if (dataPathError.empty()) {
        dataPathError = fmt::format(
            fmt::runtime(tr("modal.data_folder_error.invalid")), fmt::arg("app", AppName));
    }
    show_data_folder_error_modal(dataPathError);
}

int float_setting_percent(ConfigVar<float>& var) {
    return static_cast<int>(var.getValue() * 100.0f + 0.5f);
}

bool gyro_enabled() {
    return getSettings().game.enableGyroAim || getSettings().game.enableGyroRollgoal;
}

Rml::String touch_targeting_label(TouchTargeting targeting) {
    const auto index = static_cast<size_t>(targeting);
    if (index >= kTouchTargetingKeys.size()) {
        return tr_str("common.unknown");
    }
    return tr_str(kTouchTargetingKeys[index]);
}

struct ConfigBoolProps {
    Rml::String key;
    Rml::String icon;
    Rml::String helpText;
    std::function<void(bool)> onChange;
    std::function<bool()> isDisabled;
};

SelectButton& config_bool_select(
    Pane& leftPane, Pane& rightPane, ConfigVar<bool>& var, ConfigBoolProps props) {
    auto& button = leftPane.add_child<BoolButton>(BoolButton::Props{
        .key = std::move(props.key),
        .icon = std::move(props.icon),
        .getValue = [&var] { return var.getValue(); },
        .setValue =
            [&var, callback = std::move(props.onChange)](bool value) {
                if (value == var.getValue()) {
                    return;
                }
                var.setValue(value);
                config::save();
                if (callback) {
                    callback(value);
                }
            },
        .isDisabled = std::move(props.isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
    });
    leftPane.register_control(
        button, rightPane, [helpText = std::move(props.helpText)](Pane& pane) {
            pane.clear();
            pane.add_rml(helpText);
        });
    return button;
}

void add_speedrun_disabled_option(Pane& leftPane, Pane& rightPane, ConfigVar<bool>& var,
    const Rml::String& key, const Rml::String& helpText) {
    config_bool_select(leftPane, rightPane, var, {
        .key = key,
        .helpText = helpText,
        .isDisabled = [] { return speedrun::isActive(); },
    });
}

SelectButton& config_percent_select(Pane& leftPane, Pane& rightPane, ConfigVar<float>& var,
    Rml::String key, Rml::String helpText, int min, int max, int step = 5,
    std::function<bool()> isDisabled = {}) {
    auto& button = leftPane.add_child<NumberButton>(NumberButton::Props{
        .key = std::move(key),
        .getValue = [&var] { return float_setting_percent(var); },
        .setValue =
            [&var, min, max](int value) {
                var.setValue(std::clamp(value, min, max) / 100.0f);
                config::save();
            },
        .isDisabled = std::move(isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
        .min = min,
        .max = max,
        .step = step,
        .suffix = "%",
    });
    leftPane.register_control(button, rightPane, [helpText = std::move(helpText)](Pane& pane) {
        pane.clear();
        pane.add_rml(helpText);
    });
    return button;
}

SelectButton& config_int_select(Pane& leftPane, Pane& rightPane, ConfigVar<int>& var,
    Rml::String key, Rml::String helpText, int min, int max, int step = 5,
    std::function<bool()> isDisabled = {}, std::function<void(int)> onChange = {},
    std::string suffix = "") {
    auto& button = leftPane.add_child<NumberButton>(NumberButton::Props{
        .key = std::move(key),
        .getValue = [&var] { return var.getValue(); },
        .setValue =
            [&var, min, max, callback = std::move(onChange)](int value) {
                const int clampedValue = std::clamp(value, min, max);
                var.setValue(clampedValue);
                config::save();
                if (callback) {
                    callback(clampedValue);
                }
            },
        .isDisabled = std::move(isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
        .min = min,
        .max = max,
        .step = step,
        .suffix = suffix,
    });
    leftPane.register_control(button, rightPane, [helpText = std::move(helpText)](Pane& pane) {
        pane.clear();
        pane.add_text(helpText);
    });
    return button;
}

void graphics_tuner_control(Window& window, Pane& leftPane, Pane& rightPane,
    const GraphicsTunerProps& props) {
    const auto setting = GraphicsSetting::of(props.option);
    leftPane.register_control(
        leftPane
            .add_select_button({
                .key = props.title,
                .getValue = [setting] { return setting.text(); },
                .isModified = [setting] { return setting.isModified(); },
                .submit = false,
            })
            .on_nav_command([&window, props](Rml::Event&, NavCommand cmd) {
                if (cmd == NavCommand::Confirm || cmd == NavCommand::Left ||
                    cmd == NavCommand::Right) {
                    window.push(std::make_unique<GraphicsTuner>(props));
                    return true;
                }
                return false;
            }),
        rightPane, [helpText = props.helpText](Pane& pane) {
            pane.clear();
            pane.add_text(helpText);
        });
}

Rml::String menu_language_label(MenuLanguage language) {
    switch (language) {
    case MenuLanguage::Auto:
        return tr_str("settings.menu_language.auto");
    case MenuLanguage::English:
        return tr_str("settings.menu_language.english");
    case MenuLanguage::Spanish:
        return tr_str("settings.menu_language.spanish");
    }
    return tr_str("settings.menu_language.auto");
}

void add_menu_language_control(Pane& leftPane, Pane& rightPane) {
    leftPane.register_control(
        leftPane.add_select_button({
            .key = tr_str("settings.prelaunch.menu_language"),
            .getValue =
                [] {
                    return menu_language_label(getSettings().ui.menuLanguage.getValue());
                },
            .isModified =
                [] {
                    return getSettings().ui.menuLanguage.getValue() !=
                           getSettings().ui.menuLanguage.getDefaultValue();
                },
        }),
        rightPane, [](Pane& pane) {
            pane.clear();
            for (int i = 0; i <= static_cast<int>(MenuLanguage::Spanish); ++i) {
                const auto language = static_cast<MenuLanguage>(i);
                pane.add_button(
                        {
                            .text = menu_language_label(language),
                            .isSelected =
                                [language] {
                                    return getSettings().ui.menuLanguage.getValue() == language;
                                },
                        })
                    .on_pressed([language] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        if (getSettings().ui.menuLanguage.getValue() != language) {
                            getSettings().ui.menuLanguage.setValue(language);
                            config::save();
                            i18n::request_refresh();
                        }
                    });
            }
            pane.add_rml(tr_str("settings.menu_language.help"));
        });
}

}  // namespace

SettingsWindow::SettingsWindow(bool prelaunch) : mPrelaunch(prelaunch) {
    if (prelaunch) {
        add_tab([] { return tr_str("settings.tab.prelaunch"); }, [this](Rml::Element* content) {
            auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
            auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

            leftPane.register_control(
                leftPane
                    .add_select_button({
                        .key = tr_str("settings.prelaunch.disc_image"),
                        .getValue =
                            [] {
                                const auto& path = prelaunch_state().configuredDiscPath;
                                std::string display;
                                if (path.empty()) {
                                    display = tr_str("common.none");
                                } else {
                                    display = borealis::io::display_name(path);
                                    if (display.empty()) {
                                        display = path;
                                    }
                                }
                                return display;
                            },
                        .isModified =
                            [] {
                                const auto& state = prelaunch_state();
                                const auto& active = state.activeDiscPath;
                                return !active.empty() && state.configuredDiscPath != active;
                            },
                    })
                    .on_pressed([] { open_iso_picker(); }),
                rightPane, [](Pane& pane) {
                    pane.add_rml(tr_str("settings.prelaunch.disc_image_help"));
                });
            if (data::manager().capabilities().canChangeLocation &&
                borealis::file_select::capabilities().canOpenFolder)
            {
                leftPane.register_control(
                    leftPane.add_select_button({
                        .key = tr_str("settings.prelaunch.data_folder"),
                        .getValue = [] { return configured_data_path_display_name(); },
                        .isModified = [] { return data::is_data_path_restart_pending(); },
                    }),
                    rightPane, [](Pane& pane) {
                        pane.add_text(tr_str("settings.prelaunch.data_folder_help"));
                        pane.add_child<DataFolderPathText>();
#if DUSK_CAN_OPEN_DATA_FOLDER
                        pane.add_button(tr_str("settings.prelaunch.open_data_folder")).on_pressed([] {
                            if (data::open_data_path()) {
                                mDoAud_seStartMenu(kSoundClick);
                            }
                        });
#endif
                        pane.add_button(tr_str("settings.prelaunch.change_data_folder")).on_pressed([] {
                            const auto defaultLocation =
                                borealis::io::fs_path_to_string(data::configured_data_path());
                            borealis::file_select::open_folder(
                                {
                                    .parentWindow = aurora::window::get_sdl_window(),
                                    .defaultLocation = defaultLocation,
                                    .requireRealPath = true,
                                },
                                &data_folder_dialog_callback);
                        });
#if defined(_WIN32)
                        pane.add_button(tr_str("settings.prelaunch.portable_mode")).on_pressed([] {
                            if (data::set_portable_data_path()) {
                                mDoAud_seStartMenu(kSoundItemChange);
                            }
                        });
#endif
                        pane.add_button(
                                {
                                    .text = tr_str("settings.prelaunch.reset_to_default"),
                                    .isDisabled = [] { return data::is_default_data_path(); },
                                })
                            .on_pressed([] {
                                if (data::reset_data_path()) {
                                    mDoAud_seStartMenu(kSoundItemChange);
                                }
                            });
                        pane.add_rml(tr_str("settings.prelaunch.data_migrated_note"));
                    });
            }
            add_menu_language_control(leftPane, rightPane);
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = tr_str("settings.prelaunch.game_language"),
                    .getValue =
                        [] {
                            return language::language_name(getSettings().game.language.getValue());
                        },
                    .isDisabled =
                        [] {
                            const auto& state = prelaunch_state();
                            if (!state.configuredDiscCanLaunch) {
                                return true;
                            }
                            return language::available_languages(state.configuredDiscInfo).size() <= 1;
                        },
                    .isModified =
                        [] {
                            return getSettings().game.language.getValue() !=
                                   prelaunch_state().initialLanguage;
                        },
                }),
                rightPane, [](Pane& pane) {
                    const auto& state = prelaunch_state();
                    const auto languages = state.configuredDiscCanLaunch
                                               ? language::available_languages(state.configuredDiscInfo)
                                               : language::available_languages({});
                    for (const GameLanguage language : languages) {
                        pane.add_button({
                                            .text = tr_str(kGameLanguageNameKeys[static_cast<size_t>(language)]),
                                            .isSelected =
                                                [language] {
                                                    return getSettings().game.language.getValue() ==
                                                           language;
                                                },
                                        })
                            .on_pressed([language] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().game.language.setValue(language);
                                config::save();
                            });
                    }
                    pane.add_rml(tr_str("common.changes_require_restart"));
                });
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = tr_str("settings.prelaunch.graphics_backend"),
                    .getValue = [] { return graphics_backend_display_name(); },
                    .isModified = [] { return is_graphics_backend_restart_pending(); },
                }),
                rightPane, [](Pane& pane) {
                    const auto availableBackends = available_backends();
                    for (const auto backend : availableBackends) {
                        pane
                            .add_button({
                                .text = tr_str(backend_name(backend)),
                                .isSelected = [backend] { return configured_backend() == backend; },
                            })
                            .on_pressed([backend] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().backend.graphicsBackend.setValue(
                                    std::string{backend_id(backend)});
                                config::save();
                            });
                    }
                    pane.add_rml(tr_str("common.changes_require_restart"));
                });
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = tr_str("settings.prelaunch.save_file_type"),
                    .getValue =
                        [] {
                            return card_file_type_name(getSettings().backend.cardFileType.getValue());
                        },
                    .isModified =
                        [] {
                            return getSettings().backend.cardFileType.getValue() !=
                                   prelaunch_state().initialCardFileType;
                        },
                }),
                rightPane, [](Pane& pane) {
                    for (int i = 0; i < static_cast<int>(kCardFileTypeKeys.size()); i++) {
                    pane
                            .add_button({
                                .text = card_file_type_name(i),
                                .isSelected =
                                    [i] {
                                        return getSettings().backend.cardFileType.getValue() == i;
                                    },
                            })
                            .on_pressed([i] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().backend.cardFileType.setValue(i);
                                config::save();
                            });
                    }
                });
            add_save_files_control(leftPane, rightPane);
        });
    }

    add_tab([] { return tr_str("settings.tab.video"); }, [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(tr_str("settings.section.display"));

        leftPane.register_control(leftPane.add_button(tr_str("settings.video.toggle_fullscreen")).on_pressed([] {
            mDoAud_seStartMenu(kSoundItemChange);
            getSettings().video.enableFullscreen.setValue(!getSettings().video.enableFullscreen);
            VISetWindowFullscreen(getSettings().video.enableFullscreen);
            config::save();
        }),
            rightPane, [](Pane& pane) { pane.clear(); });
        leftPane.register_control(leftPane.add_button(tr_str("settings.video.restore_window_size")).on_pressed([] {
            mDoAud_seStartMenu(kSoundItemChange);
            getSettings().video.enableFullscreen.setValue(false);
            VISetWindowFullscreen(false);
            VISetWindowSize(FB_WIDTH * 2, FB_HEIGHT * 2);
            VICenterWindow();
        }),
            rightPane, [](Pane& pane) { pane.clear(); });
        config_bool_select(leftPane, rightPane, getSettings().video.enableVsync,
            {
                .key = tr_str("settings.video.vsync"),
                .helpText = tr_str("settings.video.vsync_help"),
                .onChange = [](bool value) { aurora_enable_vsync(value); },
            });
        config_bool_select(leftPane, rightPane, getSettings().video.lockAspectRatio,
            {
                .key = tr_str("settings.video.lock_aspect_ratio"),
                .helpText = tr_str("settings.video.lock_aspect_ratio_help"),
                .onChange =
                    [](bool value) {
                        AuroraSetViewportPolicy(
                            value ? AURORA_VIEWPORT_FIT : AURORA_VIEWPORT_STRETCH);
                    },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.pauseOnFocusLost,
            {
                .key = tr_str("settings.video.pause_on_focus_lost"),
                .helpText = tr_str("settings.video.pause_on_focus_lost_help"),
                .isDisabled = [] { return IsMobile || speedrun::isActive(); },
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.video.show_fps_counter"),
                .getValue =
                    [] {
                        if (!getSettings().video.enableFpsOverlay.getValue()) {
                            return tr_str("common.off");
                        }
                        const int idx = getSettings().video.fpsOverlayCorner.getValue();
                        return tr_str(kFpsOverlayCornerKeys[idx]);
                    },
                .isModified =
                    [] {
                        const auto& enable = getSettings().video.enableFpsOverlay;
                        const auto& corner = getSettings().video.fpsOverlayCorner;
                        return enable.getValue() != enable.getDefaultValue() ||
                               (enable.getValue() && corner.getValue() != corner.getDefaultValue());
                    },
            }),
            rightPane, [](Pane& pane) {
                pane.add_button(
                        {
                            .text = tr_str("common.off"),
                            .isSelected =
                                [] { return !getSettings().video.enableFpsOverlay.getValue(); },
                        })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        getSettings().video.enableFpsOverlay.setValue(false);
                        config::save();
                    });
                for (int i = 0; i < static_cast<int>(kFpsOverlayCornerKeys.size()); ++i) {
                    pane.add_button(
                            {
                                .text = tr_str(kFpsOverlayCornerKeys[i]),
                                .isSelected =
                                    [i] {
                                        return getSettings().video.enableFpsOverlay.getValue() &&
                                               getSettings().video.fpsOverlayCorner.getValue() == i;
                                    },
                            })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().video.enableFpsOverlay.setValue(true);
                            getSettings().video.fpsOverlayCorner.setValue(i);
                            config::save();
                        });
                }
                pane.add_rml(tr_str("settings.video.fps_counter_help"));
            });
        config_bool_select(leftPane, rightPane, getSettings().video.rememberWindowSize,
            {
                .key = tr_str("settings.video.remember_window_size"),
                .helpText = tr_str("settings.video.remember_window_size_help"),
                .onChange =
                    [](bool value) {
                        if (value && !getSettings().video.enableFullscreen) {
                            const auto windowSize = aurora::window::get_window_size();
                            getSettings().video.lastWindowWidth.setValue(windowSize.width);
                            getSettings().video.lastWindowHeight.setValue(windowSize.height);
                            config::save();
                        }
                    },
                .isDisabled = [] { return IsMobile; },
            });

        config_int_select(leftPane, rightPane, getSettings().video.uiScale,
            tr_str("settings.video.ui_scale"),
            tr_str("settings.video.ui_scale_help"),
            50, 200, 25, {}, {}, "%");

        leftPane.add_section(tr_str("settings.section.resolution"));
        graphics_tuner_control(*this, leftPane, rightPane,
            GraphicsTunerProps{
                .option = GraphicsOption::InternalResolution,
                .title = tr_str("settings.video.internal_resolution"),
                .helpText = tr_str("settings.video.internal_resolution_help"),
            });
        graphics_tuner_control(*this, leftPane, rightPane,
            GraphicsTunerProps{
                .option = GraphicsOption::ShadowResolution,
                .title = tr_str("settings.video.shadow_resolution"),
                .helpText = tr_str("settings.video.shadow_resolution_help"),
            });
        graphics_tuner_control(*this, leftPane, rightPane,
            GraphicsTunerProps{
                .option = GraphicsOption::Resampler,
                .title = tr_str("settings.video.output_resampling"),
                .helpText = tr_str("settings.video.resampler_help"),
            });

        leftPane.add_section(tr_str("settings.section.post_processing"));
        graphics_tuner_control(*this, leftPane, rightPane,
            GraphicsTunerProps{
                .option = GraphicsOption::BloomMode,
                .title = tr_str("settings.video.bloom"),
                .helpText = tr_str("settings.video.bloom_help"),
            });
        graphics_tuner_control(*this, leftPane, rightPane,
            GraphicsTunerProps{
                .option = GraphicsOption::BloomMultiplier,
                .title = tr_str("settings.video.bloom_brightness"),
                .helpText = tr_str("settings.video.bloom_brightness_help"),
            });
        graphics_tuner_control(*this, leftPane, rightPane,
            GraphicsTunerProps{
                .option = GraphicsOption::DepthOfFieldMode,
                .title = tr_str("settings.video.depth_of_field"),
                .helpText = tr_str("settings.video.depth_of_field_help"),
            });

        leftPane.add_section(tr_str("settings.section.rendering"));
        graphics_tuner_control(*this, leftPane, rightPane,
            GraphicsTunerProps{
                .option = GraphicsOption::TextureReplacements,
                .title = tr_str("settings.video.enable_texture_replacements"),
                .helpText = tr_str("settings.video.texture_replacements_help"),
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.video.unlock_framerate"),
                .getValue =
                    [] {
                        return tr_str(kInterpolationModeKeys[static_cast<u8>(
                            getSettings().game.enableFrameInterpolation.getValue())]);
                    },
                .isModified =
                    [] {
                        return getSettings().game.enableFrameInterpolation.getValue() !=
                               getSettings().game.enableFrameInterpolation.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kInterpolationModeKeys.size()); i++) {
                    pane.add_button({
                            .text = tr_str(kInterpolationModeKeys[i]),
                            .isSelected =
                                [i] {
                                    return getSettings().game.enableFrameInterpolation.getValue() == static_cast<FrameInterpMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.enableFrameInterpolation.setValue(static_cast<FrameInterpMode>(i));
                            presentation::update_frame_rate_preference();
                            config::save();
                        });
                }
                pane.add_rml(tr_str("settings.video.unlock_framerate_help"));
            });
        config_int_select(leftPane, rightPane, getSettings().video.maxFrameRate,
            tr_str("settings.video.framerate_cap"), tr_str("settings.video.framerate_cap_help"), 30, 540, 1,
            [] { return getSettings().game.enableFrameInterpolation.getValue() != FrameInterpMode::Capped; },
            [](int) { presentation::update_frame_rate_preference(); });
        config_bool_select(leftPane, rightPane, getSettings().game.enableMapBackground,
            {
                .key = tr_str("settings.video.minimap_shadows"),
                .helpText = tr_str("settings.video.minimap_shadows_help")
            });
        config_bool_select(leftPane, rightPane, getSettings().game.disableCutscenePillarboxing,
            {
                .key = tr_str("settings.video.disable_pillarboxing"),
                .helpText = tr_str("settings.video.disable_pillarboxing_help"),
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.video.disable_letterboxing"),
                .getValue =
                    [] {
                        return tr_str(kLetterboxModeKeys[static_cast<u8>(getSettings().game.disableLetterboxing.getValue())]);
                    },
                .isModified =
                    [] {
                        return getSettings().game.disableLetterboxing.getValue() !=
                               getSettings().game.disableLetterboxing.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kLetterboxModeKeys.size()); i++) {
                    pane.add_button({
                            .text = tr_str(kLetterboxModeKeys[i]),
                            .isSelected =
                                [i] {
                                    return getSettings().game.disableLetterboxing.getValue() == static_cast<LetterboxMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.disableLetterboxing.setValue(static_cast<LetterboxMode>(i));
                            config::save();
                        });
                }
                pane.add_rml(tr_str("settings.video.disable_letterboxing_help"));
            });
    });

    add_tab([] { return tr_str("settings.tab.input"); }, [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                             const Rml::String& helpText, std::function<bool()> isDisabled = {}) {
            config_bool_select(leftPane, rightPane, value,
                {
                    .key = key,
                    .helpText = helpText,
                    .isDisabled = std::move(isDisabled),
                });
        };

        leftPane.add_section(tr_str("settings.section.inputs"));
        leftPane.register_control(
            leftPane.add_group_button({.text = tr_str("settings.input.configure_inputs")}).on_pressed([this] {
                push(std::make_unique<ControllerConfigWindow>());
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(tr_str("settings.input.configure_inputs_help"));
            });
        config_bool_select(leftPane, rightPane, getSettings().game.allowBackgroundInput,
            {
                .key = tr_str("settings.input.background_inputs"),
                .helpText = tr_str("settings.input.background_inputs_help"),
                .onChange = [](bool value) { aurora_set_background_input(value); },
            });

#if TOUCH_CONTROLS_AVAILABLE
        leftPane.add_section(tr_str("settings.section.touch"));
        addOption(tr_str("settings.input.touch_controls"), getSettings().game.enableTouchControls,
            tr_str("settings.input.touch_controls_help"));
        auto& customizeTouchLayout = leftPane.add_group_button(GroupButton::Props{
            .text = tr_str("settings.input.customize_layout"),
            .isDisabled = [] { return !getSettings().game.enableTouchControls; },
        });
        leftPane.register_control(customizeTouchLayout.on_pressed(
                                      [this] { push(std::make_unique<TouchControlsEditor>()); }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(tr_str("settings.input.customize_layout_help"));
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.input.touch_targeting"),
                .getValue =
                    [] {
                        return touch_targeting_label(getSettings().game.touchTargeting.getValue());
                    },
                .isDisabled = [] { return !getSettings().game.enableTouchControls; },
                .isModified =
                    [] {
                        const auto& targeting = getSettings().game.touchTargeting;
                        return targeting.getValue() != targeting.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                for (int i = 0; i < static_cast<int>(kTouchTargetingKeys.size()); ++i) {
                    pane.add_button({
                            .text = tr_str(kTouchTargetingKeys[i]),
                            .isSelected =
                                [i] {
                                    return getSettings().game.touchTargeting.getValue() ==
                                           static_cast<TouchTargeting>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.touchTargeting.setValue(
                                static_cast<TouchTargeting>(i));
                            config::save();
                        });
                }
                pane.add_rml(tr_str("settings.input.touch_targeting_help"));
            });
        config_percent_select(leftPane, rightPane, getSettings().game.touchCameraXSensitivity,
            tr_str("settings.input.touch_camera_x"),
            tr_str("settings.input.touch_camera_x_help"),
            25, 400, 5, [] { return !getSettings().game.enableTouchControls; });
        config_percent_select(leftPane, rightPane, getSettings().game.touchCameraYSensitivity,
            tr_str("settings.input.touch_camera_y"),
            tr_str("settings.input.touch_camera_y_help"), 25,
            400, 5, [] { return !getSettings().game.enableTouchControls; });
#endif

        leftPane.add_section(tr_str("settings.section.camera"));
        addOption(tr_str("settings.input.free_camera"), getSettings().game.freeCamera,
            tr_str("settings.input.free_camera_help"));
        config_percent_select(leftPane, rightPane, getSettings().game.freeCameraXSensitivity,
            tr_str("settings.input.free_camera_x"),
            tr_str("settings.input.free_camera_help"),
            50, 200, 5, [] { return !getSettings().game.freeCamera; });
        config_percent_select(leftPane, rightPane, getSettings().game.freeCameraYSensitivity,
            tr_str("settings.input.free_camera_y"),
            tr_str("settings.input.free_camera_help"),
            50, 200, 5, [] { return !getSettings().game.freeCamera; });
        addOption(tr_str("settings.input.invert_camera_x"), getSettings().game.invertCameraXAxis,
            tr_str("settings.input.invert_camera_help"));
        addOption(tr_str("settings.input.invert_camera_y"), getSettings().game.invertCameraYAxis,
            tr_str("settings.input.invert_camera_help"),
            [] { return !getSettings().game.freeCamera; });
        addOption(tr_str("settings.input.invert_fp_x"), getSettings().game.invertFirstPersonXAxis,
            tr_str("settings.input.invert_fp_help"));
        addOption(tr_str("settings.input.invert_fp_y"), getSettings().game.invertFirstPersonYAxis,
            tr_str("settings.input.invert_fp_help"));

        leftPane.add_section(tr_str("settings.section.gyro"));
        addOption(tr_str("settings.input.gyro_aim"), getSettings().game.enableGyroAim,
            tr_str("settings.input.gyro_aim_help"));
        addOption(tr_str("settings.input.gyro_rollgoal"), getSettings().game.enableGyroRollgoal,
            tr_str("settings.input.gyro_rollgoal_help"));
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityY,
            tr_str("settings.input.gyro_pitch"), tr_str("settings.input.gyro_pitch_help"), 25, 400, 5,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityX,
            tr_str("settings.input.gyro_yaw"), tr_str("settings.input.gyro_yaw_help"), 25, 400, 5,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityRollgoal,
            tr_str("settings.input.rollgoal_sensitivity"), tr_str("settings.input.rollgoal_sensitivity_help"),
            25, 400, 5,
            [] { return !getSettings().game.enableGyroRollgoal; });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroDeadband, tr_str("settings.input.gyro_deadband"),
            tr_str("settings.input.gyro_deadband_help"), 0, 50, 1,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSmoothing,
            tr_str("settings.input.gyro_smoothing"), tr_str("settings.input.gyro_smoothing_help"), 0, 100, 1,
            [] { return !gyro_enabled(); });
        addOption(tr_str("settings.input.invert_gyro_pitch"), getSettings().game.gyroInvertPitch,
            tr_str("settings.input.invert_gyro_pitch_help"), [] { return !gyro_enabled(); });
        addOption(tr_str("settings.input.invert_gyro_yaw"), getSettings().game.gyroInvertYaw,
            tr_str("settings.input.invert_gyro_yaw_help"), [] { return !gyro_enabled(); });

        leftPane.add_section(tr_str("settings.section.mouse"));
        addOption(tr_str("settings.input.mouse_aim"), getSettings().game.enableMouseAim,
            tr_str("settings.input.mouse_aim_help"));
        addOption(tr_str("settings.input.mouse_camera"), getSettings().game.enableMouseCamera,
            tr_str("settings.input.mouse_camera_help"));
        config_percent_select(leftPane, rightPane, getSettings().game.mouseAimSensitivity,
            tr_str("settings.input.mouse_aim_sensitivity"), tr_str("settings.input.mouse_aim_sensitivity_help"), 25, 400, 5,
            [] { return !getSettings().game.enableMouseAim; });
        config_percent_select(leftPane, rightPane, getSettings().game.mouseCameraSensitivity,
            tr_str("settings.input.mouse_camera_sensitivity"), tr_str("settings.input.mouse_camera_sensitivity_help"), 25, 400, 5,
            [] { return !getSettings().game.enableMouseCamera; });
        addOption(tr_str("settings.input.invert_mouse_y"), getSettings().game.invertMouseY,
            tr_str("settings.input.invert_mouse_y_help"),
            [] { return !getSettings().game.enableMouseAim || !getSettings().game.enableMouseCamera; });

        leftPane.add_section(tr_str("settings.section.gameplay_input"));
        addOption(tr_str("settings.input.menu_pointer"), getSettings().game.enableMenuPointer,
            tr_str("settings.input.menu_pointer_help"));
        addOption(tr_str("settings.input.invert_air_swim_x"), getSettings().game.invertAirSwimX,
            tr_str("settings.input.invert_air_swim_x_help"));
        addOption(tr_str("settings.input.invert_air_swim_y"), getSettings().game.invertAirSwimY,
            tr_str("settings.input.invert_air_swim_y_help"));
        addOption(tr_str("settings.input.swap_direct_select"), getSettings().game.swapDirectSelect,
            tr_str("settings.input.swap_direct_select_help"));

        leftPane.add_section(tr_str("settings.section.tools_input"));
        addOption(tr_str("settings.input.turbo_key"), getSettings().game.enableTurboKeybind,
            tr_str("settings.input.turbo_key_help"),
            [] { return speedrun::isActive(); });
        addOption(fmt::format(fmt::runtime(tr("settings.input.reset_key")),
                      fmt::arg("key", hotkeys::DO_RESET)),
            getSettings().game.enableResetKeybind,
            fmt::format(fmt::runtime(tr("settings.input.reset_key_help")),
                fmt::arg("key", hotkeys::DO_RESET)));
    });

    add_tab([] { return tr_str("settings.tab.audio"); }, [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(tr_str("settings.section.output"));
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.audio.output_mode"),
                .getValue = [] {
                    const auto idx = static_cast<int>(getSettings().audio.outputMode.getValue());
                    return tr_str(kAudioOutputModeKeys[idx]);
                },
                .isModified = [] {
                    const auto& setting = getSettings().audio.outputMode;
                    return setting.getValue() != setting.getDefaultValue();
                },
            }), rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kAudioOutputModeKeys.size()); ++i) {
                    pane.add_button({
                        .text = tr_str(kAudioOutputModeKeys[i]),
                        .isSelected = [i] {
                            const auto& setting = getSettings().audio.outputMode;
                            return setting.getValue() == static_cast<AudioOutputMode>(i);
                        },
                    }).on_pressed([i] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        getSettings().audio.outputMode.setValue(static_cast<AudioOutputMode>(i));
                        config::save();
                        audio::Reinitialize();
                    });
                }
            });

        // TODO: Individual sliders for Sub Music, Sound Effects, and Fanfare.
        leftPane.add_section(tr_str("settings.section.volume"));
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = tr_str("settings.audio.master_volume"),
                .getValue = [] { return getSettings().audio.masterVolume.getValue(); },
                .setValue =
                    [](int value) {
                        getSettings().audio.masterVolume.setValue(value);
                        config::save();
                        audio::SetMasterVolume(audio::MasterVolumeToLinear(value / 100.0f));
                    },
                .isModified =
                    [] {
                        return getSettings().audio.masterVolume.getValue() !=
                               getSettings().audio.masterVolume.getDefaultValue();
                    },
                .max = 100,
                .suffix = "%",
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(tr_str("settings.audio.master_volume_help"));
            });
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = tr_str("settings.audio.main_music_volume"),
                .getValue = [] { return getSettings().audio.mainMusicVolume.getValue(); },
                .setValue =
                    [](int value) {
                        getSettings().audio.mainMusicVolume.setValue(value);
                        config::save();
                    },
                .isModified =
                    [] {
                        return getSettings().audio.mainMusicVolume.getValue() !=
                               getSettings().audio.mainMusicVolume.getDefaultValue();
                    },
                .max = 100,
                .suffix = "%",
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(tr_str("settings.audio.main_music_volume_help"));
            });

        leftPane.add_section(tr_str("settings.section.effects"));
        config_bool_select(leftPane, rightPane, getSettings().audio.enableReverb,
            {
                .key = tr_str("settings.audio.enable_reverb"),
                .helpText = tr_str("settings.audio.enable_reverb_help"),
                .onChange = [](bool value) { audio::SetEnableReverb(value); },
            });
        config_bool_select(leftPane, rightPane, getSettings().audio.menuSounds,
            {
                .key = tr_str("settings.audio.menu_sounds"),
                .helpText = tr_str("settings.audio.menu_sounds_help"),
            });

        leftPane.add_section(tr_str("settings.section.tweaks"));
        config_bool_select(leftPane, rightPane, getSettings().game.noLowHpSound,
            {
                .key = tr_str("settings.audio.no_low_hp_sound"),
                .helpText = tr_str("settings.audio.no_low_hp_sound_help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().game.midnasLamentNonStop,
            {
                .key = tr_str("settings.audio.midnas_lament"),
                .helpText = tr_str("settings.audio.midnas_lament_help"),
            });
    });

    add_tab([] { return tr_str("settings.tab.gameplay"); }, [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                             const Rml::String& helpText) {
            config_bool_select(leftPane, rightPane, value,
                {
                    .key = key,
                    .helpText = helpText,
                });
        };
        auto addSpeedrunDisabledOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                                             const Rml::String& helpText) {
            add_speedrun_disabled_option(leftPane, rightPane, value, key, helpText);
        };

        leftPane.add_section(tr_str("settings.section.general"));
        addOption(tr_str("settings.gameplay.mirror_mode"), getSettings().game.enableMirrorMode,
            tr_str("settings.gameplay.mirror_mode_help"));
        addOption(tr_str("settings.gameplay.minimal_hud"), getSettings().game.minimalHUD,
            tr_str("settings.gameplay.minimal_hud_help"));
        config_percent_select(leftPane, rightPane, getSettings().game.hudScale,
            tr_str("settings.gameplay.hud_scale"),
            tr_str("settings.gameplay.hud_scale_help"),
            50, 200, 5,
            [] { return getSettings().game.minimalHUD.getValue(); });
        addOption(tr_str("settings.gameplay.restore_wii_glitches"), getSettings().game.restoreWiiGlitches,
            tr_str("settings.gameplay.restore_wii_glitches_help"));
        addOption(tr_str("settings.gameplay.rotating_link_doll"), getSettings().game.enableLinkDollRotation,
            tr_str("settings.gameplay.rotating_link_doll_help"));
        addOption(tr_str("settings.gameplay.hide_owl_markers"), getSettings().game.removeQuestMapMarkers,
            tr_str("settings.gameplay.hide_owl_markers_help"));

        leftPane.add_section(tr_str("settings.section.difficulty"));
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = tr_str("settings.gameplay.damage_multiplier"),
                .getValue = [] { return getSettings().game.damageMultiplier.getValue(); },
                .setValue =
                    [](int value) {
                        getSettings().game.damageMultiplier.setValue(value);
                        config::save();
                    },
                .isDisabled = [] { return speedrun::isActive(); },
                .isModified =
                    [] {
                        return getSettings().game.damageMultiplier.getValue() !=
                               getSettings().game.damageMultiplier.getDefaultValue();
                    },
                .min = 1,
                .max = 8,
                .suffix = "×",
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(tr_str("settings.gameplay.damage_multiplier_help"));
            });
        addSpeedrunDisabledOption(tr_str("settings.gameplay.instant_death"),
            getSettings().game.instantDeath, tr_str("settings.gameplay.instant_death_help"));
        addSpeedrunDisabledOption(tr_str("settings.gameplay.no_heart_drops"),
            getSettings().game.noHeartDrops, tr_str("settings.gameplay.no_heart_drops_help"));

        leftPane.add_section(tr_str("settings.section.quality_of_life"));
        addOption(tr_str("settings.gameplay.bigger_wallets"), getSettings().game.biggerWallets,
            tr_str("settings.gameplay.bigger_wallets_help"));
        addOption(tr_str("settings.gameplay.disable_rupee_cutscenes"),
            getSettings().game.disableRupeeCutscenes,
            tr_str("settings.gameplay.disable_rupee_cutscenes_help"));
        addSpeedrunDisabledOption(tr_str("settings.gameplay.faster_transitions"),
            getSettings().game.fastTransitions, tr_str("settings.gameplay.faster_transitions_help"));
        addOption(tr_str("settings.gameplay.faster_climbing"), getSettings().game.fastClimbing,
            tr_str("settings.gameplay.faster_climbing_help"));
        addOption(tr_str("settings.gameplay.faster_tears"), getSettings().game.fastTears,
            tr_str("settings.gameplay.faster_tears_help"));
        addSpeedrunDisabledOption(tr_str("settings.gameplay.autosave"), getSettings().game.autoSave,
            tr_str("settings.gameplay.autosave_help"));
        addOption(tr_str("settings.gameplay.instant_saves"), getSettings().game.instantSaves,
            tr_str("settings.gameplay.instant_saves_help"));
        addOption(tr_str("settings.gameplay.instant_text"), getSettings().game.instantText,
            tr_str("settings.gameplay.instant_text_help"));
        addSpeedrunDisabledOption(tr_str("settings.gameplay.hold_to_mash"), getSettings().game.holdToMash,
            tr_str("settings.gameplay.hold_to_mash_help"));
        addOption(tr_str("settings.gameplay.no_miss_climbing"), getSettings().game.noMissClimbing,
            tr_str("settings.gameplay.no_miss_climbing_help"));
        addOption(tr_str("settings.gameplay.no_rupee_returns"), getSettings().game.noReturnRupees,
            tr_str("settings.gameplay.no_rupee_returns_help"));
        addOption(tr_str("settings.gameplay.no_sword_recoil"), getSettings().game.noSwordRecoil,
            tr_str("settings.gameplay.no_sword_recoil_help"));
        addOption(tr_str("settings.gameplay.no_2nd_fish"), getSettings().game.no2ndFishForCat,
            tr_str("settings.gameplay.no_2nd_fish_help"));
        addOption(tr_str("settings.gameplay.button_fishing"), getSettings().game.buttonFishing,
            tr_str("settings.gameplay.button_fishing_help"));
        addOption(tr_str("settings.gameplay.poe_count_map"), getSettings().game.enhancedMapMenus,
            tr_str("settings.gameplay.poe_count_map_help"));
        addSpeedrunDisabledOption(tr_str("settings.gameplay.suns_song"), getSettings().game.sunsSong,
            tr_str("settings.gameplay.suns_song_help"));
        addOption(tr_str("settings.gameplay.quick_transform"), getSettings().game.enableQuickTransform,
            tr_str("settings.gameplay.quick_transform_help"));
        addOption(tr_str("settings.gameplay.aiming_reticle"), getSettings().game.aimingReticle,
            tr_str("settings.gameplay.aiming_reticle_help"));

        leftPane.add_section(tr_str("settings.section.speedrunning"));
        config_bool_select(leftPane, rightPane, getSettings().game.speedrunMode,
            {
                .key = tr_str("settings.gameplay.speedrun_mode"),
                .helpText = tr_str("settings.gameplay.speedrun_mode_help"),
                .onChange =
                    [this](bool enabled) {
                        if (enabled) {
                            speedrun::registerSpeedrunGameMode();
                        } else {
                            if (speedrun::isActive()) {
                                pop();
                            }
                            speedrun::unregisterSpeedrunGameMode();
                        }
                        MenuBar::refresh_tabs();
                    },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.liveSplitEnabled,
            {
                .key = tr_str("settings.gameplay.livesplit"),
                .helpText = tr_str("settings.gameplay.livesplit_help"),
                .onChange =
                    [](bool enabled) {
                        if (enabled) {
                            speedrun::connectLiveSplit();
                        } else {
                            speedrun::disconnectLiveSplit();
                        }
                    },
                .isDisabled = [] { return IsMobile || !speedrun::isActive(); },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showSpeedrunRTATimer,
            {
                .key = tr_str("settings.gameplay.show_rta"),
                .helpText = tr_str("settings.gameplay.show_rta_help"),
                .isDisabled = [] { return !speedrun::isActive(); },
            });
    });

    add_tab([] { return tr_str("settings.tab.cheats"); }, [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addCheat = [&](const Rml::String& key, ConfigVar<bool>& value,
                            const Rml::String& helpText) {
            add_speedrun_disabled_option(leftPane, rightPane, value, key, helpText);
        };

        leftPane.add_section(tr_str("settings.section.resources"));
        addCheat(tr_str("settings.cheats.infinite_hearts"), getSettings().game.infiniteHearts,
            tr_str("settings.cheats.infinite_hearts_help"));
        addCheat(tr_str("settings.cheats.infinite_arrows"), getSettings().game.infiniteArrows,
            tr_str("settings.cheats.infinite_arrows_help"));
        addCheat(tr_str("settings.cheats.infinite_seeds"), getSettings().game.infiniteSeeds,
            tr_str("settings.cheats.infinite_seeds_help"));
        addCheat(tr_str("settings.cheats.infinite_bombs"), getSettings().game.infiniteBombs,
            tr_str("settings.cheats.infinite_bombs_help"));
        addCheat(tr_str("settings.cheats.infinite_oil"), getSettings().game.infiniteOil,
            tr_str("settings.cheats.infinite_oil_help"));
        addCheat(tr_str("settings.cheats.infinite_oxygen"), getSettings().game.infiniteOxygen,
            tr_str("settings.cheats.infinite_oxygen_help"));
        addCheat(tr_str("settings.cheats.infinite_rupees"), getSettings().game.infiniteRupees,
            tr_str("settings.cheats.infinite_rupees_help"));
        addCheat(tr_str("settings.cheats.no_item_timer"), getSettings().game.enableIndefiniteItemDrops,
            tr_str("settings.cheats.no_item_timer_help"));

        leftPane.add_section(tr_str("settings.section.abilities"));

        addCheat(tr_str("settings.cheats.moon_jump"), getSettings().game.moonJump,
            tr_str("settings.cheats.moon_jump_help"));
        addCheat(tr_str("settings.cheats.easy_quick_spin"), getSettings().game.easyQuickSpin,
            tr_str("settings.cheats.easy_quick_spin_help"));

        addCheat(tr_str("settings.cheats.super_clawshot"), getSettings().game.superClawshot,
            tr_str("settings.cheats.super_clawshot_help"));
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.cheats.always_greatspin"),
                .getValue =
                    [] {
                        return tr_str(kAlwaysGreatspinModeKeys[static_cast<u8>(
                            getSettings().game.alwaysGreatspin.getValue())]);
                    },
                .isDisabled = [] { return dusk::speedrun::isActive(); },
                .isModified =
                    [] {
                        return getSettings().game.alwaysGreatspin.getValue() !=
                               getSettings().game.alwaysGreatspin.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kAlwaysGreatspinModeKeys.size()); i++) {
                    pane.add_button({
                            .text = tr_str(kAlwaysGreatspinModeKeys[i]),
                            .isSelected =
                                [i] {
                                    return getSettings().game.alwaysGreatspin.getValue() ==
                                           static_cast<AlwaysGreatspinMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.alwaysGreatspin.setValue(
                                static_cast<AlwaysGreatspinMode>(i));
                            config::save();
                        });
                }
                pane.add_rml(tr_str("settings.cheats.always_greatspin_help"));
            });
        addCheat(tr_str("settings.cheats.fast_iron_boots"), getSettings().game.enableFastIronBoots,
            tr_str("settings.cheats.fast_iron_boots_help"));
        addCheat(tr_str("settings.cheats.transform_anywhere"), getSettings().game.canTransformAnywhere,
            tr_str("settings.cheats.transform_anywhere_help"));
        addCheat(tr_str("settings.cheats.fast_roll"), getSettings().game.fastRoll,
            tr_str("settings.cheats.fast_roll_help"));
        addCheat(tr_str("settings.cheats.fast_spinner"), getSettings().game.fastSpinner,
            tr_str("settings.cheats.fast_spinner_help"));
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.cheats.magic_armor"),
                .getValue =
                    [] {
                        return tr_str(kMagicArmorModeKeys[static_cast<u8>(
                            getSettings().game.armorRupeeDrain.getValue())]);
                    },
                .isDisabled = [] { return speedrun::isActive(); },
                .isModified =
                    [] {
                        return getSettings().game.armorRupeeDrain.getValue() !=
                               getSettings().game.armorRupeeDrain.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kMagicArmorModeKeys.size()); i++) {
                    pane.add_button({
                            .text = tr_str(kMagicArmorModeKeys[i]),
                            .isSelected =
                                [i] {
                                    return getSettings().game.armorRupeeDrain.getValue() == static_cast<MagicArmorMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.armorRupeeDrain.setValue(static_cast<MagicArmorMode>(i));
                            config::save();
                        });
                }
                pane.add_rml(tr_str("settings.cheats.magic_armor_help"));
            });
        addCheat(tr_str("settings.cheats.invincible_enemies"), getSettings().game.invincibleEnemies,
            tr_str("settings.cheats.invincible_enemies_help"));
    });

    add_tab([] { return tr_str("settings.tab.interface"); }, [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(tr_str("settings.section.dusklight"));
#if DUSK_CAN_OPEN_DATA_FOLDER
        leftPane.register_control(
            leftPane.add_button(tr_str("settings.interface.open_data_folder")).on_pressed([] {
                mDoAud_seStartMenu(kSoundClick);
                data::open_data_path();
            }),
            rightPane, [](Pane& pane) {
                pane.add_text(tr_str("settings.interface.open_data_folder_help"));
            });
#endif
        leftPane.register_control(leftPane.add_button(tr_str("settings.interface.restart_main_menu")).on_pressed([this] {
            mDoAud_seStartMenu(kSoundClick);
            pop();
            prelaunch_state().returnToPrelaunchOnReset = true;
            JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
        }),
            rightPane, [](Pane& pane) {
                pane.add_text(tr_str("settings.interface.restart_main_menu_help"));
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.interface.notifications"),
                .getValue = [] {
                    const bool ach = getSettings().game.enableAchievementToasts.getValue();
                    const bool ctl = getSettings().game.enableControllerToasts.getValue();
                    if (!ach && !ctl) {
                        return tr_str("common.off");
                    }
                    if (ach && ctl) {
                        return tr_str("common.all");
                    }
                    return tr_str("common.some");
                },
                .isModified = [] {
                    const auto& ach = getSettings().game.enableAchievementToasts;
                    const auto& ctl = getSettings().game.enableControllerToasts;
                    return ach.getValue() != ach.getDefaultValue() || ctl.getValue() != ctl.getDefaultValue();
                },
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_button(tr_str("common.select_all")).on_pressed([] {
                    mDoAud_seStartMenu(kSoundItemChange);
                    getSettings().game.enableAchievementToasts.setValue(true);
                    getSettings().game.enableControllerToasts.setValue(true);
                    config::save();
                });
                pane.add_button(tr_str("common.select_none")).on_pressed([] {
                    mDoAud_seStartMenu(kSoundItemChange);
                    getSettings().game.enableAchievementToasts.setValue(false);
                    getSettings().game.enableControllerToasts.setValue(false);
                    config::save();
                });

                pane.add_section(tr_str("settings.section.types"));
                pane.add_button(
                    {
                        .text = tr_str("settings.interface.achievements"),
                        .isSelected =
                        [] {
                            return getSettings().game.enableAchievementToasts.getValue();
                        },
                    })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        auto& v = getSettings().game.enableAchievementToasts;
                        v.setValue(!v.getValue());
                        config::save();
                    });
                pane.add_button(
                    {
                        .text = tr_str("settings.interface.missing_device"),
                        .isSelected =
                            [] { return getSettings().game.enableControllerToasts.getValue(); },
                    })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        auto& v = getSettings().game.enableControllerToasts;
                        v.setValue(!v.getValue());
                        config::save();
                    });
                pane.add_rml(tr_str("settings.interface.notifications_help"));
            });
#if BOREALIS_HAS_SENTRY
        auto& crashReporting = leftPane.add_child<BoolButton>(BoolButton::Props{
            .key = tr_str("settings.interface.crash_reporting"),
            .getValue =
                [] { return borealis::sentry::get_consent() == borealis::sentry::Consent::Given; },
            .setValue = [](bool enabled) { borealis::sentry::set_consent(enabled); },
            .isDisabled =
                [] {
                    return borealis::sentry::get_consent() ==
                           borealis::sentry::Consent::Unavailable;
                },
            .isModified = [] { return false; },
        });
        leftPane.register_control(crashReporting, rightPane, [](Pane& pane) {
            pane.clear();
            pane.add_rml(tr_str("settings.interface.crash_reporting_help"));
        });
#endif
        config_bool_select(leftPane, rightPane, getSettings().backend.skipPreLaunchUI,
            {
                .key = tr_str("settings.interface.skip_main_menu"),
                .helpText = tr_str("settings.interface.skip_main_menu_help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().backend.showPipelineCompilation,
            {
                .key = tr_str("settings.interface.shader_compilation"),
                .helpText = tr_str("settings.interface.shader_compilation_help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().backend.checkForUpdates,
            {
                .key = tr_str("settings.interface.check_updates"),
                .helpText = tr_str("settings.interface.check_updates_help"),
            });
#if BOREALIS_HAS_DISCORD
        config_bool_select(leftPane, rightPane, getSettings().game.enableDiscordPresence,
            {
                .key = tr_str("settings.interface.discord_presence"),
                .helpText = tr_str("settings.interface.discord_presence_help"),
                .onChange = [](bool enabled) {
                    if (enabled) {
                        discord::initialize();
                    } else {
                        discord::shutdown();
                    }
                },
            });
#endif
        config_bool_select(leftPane, rightPane, getSettings().backend.enableAdvancedSettings,
            {
                .key = tr_str("settings.interface.advanced_settings"),
                .icon = "warning",
                .helpText = tr_str("settings.interface.advanced_settings_help"),
                .onChange = [](bool) { MenuBar::refresh_tabs(); },
                .isDisabled = [] { return speedrun::isActive(); },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewer,
            {
                .key = tr_str("settings.interface.input_viewer"),
                .helpText = tr_str("settings.interface.input_viewer_help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewerGyro,
            {
                .key = tr_str("settings.interface.gyro_input_viewer"),
                .helpText = tr_str("settings.interface.gyro_input_viewer_help"),
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        leftPane.add_section(tr_str("settings.section.game_section"));
        leftPane.register_control(
            leftPane.add_select_button({
                .key = tr_str("settings.interface.menu_scaling"),
                .getValue =
                    [] {
                        return tr_str(kMenuScalingModeKeys[static_cast<u8>(
                            getSettings().game.menuScalingMode.getValue())]);
                    },
                .isModified =
                    [] {
                        const auto& mode = getSettings().game.menuScalingMode;
                        return mode.getValue() != mode.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kMenuScalingModeKeys.size()); ++i) {
                    pane
                        .add_button({
                            .text = tr_str(kMenuScalingModeKeys[i]),
                            .isSelected =
                                [i] {
                                    return getSettings().game.menuScalingMode.getValue() ==
                                           static_cast<MenuScaling>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.menuScalingMode.setValue(
                                static_cast<MenuScaling>(i));
                            config::save();
                        });
                }
                pane.add_rml(tr_str("settings.interface.menu_scaling_help"));
            });
        config_bool_select(leftPane, rightPane, getSettings().game.hideTvSettingsScreen,
            {
                .key = tr_str("settings.interface.skip_tv_settings"),
                .helpText = tr_str("settings.interface.skip_tv_settings_help"),
            });
        add_speedrun_disabled_option(leftPane, rightPane, getSettings().game.recordingMode,
            tr_str("settings.interface.recording_mode"),
            tr_str("settings.interface.recording_mode_help"));
    });

    add_tab([] { return tr_str("settings.tab.tools"); }, [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(tr_str("settings.section.link"));
        add_speedrun_disabled_option(leftPane, rightPane, getSettings().game.enableMoveLinkCombo,
            tr_str("settings.tools.move_link"),
            tr_str("settings.tools.move_link_help"));
        add_speedrun_disabled_option(leftPane, rightPane, getSettings().game.enableTeleportCombo,
            tr_str("settings.tools.teleport"),
            tr_str("settings.tools.teleport_help"));
    });
}

void SettingsWindow::update() {
    if (mPrelaunch && top_document() == this) {
        try_push_verification_modal(*this);
        try_push_language_unavailable_modal(*this);
    }

    Window::update();
}

void SettingsWindow::hide(bool close) {
    config::save();
    Window::hide(close);
}

}  // namespace dusk::ui
