#if BOREALIS_HAS_SENTRY

#include "reporting.hpp"

#include "button.hpp"
#include "ui.hpp"

#include <borealis/sentry.hpp>
#include <dolphin/gx/GXAurora.h>

namespace dusk::ui {

CrashReportWindow::CrashReportWindow() : WindowSmall("modal") {
    auto* header = append(mDialog, "modal-header");

    auto* title = append(header, "modal-title");
    append_text(title, "Send Crash Reports");

    auto* headIcon = append(header, "icon");
    headIcon->SetClass("question-mark", true);

    auto* intro = append(mDialog, "modal-body");
    append_text(intro,
        "Dusklight can automatically send crash reports to the developers. Crash reports contain "
        "the following:");
    for (const char* item :
        {
            "• Operating system version",
            "• CPU architecture",
            "• GPU model & driver version",
            "• File paths (may include account username)",
            "• Stack trace",
        })
    {
        append(intro, "br");
        append_text(intro, item);
    }
    append(intro, "br");
    append(intro, "br");
    append_text(intro, "This can be changed in the Settings menu at any time.");

    auto* grid = append(mDialog, "preset-grid");

    struct OptionInfo {
        const char* name;
        const char* desc;
        void (*apply)();
    };

    static constexpr OptionInfo kOptions[] = {
        {"Enable",
            "Send crash reports to Dusklight developers. Reports will include the information "
            "described above.",
            [] { borealis::sentry::set_consent(true); }},
        {"Disable",
            "Do not send crash reports. This may make it more difficult to resolve issues you "
            "encounter.",
            [] { borealis::sentry::set_consent(false); }},
    };

    for (const auto& option : kOptions) {
        auto* col = append(grid, "preset-option");

        auto btn = std::make_unique<Button>(col, Rml::String(option.name));
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
        append_text(desc, option.desc);
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
