#include "tooltip.hpp"

#include "ui.hpp"

#include <algorithm>

namespace dusk::ui {

Tooltip::Tooltip(Rml::Element* anchor, const Rml::String& label)
    : mAnchor{anchor}, mRoot{append(anchor->GetOwnerDocument(), "ui-tooltip")},
      mFollowsFocus{anchor->GetContext()->GetHoverElement() == nullptr},
      mMouseMove{anchor->GetContext()->GetRootElement(), Rml::EventId::Mousemove,
          [this](Rml::Event&) { mFollowsFocus = false; }, true},
      mMouseDown{anchor->GetContext()->GetRootElement(), Rml::EventId::Mousedown,
          [this](Rml::Event&) { mFollowsFocus = false; }, true},
      mKeyDown{anchor->GetContext()->GetRootElement(), Rml::EventId::Keydown,
          [this](Rml::Event& event) {
              if (map_nav_event(event) != NavCommand::None) {
                  mFollowsFocus = true;
              }
          },
          true} {
    // Attach outside the pane so scrolling and overflow cannot clip the label.
    append_text(mRoot, label);
}

Tooltip::~Tooltip() {
    mRoot->GetParentNode()->RemoveChild(mRoot);
}

void Tooltip::update() {
    auto* context = mAnchor->GetContext();
    const bool active = mFollowsFocus ? mAnchor->Contains(context->GetFocusElement()) :
                                        mAnchor->IsPseudoClassSet("hover");
    const bool visible =
        !mAnchor->IsPseudoClassSet("disabled") && mAnchor->IsVisible(true) && active;
    const bool opening = visible && !mRoot->IsClassSet("visible");
    mRoot->SetClass("visible", visible);
    if (!visible) {
        return;
    }
    if (opening) {
        mRoot->GetOwnerDocument()->UpdateDocument();
    }
    const float margin = 8.0f * context->GetDensityIndependentPixelRatio();
    const auto dimensions = Rml::Vector2f{context->GetDimensions()};
    const auto anchor = mAnchor->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto anchorSize = mAnchor->GetBox().GetSize(Rml::BoxArea::Border);
    const auto size = mRoot->GetBox().GetSize(Rml::BoxArea::Border);
    float x = anchor.x + (anchorSize.x - size.x) * 0.5f;
    float y = anchor.y + anchorSize.y + margin;
    if (y + size.y > dimensions.y - margin) {
        y = anchor.y - size.y - margin;
    }
    x = std::clamp(x, margin, std::max(margin, dimensions.x - size.x - margin));
    y = std::clamp(y, margin, std::max(margin, dimensions.y - size.y - margin));
    mRoot->SetProperty(Rml::PropertyId::Left, Rml::Property{x, Rml::Unit::PX});
    mRoot->SetProperty(Rml::PropertyId::Top, Rml::Property{y, Rml::Unit::PX});
}

}  // namespace dusk::ui
