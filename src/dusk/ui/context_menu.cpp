#include "context_menu.hpp"

#include "button.hpp"
#include "icon_button.hpp"

namespace dusk::ui {

ContextMenu::Binding::Binding(Document& owner, Rml::Element* root, Rml::String selector,
    std::function<std::vector<Item>(Rml::Element*)> items)
    : mMouseDown{root, Rml::EventId::Mousedown,
          [this, &owner, root, selector = std::move(selector), items = std::move(items)](
              Rml::Event& event) {
              if (event.GetParameter<int>("button", -1) != 1 || !owner.active()) {
                  return;
              }
              auto* target = event.GetTargetElement()->Closest(selector);
              if (target == nullptr || !root->Contains(target)) {
                  return;
              }
              auto menuItems = items(target);
              if (menuItems.empty()) {
                  return;
              }
              dismiss();
              auto menu = std::make_unique<ContextMenu>(target, std::move(menuItems),
                  Rml::Vector2f{
                      event.GetParameter<float>("mouse_x", 0),
                      event.GetParameter<float>("mouse_y", 0),
                  });
              mMenu = menu.get();
              mMenu->on_close([this] { mMenu = nullptr; });
              push_document(std::move(menu));
              event.StopPropagation();
          }} {}

ContextMenu::Binding::~Binding() {
    dismiss();
}

void ContextMenu::Binding::dismiss() {
    if (mMenu != nullptr) {
        mMenu->on_close(nullptr);
        mMenu->dismiss();
        mMenu = nullptr;
    }
}

ContextMenu::ContextMenu(
    Rml::Element* anchor, std::vector<Item> items, std::optional<Rml::Vector2f> position)
    : Popover{anchor, Side::Below, "context-menu"},
      mNavigation{body(), {.horizontalBoundary = NavGroup::Boundary::Stop,
                              .verticalBoundary = NavGroup::Boundary::Stop}} {
    if (position) {
        set_position(*position);
    }
    for (auto& item : items) {
        if (item.separatorBefore && body()->GetNumChildren() != 0) {
            append(body(), "menu-separator");
        }
        auto& button = mNavigation.add_item<Button>(Rml::String{});
        append_text(append(button.root(), "icon"), material_icon(item.icon));
        append_text_element(button.root(), "span", item.text);
        button.root()->SetClass("destructive", item.destructive);
        button.set_disabled(!item.enabled || !item.onPressed);
        button.on_pressed([this, callback = std::move(item.onPressed)] {
            dismiss();
            callback();
        });
    }
}

bool ContextMenu::focus() {
    return mNavigation.focus() || Popover::focus();
}

}  // namespace dusk::ui
