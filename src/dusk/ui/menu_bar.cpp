#include "menu_bar.hpp"

#include "achievements.hpp"
#include "editor.hpp"
#include "mod_updates.hpp"
#include "mods_window.hpp"
#include "prelaunch.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include "warp.hpp"

#include "dusk/game_mode.hpp"
#include "dusk/livesplit.h"
#include "dusk/main.h"
#include "dusk/mods/svc/ui.hpp"
#include "dusk/settings.h"
#include "dusk/speedrun.h"

#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <RmlUi/Core.h>
#include <borealis/ui/modal.hpp>
#include <borealis/ui/window.hpp>

namespace dusk::ui {

MenuBar::MenuBar() : borealis::ui::MenuBar(Props{.styleSheets = {"res/rml/mod_common.rcss"}}) {
    build_tabs();
}

void MenuBar::build_tabs() {
    mTabBar->add_tab("Settings", [this] { push(std::make_unique<SettingsWindow>()); });

    if (getSettings().backend.enableAdvancedSettings) {
        mTabBar->add_tab("Warp", [this] { push(std::make_unique<WarpWindow>()); });
        mTabBar->add_tab("Editor", [this] { push(std::make_unique<EditorWindow>()); });
    }

    // Only allow us to access achievements if we are playing on a game mode that uses them
    if (gamemode::getGameModeManager().isCurrentGameMode(gamemode::kVanillaGameModeId) ||
        gamemode::getGameModeManager().isCurrentGameMode(speedrun::kSpeedrunGameModeId))
    {
        mTabBar->add_tab("Achievements", [this] { push(std::make_unique<AchievementsWindow>()); });
    }
    mModsButton = &mTabBar->add_tab("Mods", [this] { push(std::make_unique<ModsWindow>()); });
    for (auto& tab : mods::svc::ui_mod_menu_tabs()) {
        mTabBar->add_tab(tab.label, std::move(tab.onSelected));
    }

    mTabBar->add_tab("Reset", [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal& modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props{
            .title = "Reset Game",
            .bodyRml = "Unsaved progress will be lost.<br/>"
                       "<modal-tip>Tip: You can also reset by holding Start+X+B</modal-tip>",
            .actions =
                {
                    ModalAction{
                        .label = "Cancel",
                        .onPressed =
                            [this, dismiss](Modal& modal) {
                                play_nav_sound(NavSound::WindowClose);
                                dismiss(modal);
                            },
                    },
                    ModalAction{
                        .label = "Reset",
                        .onPressed =
                            [this, dismiss](Modal& modal) {
                                play_nav_sound(NavSound::Click);
                                if (fpcM_SearchByName(fpcNm_LOGO_SCENE_e)) {
                                    dismiss(modal);
                                    return;
                                }
                                dismiss(modal);
                                if (gamemode::getGameModeManager().getRegisteredGameModes().size() >
                                    1) {
                                    // If game modes are registered, return to prelaunch on reset.
                                    prelaunch_state().returnToPrelaunchOnReset = true;
                                }
                                hide(false);
                                JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
                            },
                    },
                },
            .onDismiss = dismiss,
            .icon = "question-mark",
        }));
    });
    mTabBar->add_tab("Quit", [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal& modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props{
            .title = "Quit Dusklight",
            .bodyText = "Unsaved progress will be lost.",
            .actions =
                {
                    ModalAction{
                        .label = "Cancel",
                        .onPressed =
                            [dismiss](Modal& modal) {
                                play_nav_sound(NavSound::WindowClose);
                                dismiss(modal);
                            },
                    },
                    ModalAction{
                        .label = "Quit",
                        .onPressed =
                            [dismiss](Modal& modal) {
                                play_nav_sound(NavSound::Click);
                                dismiss(modal);
                                IsRunning = false;
                            },
                    },
                },
            .onDismiss = dismiss,
            .icon = "question-mark",
        }));
    });

    if (speedrun::isActive()) {
        mTabBar->add_tab("Reset Run", [this] {
            mTabBar->set_active_tab(-1);
            play_nav_sound(NavSound::Click);
            speedrun::g_speedrunInfo.reset();
            speedrun::reset();
            JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
            hide(false);
        });
    }
}

void MenuBar::update() {
    if (mModsButton) {
        set_mod_update_badge(*mModsButton);
    }
    borealis::ui::MenuBar::update();
}

bool MenuBar::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    if (!getSettings().backend.wasPresetChosen) {
        return true;
    }
    return borealis::ui::MenuBar::handle_nav_command(event, cmd);
}

}  // namespace dusk::ui
