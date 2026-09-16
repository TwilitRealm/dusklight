#include "dropdown_button.hpp"

#include "context_menu.hpp"
#include "icon_button.hpp"
#include "m_Do/m_Do_audio.h"

#include <algorithm>

namespace dusk::ui {

DropdownButton::DropdownButton(Rml::Element* parent, Props props)
    : BaseControlledSelectButton{parent, {.key = props.key, .submit = false}},
      mProps{std::move(props)} {
    mRoot->SetClass("dropdown-button", true);
    append_text(append(mRoot, "dropdown-indicator"), material_icon("expand_more"));
    update();
}

DropdownButton::~DropdownButton() {
    if (mMenu != nullptr) {
        mMenu->on_close(nullptr);
        mMenu->dismiss(false);
    }
}

void DropdownButton::update() {
    BaseControlledSelectButton::update();
    if (mMenu != nullptr && (disabled() || !mRoot->IsVisible(true))) {
        mMenu->dismiss();
    }
}

void DropdownButton::set_options(std::vector<Option> options) {
    if (mMenu != nullptr) {
        mMenu->dismiss();
    }
    mProps.options = std::move(options);
    update();
}

bool DropdownButton::modified() const {
    return mProps.isModified ? mProps.isModified() : BaseControlledSelectButton::modified();
}

bool DropdownButton::disabled() const {
    return !mProps.setValue || std::ranges::none_of(mProps.options, &Option::enabled) ||
           (mProps.isDisabled ? mProps.isDisabled() : BaseControlledSelectButton::disabled());
}

Rml::String DropdownButton::format_value() {
    const int index = mProps.getValue ? mProps.getValue() : -1;
    return index >= 0 && index < static_cast<int>(mProps.options.size()) ?
               mProps.options[index].text :
               Rml::String{};
}

bool DropdownButton::handle_nav_command(NavCommand cmd) {
    if (cmd == NavCommand::Confirm) {
        toggle_menu();
        return true;
    }
    return false;
}

void DropdownButton::toggle_menu() {
    if (mMenu != nullptr) {
        mMenu->dismiss();
        return;
    }

    const int selected = mProps.getValue ? mProps.getValue() : -1;
    std::vector<ContextMenu::Item> items;
    items.reserve(mProps.options.size());
    for (int index = 0; index < static_cast<int>(mProps.options.size()); ++index) {
        const auto& option = mProps.options[index];
        items.push_back({
            .text = option.text,
            .icon = index == selected ? "check" : "",
            .onPressed =
                [getValue = mProps.getValue, setValue = mProps.setValue, index] {
                    if (!getValue || getValue() != index) {
                        mDoAud_seStartMenu(kSoundItemChange);
                        setValue(index);
                    }
                },
            .enabled = option.enabled,
            .selected = index == selected,
        });
    }

    auto menu = std::make_unique<ContextMenu>(mRoot, std::move(items));
    menu->body()->SetProperty(Rml::PropertyId::MinWidth,
        Rml::Property{mRoot->GetBox().GetSize(Rml::BoxArea::Border).x, Rml::Unit::PX});
    mMenu = menu.get();
    mRoot->SetClass("expanded", true);
    mMenu->on_close([this] {
        mMenu = nullptr;
        mRoot->SetClass("expanded", false);
    });
    push_document(std::move(menu));
}

}  // namespace dusk::ui
