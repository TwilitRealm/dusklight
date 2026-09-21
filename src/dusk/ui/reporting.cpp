#if BOREALIS_HAS_SENTRY

#include "reporting.hpp"

#include "button.hpp"
#include "i18n.hpp"
#include "ui.hpp"

#include <borealis/sentry.hpp>
#include <dolphin/gx/GXAurora.h>

namespace dusk::ui {
namespace {
using i18n::tr_str;
}  // namespace

CrashReportWindow::CrashReportWindow() : WindowSmall("modal") {
    auto* header = append(mDialog, "modal-header");

    auto* title = append(header, "modal-title");
    append_text(title, tr_str("reporting.title"));

    auto* headIcon = append(header, "icon");
    headIcon->SetClass("question-mark", true);

    auto* intro = append(mDialog, "modal-body");
    append_text(intro, tr_str("reporting.intro"));
    for (const char* item :
        {
            "reporting.item_os",
            "reporting.item_cpu",
            "reporting.item_gpu",
            "reporting.item_paths",
            "reporting.item_stack",
        })
    {
        append(intro, "br");
        append_text(intro, tr_str(item));
    }
    append(intro, "br");
    append(intro, "br");
    append_text(intro, tr_str("reporting.change_anytime"));

    auto* grid = append(mDialog, "preset-grid");

    struct OptionInfo {
        const char* nameKey;
        const char* descKey;
        void (*apply)();
    };

    static constexpr OptionInfo kOptions[] = {
        {"reporting.enable", "reporting.enable_desc",
            [] { borealis::sentry::set_consent(true); }},
        {"reporting.disable", "reporting.disable_desc",
            [] { borealis::sentry::set_consent(false); }},
    };

    for (const auto& option : kOptions) {
        auto* col = append(grid, "preset-option");

        auto btn = std::make_unique<Button>(col, tr_str(option.nameKey));
        btn->on_nav_command([this, apply = option.apply](Rml::Event&, NavCommand cmd) {
            if (cmd == NavCommand::Confirm) {
                apply();
                hide(true);
                mDoAud_seStartMenu(kSoundClick);
                return true;
            }
            return false;
        });
        mButtons.push_back(std::move(btn));

        auto* desc = append(col, "preset-description");
        append_text(desc, tr_str(option.descKey));
    }
}

bool CrashReportWindow::focus() {
    if (!mButtons.empty()) {
        return mButtons.back()->focus();
    }
    return false;
}

bool CrashReportWindow::handle_nav_command(Rml::Event& event, NavCommand cmd) {
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

#endif
