#pragma once

#include "select_button.hpp"

namespace dusk::ui {

class ContextMenu;

class DropdownButton : public BaseControlledSelectButton {
public:
    struct Option {
        Rml::String text;
        bool enabled = true;
    };

    struct Props {
        Rml::String key;
        std::vector<Option> options;
        std::function<int()> getValue;  // Selected index, -1 for none
        std::function<void(int)> setValue;
        std::function<bool()> isDisabled;
        std::function<bool()> isModified;
    };

    DropdownButton(Rml::Element* parent, Props props);
    ~DropdownButton() override;

    void update() override;
    void set_options(std::vector<Option> options);
    bool modified() const override;
    bool disabled() const override;

protected:
    Rml::String format_value() override;
    bool handle_nav_command(NavCommand cmd) override;

private:
    void toggle_menu();

    Props mProps;
    ContextMenu* mMenu = nullptr;
};

}  // namespace dusk::ui
