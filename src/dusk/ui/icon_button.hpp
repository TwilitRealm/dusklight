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
    void update() override;

private:
    Tooltip mTooltip;
};

}  // namespace dusk::ui
