#pragma once

#include "button.hpp"
#include "tooltip.hpp"

#include <string_view>

namespace dusk::ui {

const char* material_icon(std::string_view name);

class IconButton : public ControlledButton {
public:
    struct Props {
        Rml::String icon;
        Rml::String label;
        std::function<bool()> isSelected;
        std::function<bool()> isDisabled;
    };

    IconButton(Rml::Element* parent, Props props);
    void set_icon(std::string_view icon);
    void set_label(const Rml::String& label);
    void set_tooltip(const Rml::String& text) override;

private:
    Rml::Element* mIcon;
    Rml::String mIconName;
    Rml::String mLabel;
    Rml::String mTooltipText;
};

}  // namespace dusk::ui
