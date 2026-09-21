#include "preset.hpp"

#include "button.hpp"
#include "dusk/config.hpp"
#include "i18n.hpp"
#include "dusk/settings.h"
#include "ui.hpp"

#include <dolphin/gx/GXAurora.h>

namespace dusk::ui {
namespace {
using i18n::tr_str;

void applyPresetClassic() {
    auto& s = getSettings();
    s.video.lockAspectRatio.setValue(true);
    s.game.bloomMode.setValue(BloomMode::Classic);
    s.game.depthOfFieldMode.setValue(DepthOfFieldMode::Classic);
    s.game.enableAchievementToasts.setValue(false);
    s.game.enableControllerToasts.setValue(false);
    s.game.internalResolutionScale.setValue(1);
    s.game.shadowResolutionMultiplier.setValue(1);
    s.game.hideTvSettingsScreen.setValue(false);
    s.game.menuScalingMode.setValue(MenuScaling::GameCube);
    s.game.enableMenuPointer.setValue(false);
    AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
}

void applyPresetDusk() {
    auto& s = getSettings();
    s.game.hideTvSettingsScreen.setValue(true);
    s.game.noReturnRupees.setValue(true);
    s.game.disableRupeeCutscenes.setValue(true);
    s.game.noSwordRecoil.setValue(true);
    s.game.fastClimbing.setValue(true);
    s.game.noMissClimbing.setValue(true);
    s.game.fastTears.setValue(true);
    s.game.biggerWallets.setValue(true);
    s.game.invertCameraXAxis.setValue(true);
    s.game.invertFirstPersonYAxis.setValue(true);
    s.game.no2ndFishForCat.setValue(true);
    s.game.buttonFishing.setValue(true);
    s.game.enableAchievementToasts.setValue(true);
    s.game.enableControllerToasts.setValue(true);
    s.game.enableQuickTransform.setValue(true);
    s.game.instantSaves.setValue(true);
    s.game.midnasLamentNonStop.setValue(true);
    s.game.enableFrameInterpolation.setValue(FrameInterpMode::Unlimited);
    s.game.sunsSong.setValue(true);
    s.game.bloomMode.setValue(BloomMode::Dusk);
    s.game.depthOfFieldMode.setValue(DepthOfFieldMode::Dusk);
    s.game.internalResolutionScale.setValue(0);
    s.game.shadowResolutionMultiplier.setValue(4);
    s.game.enableGyroAim.setValue(true);
    s.game.autoSave.setValue(true);
    s.game.menuScalingMode.setValue(MenuScaling::Dusklight);
    s.game.enhancedMapMenus.setValue(true);
    s.game.enableMenuPointer.setValue(true);
}

}  // namespace

PresetWindow::PresetWindow() : WindowSmall("modal") {
    auto* header = append(mDialog, "modal-header");

    auto* title = append(header, "modal-title");
    append_text(title, tr_str("preset.title"));

    auto* headIcon = append(header, "icon");
    headIcon->SetClass("celebration", true);

    auto* intro = append(mDialog, "modal-body");
    append_text(intro, tr_str("preset.intro"));

    auto* grid = append(mDialog, "preset-grid");

    struct PresetInfo {
        const char* nameKey;
        const char* descKey;
        void (*apply)();
    };

    static constexpr PresetInfo kPresets[] = {
        {
            "preset.classic",
            "preset.classic_desc",
            applyPresetClassic,
        },
        {
            "preset.dusklight",
            "preset.dusklight_desc",
            applyPresetDusk,
        },
    };

    for (const auto& preset : kPresets) {
        auto* col = append(grid, "preset-option");

        auto btn = std::make_unique<Button>(col, tr_str(preset.nameKey));
        btn->on_nav_command([this, apply = preset.apply](Rml::Event&, NavCommand cmd) {
            if (cmd == NavCommand::Confirm) {
                apply();
                getSettings().backend.wasPresetChosen.setValue(true);
                config::save();
                hide(true);
                mDoAud_seStartMenu(kSoundClick);
                return true;
            }
            return false;
        });
        mButtons.push_back(std::move(btn));

        auto* desc = append(col, "preset-description");
        append_text(desc, tr_str(preset.descKey));
    }
}

bool PresetWindow::focus() {
    if (!mButtons.empty()) {
        return mButtons.back()->focus();
    }
    return false;
}

bool PresetWindow::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    if (cmd == NavCommand::Cancel || cmd == NavCommand::Menu) {
        return true;
    }
    int direction = 0;
    if (cmd == NavCommand::Left) {
        direction = -1;
    } else if (cmd == NavCommand::Right) {
        direction = 1;
    } else {
        return false;
    }
    auto* target = event.GetTargetElement();
    for (int i = 0; i < static_cast<int>(mButtons.size()); ++i) {
        if (mButtons[i]->contains(target)) {
            const int next = i + direction;
            if (next >= 0 && next < static_cast<int>(mButtons.size())) {
                if (mButtons[next]->focus()) {
                    mDoAud_seStartMenu(kSoundItemFocus);
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

}  // namespace dusk::ui
