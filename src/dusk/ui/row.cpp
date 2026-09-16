#include "row.hpp"

#include "m_Do/m_Do_audio.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace dusk::ui {

Row::Row(Rml::Element* parent, Props props) : FluentComponent{append(parent, "ui-row")} {
    auto align = Rml::Style::JustifyContent::FlexStart;
    switch (props.align) {
    case Align::End:
        align = Rml::Style::JustifyContent::FlexEnd;
        break;
    case Align::Center:
        align = Rml::Style::JustifyContent::Center;
        break;
    case Align::SpaceBetween:
        align = Rml::Style::JustifyContent::SpaceBetween;
        break;
    default:
        break;
    }
    mRoot->SetProperty(Rml::PropertyId::JustifyContent, align);
    mRoot->SetProperty(Rml::PropertyId::FlexWrap,
        props.wrap ? Rml::Style::FlexWrap::Wrap : Rml::Style::FlexWrap::Nowrap);

    listen(Rml::EventId::Keydown, [this](Rml::Event& event) {
        const auto cmd = map_nav_event(event);
        if (cmd != NavCommand::Left && cmd != NavCommand::Right) {
            return;
        }
        const int step = cmd == NavCommand::Left ? -1 : 1;
        for (int i = 0; i < static_cast<int>(mChildren.size()); ++i) {
            if (!mChildren[i]->contains(event.GetTargetElement())) {
                continue;
            }
            for (i += step; i >= 0 && i < static_cast<int>(mChildren.size()); i += step) {
                if (mChildren[i]->focus_from(cmd)) {
                    mDoAud_seStartMenu(kSoundItemFocus);
                    event.StopPropagation();
                    return;
                }
            }
            return;
        }
    });
}

bool Row::focus() {
    if (disabled() || !mRoot->IsVisible(true)) {
        return false;
    }
    if (mSelected != nullptr && mSelected->root()->IsVisible(true) && mSelected->focus()) {
        return true;
    }
    for (const auto& child : mChildren) {
        if (child->root()->IsVisible(true) && child->focus()) {
            return true;
        }
    }
    return false;
}

bool Row::focus_from(NavCommand direction) {
    if (disabled() || !mRoot->IsVisible(true)) {
        return false;
    }
    std::vector<Component*> candidates;
    for (const auto& child : mChildren) {
        candidates.push_back(child.get());
    }
    if (direction == NavCommand::Left) {
        std::ranges::reverse(candidates);
    }
    for (auto* child : candidates) {
        if (child->root()->IsVisible(true) && child->focus_from(direction)) {
            return true;
        }
    }
    return false;
}

bool Row::selected() const {
    return std::ranges::any_of(mChildren, [](const auto& child) { return child->selected(); });
}

void Row::set_selected(bool value) {
    auto* focused = mRoot->GetContext()->GetFocusElement();
    if (value && contains(focused)) {
        for (const auto& child : mChildren) {
            if (child->contains(focused)) {
                mSelected = child.get();
            }
        }
    }
    for (const auto& child : mChildren) {
        child->set_selected(value && child.get() == mSelected);
    }
    if (!value) {
        mSelected = nullptr;
    }
}

}  // namespace dusk::ui
