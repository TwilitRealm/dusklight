#pragma once

#include "event.hpp"

namespace dusk::ui {

class Tooltip {
public:
    Tooltip(Rml::Element* anchor, const Rml::String& label);
    ~Tooltip();

    Tooltip(const Tooltip&) = delete;
    Tooltip& operator=(const Tooltip&) = delete;

    void update();

private:
    Rml::Element* mAnchor;
    Rml::Element* mRoot;
    bool mFollowsFocus;
    ScopedEventListener mMouseMove;
    ScopedEventListener mMouseDown;
    ScopedEventListener mKeyDown;
};

}  // namespace dusk::ui
