#include "dusk/gamepad_color.h"

#include "ui/controller_config.hpp"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_alink.h"

#include <SSystem/SComponent/c_xyz.h>

#include <pad.h>

#include <cmath>
#include <algorithm>
#include <array>

namespace dusk::input {

namespace {
    std::array<cXyz, PAD_MAX_CONTROLLERS> currentColor{};

    enum ColorVariations : u8 {
        NO_COLOR = 0,
        DUSK_COLOR = 1,
        ZHINT = 2,
        LINK_WOLF = 3,
        LINK_CASUAL = 4,
        LINK_KOKIRI = 5,
        LINK_ZORA = 6,
        LINK_MAGIC = 7,
        LINK_MAGIC_HEAVY = 8,
    };

    struct ColorSetting {
        cXyz color;
        float speed;
    };

    const ColorSetting kColorTable[] = {
        /* 0: NO_COLOR         */ { {0, 0, 0},       2.0f },
        /* 1: DUSK_COLOR       */ { {50, 50, -50},   2.0f },
        /* 2: ZHINT            */ { {50, 50, 175},   2.0f },
        /* 3: LINK_WOLF        */ { {115, 115, 75},  5.0f },
        /* 4: LINK_CASUAL      */ { {235, 230, 115}, 5.0f },
        /* 5: LINK_KOKIRI      */ { {0, 100, 0},     5.0f },
        /* 6: LINK_ZORA        */ { {0, 0, 100},     5.0f },
        /* 7: LINK_MAGIC       */ { {100, 0, 5},     5.0f },
        /* 8: LINK_MAGIC_HEAVY */ { {5, 100, 100},   5.0f },
    };

    cXyz LerpColor(const cXyz a, const cXyz b, const float t) {
        return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
    }

    void clamp(cXyz& color) {
        color.x = std::clamp(color.x, 0.0f, 255.0f);
        color.y = std::clamp(color.y, 0.0f, 255.0f);
        color.z = std::clamp(color.z, 0.0f, 255.0f);
    }

    ColorSetting getColorSetting() {
        const fopAc_ac_c* zHint = dComIfGp_att_getZHint();
        if (zHint != nullptr) {
            return kColorTable[ZHINT];
        }

        const daAlink_c* link = daAlink_getAlinkActorClass();

        if (link == nullptr)
            return kColorTable[DUSK_COLOR];

        if (link->checkWolf()) {
            return kColorTable[LINK_WOLF];
        }

        switch (dComIfGs_getSelectEquipClothes()) {
        case dItemNo_WEAR_KOKIRI_e:
            return kColorTable[LINK_KOKIRI];
        case dItemNo_WEAR_ZORA_e:
            return kColorTable[LINK_ZORA];
        case dItemNo_ARMOR_e:
            if (link->checkMagicArmorHeavy()) {
                return kColorTable[LINK_MAGIC_HEAVY];
            }

            return kColorTable[LINK_MAGIC];
        case dItemNo_WEAR_CASUAL_e:
            return kColorTable[LINK_CASUAL];
        default:
            return kColorTable[LINK_KOKIRI];
        }
    }

    cXyz getAdditionalColor() {
        if (dKy_darkworld_check()) {
            return kColorTable[DUSK_COLOR].color;
        }

        return kColorTable[NO_COLOR].color;
    }

    cXyz getColorFromHP() {
        // HP in quarter-heart units (pieces)
        const int currentHp = dComIfGs_getLife();
        const int maxHp = dComIfGs_getMaxLifeGauge();

        // Normalize to [0, 1]
        float t = static_cast<float>(currentHp) / static_cast<float>(maxHp);
        t = std::clamp(t, 0.0f, 1.0f);

        const cXyz kRed = {255, 0, 0};   // 0% HP
        const cXyz kGreen = {0, 255, 0}; // 100% HP

        // Interpolate between red and green based on HP percentage
        return LerpColor(kRed, kGreen, t);
    }
}  // namespace

bool pad_has_led(const int port) noexcept {
    if (port >= PAD_MAX_CONTROLLERS || port < 0)
        return false;

    return PADHasLED(port);
}

void handleGamepadColor() {
    const ColorSetting gameStateColor = getColorSetting();
    const cXyz hpColor = getColorFromHP();
    const cXyz additionalColor = getAdditionalColor();
    const auto& settings = getSettings();

    for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
        const bool hasLed = pad_has_led(port);
        const auto ledMode = settings.game.ledStatusMode[port].getValue();

        if (!hasLed) {
            currentColor[port] = cXyz(0, 0, 0);
            continue;
        }

        switch (ledMode) {
        case LedStatusMode::OFF:
            currentColor[port] = cXyz(0, 0, 0);
            PADSetColor(port, 0, 0, 0);
            continue;
        case LedStatusMode::PLAYER_HP: {
            currentColor[port] = hpColor;
            break;
        }
        case LedStatusMode::GAME_STATE: {
            cXyz finalColor = gameStateColor.color + additionalColor;
            const float lerpSpeed = gameStateColor.speed / 30.0f;
            clamp(finalColor);
            currentColor[port] = LerpColor(currentColor[port], finalColor, lerpSpeed);
            break;
        }
        default:
            continue;
        }

        PADSetColor(
            port,
            static_cast<u8>(currentColor[port].x),
            static_cast<u8>(currentColor[port].y),
            static_cast<u8>(currentColor[port].z)
        );
    }
}

}  // namespace dusk::input
