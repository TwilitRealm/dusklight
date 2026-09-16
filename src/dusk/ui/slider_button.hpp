#pragma once

#include "number_button.hpp"

#include <RmlUi/Core.h>

#include <functional>

namespace dusk::ui {

class SliderButton : public BaseStringButton {
public:
    struct Props {
        Rml::String key;
        std::function<int()> getValue;
        std::function<void(int)> setValue;
        std::function<bool()> isDisabled;
        std::function<bool()> isModified;
        int min = 0;
        int max = 100;
        int step = 5;
        Rml::String suffix;
    };

    SliderButton(Rml::Element* parent, Props props);

    bool modified() const override;
    bool disabled() const override;
    void update() override;

protected:
    Rml::String format_value() override;
    void set_value(Rml::String value) override;
    bool handle_nav_command(NavCommand cmd) override;

private:
    void update_fill();
    void apply_from_pointer(Rml::Event& event);

    std::function<int()> mGetValue;
    std::function<void(int)> mSetValue;
    std::function<bool()> mIsDisabled;
    std::function<bool()> mIsModified;
    int mMin;
    int mMax;
    int mStep;
    Rml::String mSuffix;
    Rml::Element* mFillElem = nullptr;
    Rml::Element* mThumbElem = nullptr;
};

}  // namespace dusk::ui