#include "slider_button.hpp"

#include "m_Do/m_Do_audio.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <charconv>
#include <fmt/format.h>

namespace dusk::ui {

SliderButton::SliderButton(Rml::Element* parent, Props props)
    : BaseStringButton(parent, {.key = std::move(props.key), .type = "text"}),
      mGetValue(std::move(props.getValue)), mSetValue(std::move(props.setValue)),
      mIsDisabled(std::move(props.isDisabled)), mIsModified(std::move(props.isModified)),
      mMin(props.min), mMax(props.max), mStep(props.step), mSuffix(std::move(props.suffix)) {
    mRoot->SetClass("slider-volume", true);
    // Force the row to be the containing block for the absolutely-positioned value
    // and edit input, so they anchor to this row instead of escaping to a distant
    // panel (beware: stylesheet-only `position: relative` can be dropped by RmlUi).
    mRoot->SetProperty(Rml::PropertyId::Position, Rml::Style::Position::Relative);

    // Place the track between the key/label and the numeric value, so the slider
    // sits left of the value. The value/input are absolutely positioned to a fixed
    // right column in CSS, so editing never shifts the track.
    auto* doc = mRoot->GetOwnerDocument();
    auto trackElem = doc->CreateElement("slider");
    Rml::Element* trackRaw = mRoot->InsertBefore(
        std::move(trackElem), mValueElem != nullptr ? mValueElem : nullptr);
    auto fillElem = doc->CreateElement("slider-fill");
    mFillElem = trackRaw->AppendChild(std::move(fillElem));
    auto thumbElem = doc->CreateElement("slider-thumb");
    mThumbElem = trackRaw->AppendChild(std::move(thumbElem));

    // Note: we do NOT StopPropagation here. The mousedown must bubble to RmlUi so
    // it can begin drag capture (enabled by the `drag: drag` style on the track).
    // The focused edit is guarded separately below, see the Click handler.
    const auto adjustFromPointer = [this](Rml::Event& event) {
        if (disabled()) {
            return;
        }
        apply_from_pointer(event);
    };
    Component::listen(trackRaw, Rml::EventId::Mousedown, adjustFromPointer);
    Component::listen(trackRaw, Rml::EventId::Drag, adjustFromPointer);

    // Only clicking the numeric value (or the active edit input) should open the
    // text box. Any other click on the row -- including the dead space above/below
    // the bar, or dragging -- must NOT enter edit mode. We intercept the click in
    // the capture phase (runs before the base select-button's confirm handler),
    // completely stopping it unless it targets the value or the edit input.
    Component::listen(mRoot, Rml::EventId::Click,
        [this](Rml::Event& event) {
            Rml::Element* target = event.GetTargetElement();
            const bool onEditTarget = (target == mValueElem) ||
                (target != nullptr && target->GetTagName() == "input");
            if (onEditTarget) {
                return;  // let it bubble to the base confirm handler -> edit
            }
            event.StopImmediatePropagation();
        },
        true);

    update_fill();
}

bool SliderButton::modified() const {
    if (mIsModified) {
        return mIsModified();
    }
    return BaseStringButton::modified();
}

bool SliderButton::disabled() const {
    if (mIsDisabled) {
        return mIsDisabled();
    }
    return BaseStringButton::disabled();
}

void SliderButton::update() {
    BaseStringButton::update();
    update_fill();
}

Rml::String SliderButton::format_value() {
    return fmt::format("{}{}", mGetValue(), mSuffix);
}

void SliderButton::set_value(Rml::String value) {
    if (!mSetValue) {
        return;
    }
    int parsedValue = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsedValue);
    if (result.ec != std::errc() || result.ptr != end) {
        return;
    }
    mSetValue(std::clamp(parsedValue, mMin, mMax));
}

bool SliderButton::handle_nav_command(NavCommand cmd) {
    if (!is_editing() && (cmd == NavCommand::Left || cmd == NavCommand::Right)) {
        const int newValue = std::clamp(
            mGetValue() + (cmd == NavCommand::Right ? mStep : -mStep), mMin, mMax);
        if (newValue != mGetValue()) {
            mSetValue(newValue);
            mDoAud_seStartMenu(kSoundItemChange);
            update_fill();
        }
        return true;
    }
    return BaseStringButton::handle_nav_command(cmd);
}

void SliderButton::update_fill() {
    if (mFillElem == nullptr) {
        return;
    }
    const int current = std::clamp(mGetValue(), mMin, mMax);
    const float span = static_cast<float>(mMax - mMin);
    const float percent = span > 0.0f ? static_cast<float>(current - mMin) / span : 0.0f;
    mFillElem->SetProperty(
        Rml::PropertyId::Width, Rml::Property{percent * 100.0f, Rml::Unit::PERCENT});
    if (mThumbElem != nullptr) {
        mThumbElem->SetProperty(Rml::PropertyId::Left, Rml::Property{percent * 100.0f, Rml::Unit::PERCENT});
    }
}

void SliderButton::apply_from_pointer(Rml::Event& event) {
    Rml::Element* trackElem = mFillElem != nullptr ? mFillElem->GetParentNode() : nullptr;
    if (trackElem == nullptr) {
        return;
    }

    const Rml::Box box = trackElem->GetBox();
    const float widthDp = box.GetSize(Rml::BoxArea::Border).x;
    if (widthDp <= 0.0f) {
        return;
    }

    const float dpRatio = trackElem->GetContext()->GetDensityIndependentPixelRatio();
    const float px = event.GetParameter<float>("mouse_x", 0.0f);
    const auto origin = trackElem->GetAbsoluteOffset(Rml::BoxArea::Border);
    const float x = (px - origin.x) / (widthDp * dpRatio);
    const float clamped = std::clamp(x, 0.0f, 1.0f);

    const float raw = static_cast<float>(mMin) + clamped * static_cast<float>(mMax - mMin);
    const int step = std::max(1, mStep);
    const int newValue =
        std::clamp(static_cast<int>(std::round(raw / static_cast<float>(step))) * step, mMin, mMax);
    if (newValue != mGetValue()) {
        mSetValue(newValue);
        mDoAud_seStartMenu(kSoundItemChange);
        update_fill();
        update();
    }
}

}  // namespace dusk::ui