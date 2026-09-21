#include "achievements.hpp"

#include "dusk/ui/i18n.hpp"
#include "nav_types.hpp"
#include "pane.hpp"

#include "dusk/achievements.h"

#include "m_Do/m_Do_audio.h"

#include <fmt/format.h>

namespace dusk::ui {

using i18n::tr;
using i18n::tr_str;

namespace {

struct CategoryInfo {
    AchievementCategory cat;
    const char* label;
};

constexpr AchievementCategory kCategories[] = {
    AchievementCategory::Challenge,
    AchievementCategory::Collection,
    AchievementCategory::Minigame,
    AchievementCategory::Misc,
    AchievementCategory::Glitched,
};

Rml::String category_label(AchievementCategory cat) {
    switch (cat) {
    case AchievementCategory::Challenge:
        return tr_str("achievements.category_challenge");
    case AchievementCategory::Collection:
        return tr_str("achievements.category_collection");
    case AchievementCategory::Minigame:
        return tr_str("achievements.category_minigame");
    case AchievementCategory::Misc:
        return tr_str("achievements.category_misc");
    case AchievementCategory::Glitched:
        return tr_str("achievements.category_glitched");
    default:
        return "?";
    }
}

void append_achievement_info(Rml::Element* parent, const Achievement& a) {
    auto* header = append(parent, "achievement-header");
    auto* name = append(header, "achievement-name");
    name->SetClass("unlocked", a.unlocked);
    append_text(name, tr_str("achievements.name_" + std::string(a.key)));
    auto* badge = append(header, "status-badge");
    badge->SetClass(a.unlocked ? "success" : "error", true);
    append_text(badge, a.unlocked ? tr_str("achievements.unlocked") : tr_str("achievements.locked"));

    auto* description = append(parent, "p");
    description->SetClass("achievement-desc", true);
    append_text(description, tr_str("achievements.desc_" + std::string(a.key)));
    if (a.isCounter) {
        const float fraction = a.goal > 0 ? float(a.progress) / float(a.goal) : 1.0f;
        auto* progress = append(parent, "progress");
        progress->SetAttribute("value", fraction);
        progress->SetClass(a.unlocked ? "success" : "info", true);
        append_text(
            append(parent, "achievement-progress"), fmt::format("{} / {}", a.progress, a.goal));
    }
}

class AchievementRow : public FluentComponent<AchievementRow> {
public:
    AchievementRow(Rml::Element* parent, const Achievement& a)
        : FluentComponent(createRowRoot(parent)) {
        auto& btn = add_child<Button>(Button::Props{"×"});
        mClearButton = &btn;
        btn.root()->SetClassNames("achievement-clear compact danger");

        btn.on_nav_command([this, key = std::string(a.key)](Rml::Event&, NavCommand cmd) {
            if (cmd == NavCommand::Confirm) {
                if (mConfirming) {
                    mDoAud_seStartMenu(kSoundClick);
                    AchievementSystem::get().clearOne(key.c_str());
                    resetConfirm();
                } else {
                    mConfirming = true;
                    mClearButton->set_text(tr_str("achievements.clear_question"));
                }
                return true;
            }
            if (cmd == NavCommand::Cancel && mConfirming) {
                resetConfirm();
                return true;
            }
            return false;
        });

        Component::listen(btn.root(), Rml::EventId::Blur, [this](Rml::Event&) { resetConfirm(); });

        auto* infoDiv = append(mRoot, "achievement-info");
        append_achievement_info(infoDiv, a);
    }

    bool focus() override { return mClearButton->focus(); }

private:
    static Rml::Element* createRowRoot(Rml::Element* parent) {
        auto* doc = parent->GetOwnerDocument();
        auto elem = doc->CreateElement("achievement-row");
        return parent->AppendChild(std::move(elem));
    }

    void resetConfirm() {
        mConfirming = false;
        mClearButton->set_text("×");
    }

    Button* mClearButton = nullptr;
    bool mConfirming = false;
};

}  // namespace

AchievementsWindow::AchievementsWindow() {
    const auto all = AchievementSystem::get().getAchievements();

    {
        auto elem = mDocument->CreateElement("achievement-total");
        mTotalEl = mRoot->AppendChild(std::move(elem));
        updateTotal();
    }

    for (const auto catInfo : kCategories) {
        int catTotal = 0;
        for (const auto& a : all) {
            if (a.category == catInfo) {
                ++catTotal;
            }
        }
        if (catTotal == 0) {
            continue;
        }

        add_tab([cat = catInfo] { return category_label(cat); },
            [this, cat = catInfo](Rml::Element* content) {
            const auto achievements = AchievementSystem::get().getAchievements();

            int total = 0, unlocked = 0;
            for (const auto& a : achievements) {
                if (a.category == cat) {
                    ++total;
                    if (a.unlocked) {
                        ++unlocked;
                    }
                }
            }

            auto& pane = add_child<Pane>(content, Pane::Type::Controlled);

            pane.add_section(fmt::format(fmt::runtime(tr_str("achievements.section_progress")),
                fmt::arg("unlocked", unlocked), fmt::arg("total", total)));

            for (const auto& a : achievements) {
                if (a.category != cat) {
                    continue;
                }
                pane.add_child<AchievementRow>(a);
            }

            pane.add_section(tr_str("common.actions"));

            auto& clearAllBtn = pane.add_button(tr_str("achievements.clear_all"));
            clearAllBtn.root()->SetClass("danger", true);
            auto* clearAllPtr = &clearAllBtn;
            auto confirmingAll = std::make_shared<bool>(false);

            clearAllBtn.on_nav_command([clearAllPtr, confirmingAll](Rml::Event&, NavCommand cmd) {
                if (cmd == NavCommand::Confirm) {
                    if (*confirmingAll) {
                        mDoAud_seStartMenu(kSoundClick);
                        AchievementSystem::get().clearAll();
                        *confirmingAll = false;
                        clearAllPtr->set_text(tr_str("achievements.clear_all"));
                    } else {
                        *confirmingAll = true;
                        clearAllPtr->set_text(tr_str("achievements.are_you_sure"));
                    }
                    return true;
                }
                if (cmd == NavCommand::Cancel && *confirmingAll) {
                    *confirmingAll = false;
                    clearAllPtr->set_text(tr_str("achievements.clear_all"));
                    return true;
                }
                return false;
            });
            clearAllBtn.listen(Rml::EventId::Blur, [clearAllPtr, confirmingAll](Rml::Event&) {
                *confirmingAll = false;
                clearAllPtr->set_text(tr_str("achievements.clear_all"));
            });
        });
    }
}

void AchievementsWindow::update() {
    const auto current = AchievementSystem::get().getAchievements();
    bool dirty = current.size() != mSnapshot.size();
    if (!dirty) {
        for (size_t i = 0; i < current.size(); ++i) {
            if (current[i].progress != mSnapshot[i].progress ||
                current[i].unlocked != mSnapshot[i].unlocked)
            {
                dirty = true;
                break;
            }
        }
    }
    if (dirty) {
        mSnapshot = current;
        refresh_active_tab();
        updateTotal();
    }
    Window::update();
}

void AchievementsWindow::updateTotal() {
    if (mTotalEl == nullptr) {
        return;
    }
    const auto all = AchievementSystem::get().getAchievements();
    const int total = std::count_if(all.begin(), all.end(), [](const Achievement& achievement) {
        return achievement.category != AchievementCategory::Glitched;
    });
    int unlocked = 0;
    for (const auto& a : all) {
        if (a.unlocked) {
            ++unlocked;
        }
    }
    const int pct = total > 0 ? (unlocked * 100 / total) : 0;
    set_text_content(mTotalEl, fmt::format("{}%", pct));
}

}  // namespace dusk::ui
