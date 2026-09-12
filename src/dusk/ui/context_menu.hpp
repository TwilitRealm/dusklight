#pragma once

#include "nav_group.hpp"
#include "popover.hpp"

namespace dusk::ui {

class ContextMenu : public Popover {
public:
    struct Item {
        Rml::String text;
        Rml::String icon;
        std::function<void()> onPressed;
        bool enabled = true;
        bool destructive = false;
        bool separatorBefore = false;
    };

    class Binding {
    public:
        Binding(Document& owner, Rml::Element* root, Rml::String selector,
            std::function<std::vector<Item>(Rml::Element*)> items);
        ~Binding();

        void dismiss();

    private:
        ContextMenu* mMenu = nullptr;
        ScopedEventListener mMouseDown;
    };

    ContextMenu(Rml::Element* anchor, std::vector<Item> items,
        std::optional<Rml::Vector2f> position = std::nullopt);

    bool focus() override;

private:
    NavGroup mNavigation;
};

}  // namespace dusk::ui
