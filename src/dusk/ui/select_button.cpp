#include "select_button.hpp"

#include "ui.hpp"

#include <utility>

namespace dusk::ui {
namespace {

Rml::Element* createRoot(Rml::Element* parent) {
    auto* doc = parent->GetOwnerDocument();
    auto elem = doc->CreateElement("select-button");
    return parent->AppendChild(std::move(elem));
}

Rml::String value_label(const Rml::String& value, bool modified) {
    return modified ? Rml::String{"•\u00a0"} + value : value;
}

}  // namespace

SelectButton::SelectButton(Rml::Element* parent, Props props)
    : FluentComponent(createRoot(parent)) {
    mKeyElem = append(mRoot, "key");
    mIconElem = append(mRoot, "icon");
    mValueElem = append(mRoot, "value");
    update_props(std::move(props));
    on_nav_command([this](Rml::Event&, NavCommand cmd) { return handle_nav_command(cmd); });
}

bool SelectButton::modified() const {
    return mProps.modified;
}

void SelectButton::set_modified(bool value) {
    if (mProps.modified != value) {
        mValueElem->SetClass("modified", value);
        set_text_content(mValueElem, value_label(mProps.value, value));
        mProps.modified = value;
    }
}

void SelectButton::set_value_label(const Rml::String& value) {
    if (mProps.value != value) {
        set_text_content(mValueElem, value_label(value, mProps.modified));
        mProps.value = value;
    }
}

SelectButton& SelectButton::on_pressed(SelectButtonCallback callback) {
    if (!callback) {
        return *this;
    }
    listen(Rml::EventId::Submit, [this, callback = std::move(callback)](Rml::Event& event) {
        if (!disabled() && event.GetTargetElement() == mRoot) {
            callback();
            event.StopPropagation();
        }
    });
    return *this;
}

void SelectButton::update_props(Props props) {
    if (mProps.key != props.key) {
        set_text_content(mKeyElem, props.key);
    }
    if (mProps.icon != props.icon) {
        Rml::StringList iconClasses;
        Rml::StringUtilities::ExpandString(iconClasses, mIconElem->GetClassNames(), ' ', true);
        for (const auto& className : iconClasses) {
            mIconElem->SetClass(className, false);
        }
        if (!props.icon.empty()) {
            mIconElem->SetClass(props.icon, true);
        }
    }
    set_value_label(props.value);
    set_modified(props.modified);
    mProps = std::move(props);
}

bool SelectButton::handle_nav_command(NavCommand cmd) {
    if (cmd == NavCommand::Confirm && mProps.submit) {
        mRoot->DispatchEvent(Rml::EventId::Submit, {});
        return true;
    }
    return false;
}

void BaseControlledSelectButton::update() {
    set_disabled(disabled());
    set_value_label(format_value());
    set_modified(modified());
    SelectButton::update();
}

bool ControlledSelectButton::modified() const {
    if (mIsModified) {
        return mIsModified();
    }
    return BaseControlledSelectButton::modified();
}

bool ControlledSelectButton::disabled() const {
    if (mIsDisabled) {
        return mIsDisabled();
    }
    return BaseControlledSelectButton::disabled();
}

Rml::String ControlledSelectButton::format_value() {
    if (!mGetValue) {
        return "";
    }
    return mGetValue();
}

}  // namespace dusk::ui
