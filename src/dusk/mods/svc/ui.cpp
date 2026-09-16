#include "ui.hpp"

#include "config.hpp"
#include "internal.hpp"
#include "registry.hpp"
#include "ui_v1.hpp"

#include "dusk/mod_loader.hpp"
#include "dusk/mods/loader/loader.hpp"
#include "dusk/mods/log_buffer.hpp"
#include "dusk/ui/context_menu.hpp"
#include "dusk/ui/dropdown_button.hpp"
#include "dusk/ui/icon_button.hpp"
#include "dusk/ui/list.hpp"
#include "dusk/ui/menu_bar.hpp"
#include "dusk/ui/mod_window.hpp"
#include "dusk/ui/modal.hpp"
#include "dusk/ui/row.hpp"
#include "dusk/ui/ui.hpp"
#include "mods/svc/ui.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL_clipboard.h>
#include <aurora/rmlui.hpp>
#include <borealis/log.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr size_t kUiControlSelectedSize =
    offsetof(UiControlDesc, is_selected) + sizeof(UiPredicateFn);
constexpr size_t kUiControlStringSetModeSize =
    offsetof(UiControlDesc, string_set_mode) + sizeof(UiStringSetMode);
constexpr size_t kUiControlFilePickerSize = offsetof(UiControlDesc, directory_mode) + sizeof(bool);
constexpr size_t kUiControlIconSize = offsetof(UiControlDesc, icon) + sizeof(const char*);
constexpr size_t kUiControlOptionsSize =
    offsetof(UiControlDesc, option_enabled) + sizeof(const bool*);
constexpr size_t kUiControlTooltipSize = offsetof(UiControlDesc, tooltip) + sizeof(const char*);
constexpr size_t kUiListItemV21Size = offsetof(UiListItem, label) + sizeof(const char*);
constexpr size_t kUiListDescV21Size = offsetof(UiListDesc, user_data) + sizeof(void*);

}  // namespace

namespace dusk::mods::svc::ui_impl {
namespace {

constexpr borealis::Log Log{"dusk::mods::ui"};

enum class UiSlotKind : u8 {
    Window,
    Dialog,
    Pane,
    Row,
    Text,
    Progress,
    Control,
    List,
    Style,
    MenuTab,
    ContextMenu,
};

const char* slot_kind_name(UiSlotKind kind) {
    switch (kind) {
    case UiSlotKind::Window:
        return "window";
    case UiSlotKind::Dialog:
        return "dialog";
    case UiSlotKind::Row:
        return "row";
    case UiSlotKind::Pane:
        return "pane";
    case UiSlotKind::Text:
        return "text";
    case UiSlotKind::Progress:
        return "progress";
    case UiSlotKind::Control:
        return "control";
    case UiSlotKind::List:
        return "list";
    case UiSlotKind::Style:
        return "style";
    case UiSlotKind::MenuTab:
        return "menu tab";
    case UiSlotKind::ContextMenu:
        return "context menu";
    default:
        return "unknown";
    }
}

// Game thread only: all mutations happen in service calls made from mod code, in UI callbacks
// (ui::update), or in the loader's deactivate paths.
struct UiSlot {
    UiSlotKind kind = UiSlotKind::Window;
    // Pane/Row/Text/Progress/Control: freed automatically when the element is destroyed
    Rml::Element* element = nullptr;
    // Container payload
    ui::Component* component = nullptr;
    ui::Component* container = nullptr;
    UiControlKind controlKind = UI_CONTROL_BUTTON;
    std::unique_ptr<Rml::Property> displayOverride;
    bool hidden = false;
    uint64_t anchorHandle = 0;
    ui::Pane* pane = nullptr;
    ui::Pane* helpPane = nullptr;
    // List payload
    ui::List* list = nullptr;
    // Window/Dialog payload
    ui::Document* document = nullptr;
    UiWindowClosedFn onClosed = nullptr;
    void* onClosedUserData = nullptr;
    // Style payload
    ui::DocumentScope styleScope = ui::DocumentScope::None;
    std::string styleId;
    // Cached rendered values for element setters. These make the natural "set every update"
    // style cheap when the displayed value has not changed.
    std::string elementValue;
    float elementFloat = 0.0f;
    bool hasElementValue = false;
    bool elementValueIsRml = false;
};

SlotMap<UiSlot> s_slots;

struct ModUiPanel {
    UiPanelBuildFn build = nullptr;
    UiPanelUpdateFn update = nullptr;
    void* userData = nullptr;
};

std::unordered_map<const LoadedMod*, ModUiPanel> s_modPanels;

struct ModMenuTab {
    uint64_t handle = 0;
    std::string label;
    UiPressedFn onSelected = nullptr;
    void* userData = nullptr;
};

std::unordered_map<const LoadedMod*, std::vector<ModMenuTab>> s_modMenuTabs;
bool s_menuTabsDirty = false;

UiSlot* slot_from_handle(uint64_t handle) {
    auto* entry = s_slots.find(handle);
    return entry != nullptr ? &entry->value : nullptr;
}

// Note: s_slots may reallocate on any later allocation, so callers must not hold the returned
// slot reference across calls that can allocate (e.g. mod build callbacks); re-resolve instead.
UiSlot& alloc_slot(LoadedMod& mod, UiSlotKind kind, uint64_t& outHandle) {
    outHandle = s_slots.emplace(mod, UiSlot{.kind = kind});
    return s_slots.find(outHandle)->value;
}

UiSlot* resolve(LoadedMod& mod, uint64_t handle, UiSlotKind kind, const char* what) {
    auto* entry = s_slots.find_owned(handle, mod);
    if (entry == nullptr || entry->value.kind != kind) {
        Log.error("[{}] {}: stale or invalid {} handle {:#x}", mod.metadata.id, what,
            slot_kind_name(kind), handle);
        return nullptr;
    }
    return &entry->value;
}

UiSlot* resolve_element(LoadedMod& mod, uint64_t handle) {
    auto* entry = s_slots.find_owned(handle, mod);
    return entry != nullptr && entry->value.element != nullptr ? &entry->value : nullptr;
}

UiSlot* resolve_container(LoadedMod& mod, uint64_t handle, const char* what) {
    auto* entry = s_slots.find_owned(handle, mod);
    if (entry == nullptr ||
        (entry->value.kind != UiSlotKind::Pane && entry->value.kind != UiSlotKind::Row))
    {
        Log.error(
            "[{}] {}: stale or invalid container handle {:#x}", mod.metadata.id, what, handle);
        return nullptr;
    }
    return &entry->value;
}

bool element_available(const UiSlot& slot) {
    if (!slot.element->IsVisible(true)) {
        return false;
    }
    for (auto* node = slot.element; node != nullptr; node = node->GetParentNode()) {
        if (node->IsPseudoClassSet("disabled") || node->HasAttribute("disabled")) {
            return false;
        }
    }
    return slot.component == nullptr || !slot.component->disabled();
}

// Whether the registration a callback was created under is still live. Callbacks captured by
// host-owned UI must check this before calling into the mod: `mod->active` alone is true again
// once a reload completes, but captured fn pointers still target the unloaded image. Teardown
// frees the slots (ui_remove_mod), which invalidates every callback built under them.
bool slot_live(uint64_t handle) {
    return s_slots.find(handle) != nullptr;
}

bool dialog_open(uint64_t handle) {
    auto* slot = slot_from_handle(handle);
    return slot != nullptr && slot->kind == UiSlotKind::Dialog && slot->document != nullptr &&
           slot->document->active();
}

// Frees the slot when the tracked element is destroyed (tab rebuilds, window teardown, ...).
// The generation check makes a late detach of an already-recycled slot a no-op.
class SlotDetachListener final : public Rml::EventListener {
public:
    explicit SlotDetachListener(uint64_t handle) : m_handle{handle} {}

    void ProcessEvent(Rml::Event&) override {}

    void OnDetach(Rml::Element*) override {
        s_slots.erase(m_handle);
        std::vector<ui::ContextMenu*> menus;
        s_slots.for_each([&](uint64_t, const auto& entry) {
            if (entry.value.kind == UiSlotKind::ContextMenu && entry.value.anchorHandle == m_handle)
            {
                menus.push_back(static_cast<ui::ContextMenu*>(entry.value.document));
            }
        });
        for (auto* menu : menus) {
            menu->dismiss(false);
        }
        delete this;
    }

private:
    uint64_t m_handle;
};

void track_element(uint64_t handle, UiSlot& slot, Rml::Element& element) {
    slot.element = &element;
    element.AddEventListener(Rml::EventId::Click, new SlotDetachListener{handle});
}

template <typename T, typename Fn>
T guarded_call(LoadedMod& mod, const char* what, T fallback, Fn&& fn) {
    if (!mod.active) {
        return fallback;
    }
    try {
        return fn();
    } catch (const std::exception& e) {
        fail_mod(mod, MOD_ERROR, fmt::format("exception in {}: {}", what, e.what()));
    } catch (...) {
        fail_mod(mod, MOD_ERROR, fmt::format("unknown exception in {}", what));
    }
    return fallback;
}

template <typename Fn>
void guarded_call(LoadedMod& mod, const char* what, Fn&& fn) {
    if (!mod.active) {
        return;
    }
    try {
        fn();
    } catch (const std::exception& e) {
        fail_mod(mod, MOD_ERROR, fmt::format("exception in {}: {}", what, e.what()));
    } catch (...) {
        fail_mod(mod, MOD_ERROR, fmt::format("unknown exception in {}", what));
    }
}

// Shared by panel/tab build and update callbacks: translates a non-OK result or an escaped
// exception into fail_mod, mirroring mod_update handling.
template <typename Fn>
void invoke_mod_ui_callback(LoadedMod& mod, const char* what, Fn&& fn) {
    ModError error = MOD_ERROR_INIT;
    const ModResult result = guarded_call(mod, what, MOD_OK, [&] { return fn(&error); });
    if (result != MOD_OK && mod.active) {
        fail_mod(
            mod, result, error.message[0] != '\0' ? error.message : fmt::format("{} failed", what));
    }
}

uint64_t wrap_pane(LoadedMod& mod, ui::Pane& pane, ui::Pane* helpPane) {
    uint64_t handle = 0;
    auto& slot = alloc_slot(mod, UiSlotKind::Pane, handle);
    slot.component = &pane;
    slot.container = &pane;
    slot.pane = &pane;
    slot.helpPane = helpPane;
    track_element(handle, slot, *pane.root());
    return handle;
}

int clamp_to_int(int64_t value) {
    return static_cast<int>(std::clamp<int64_t>(value, INT_MIN, INT_MAX));
}

std::function<bool()> wrap_predicate(
    LoadedMod& mod, UiPredicateFn fn, void* userData, uint64_t guardHandle) {
    if (fn == nullptr) {
        return {};
    }
    return [modPtr = &mod, fn, userData, guardHandle] {
        if (!slot_live(guardHandle)) {
            return false;
        }
        return guarded_call(*modPtr, "control predicate", false,
            [&] { return fn(modPtr->context.get(), userData); });
    };
}

void wire_callback_binding(
    LoadedMod& mod, const UiControlDesc& desc, ui::ModControlSpec& spec, uint64_t guardHandle) {
    auto* modPtr = &mod;
    const auto get = desc.get;
    const auto set = desc.set;
    auto* userData = desc.user_data;
    const auto getValue = [modPtr, get, userData, guardHandle] {
        UiControlValue value = UI_CONTROL_VALUE_INIT;
        if (!slot_live(guardHandle)) {
            return value;
        }
        guarded_call(
            *modPtr, "control getter", [&] { get(modPtr->context.get(), userData, &value); });
        return value;
    };
    const auto setValue = [modPtr, set, userData, guardHandle](const UiControlValue& value) {
        if (!slot_live(guardHandle)) {
            return;
        }
        guarded_call(
            *modPtr, "control setter", [&] { set(modPtr->context.get(), userData, &value); });
    };
    switch (desc.kind) {
    case UI_CONTROL_TOGGLE:
        spec.getBool = [getValue] { return getValue().bool_value; };
        spec.setBool = [setValue](bool value) {
            UiControlValue raw = UI_CONTROL_VALUE_INIT;
            raw.bool_value = value;
            setValue(raw);
        };
        break;
    case UI_CONTROL_NUMBER:
    case UI_CONTROL_SELECT:
    case UI_CONTROL_DROPDOWN:
        spec.getInt = [getValue, dropdown = desc.kind == UI_CONTROL_DROPDOWN] {
            const auto value = getValue().int_value;
            return dropdown && (value < 0 || value > INT_MAX) ? -1 : clamp_to_int(value);
        };
        spec.setInt = [setValue](int value) {
            UiControlValue raw = UI_CONTROL_VALUE_INIT;
            raw.int_value = value;
            setValue(raw);
        };
        break;
    case UI_CONTROL_STRING:
    case UI_CONTROL_COLOR:
    case UI_CONTROL_FILE_PICKER:
        spec.getString = [getValue]() -> Rml::String {
            const UiControlValue value = getValue();
            return value.string_value != nullptr ? value.string_value : "";
        };
        spec.setString = [setValue](Rml::String value) {
            UiControlValue raw = UI_CONTROL_VALUE_INIT;
            raw.string_value = value.c_str();
            setValue(raw);
        };
        break;
    default:
        break;
    }
}

// The lambdas re-resolve the var on every call, so a control whose var was unregistered
// mid-flight degrades to a no-op instead of a dangling read.
bool wire_config_var_binding(LoadedMod& mod, const UiControlDesc& desc, ui::ModControlSpec& spec) {
    auto* modPtr = &mod;
    const uint64_t varHandle = desc.config_var;
    switch (desc.kind) {
    case UI_CONTROL_TOGGLE: {
        const auto find = [modPtr, varHandle] {
            return static_cast<ConfigVar<bool>*>(
                config_find_var(*modPtr, varHandle, CONFIG_VAR_BOOL));
        };
        if (find() == nullptr) {
            return false;
        }
        spec.getBool = [find] {
            const auto* var = find();
            return var != nullptr && var->getValue();
        };
        spec.setBool = [find](bool value) {
            auto* var = find();
            if (var == nullptr || var->getValue() == value) {
                return;
            }
            var->setValue(value);
            config_mark_dirty();
        };
        if (!spec.isModified) {
            spec.isModified = [find] {
                const auto* var = find();
                return var != nullptr && var->getValue() != var->getDefaultValue();
            };
        }
        return true;
    }
    case UI_CONTROL_NUMBER:
    case UI_CONTROL_SELECT:
    case UI_CONTROL_DROPDOWN: {
        const auto find = [modPtr, varHandle] {
            return static_cast<ConfigVar<s64>*>(
                config_find_var(*modPtr, varHandle, CONFIG_VAR_INT));
        };
        if (find() == nullptr) {
            return false;
        }
        spec.getInt = [find, dropdown = desc.kind == UI_CONTROL_DROPDOWN] {
            const auto* var = find();
            const auto value = var != nullptr ? var->getValue() : (dropdown ? -1 : 0);
            return dropdown && (value < 0 || value > INT_MAX) ? -1 : clamp_to_int(value);
        };
        spec.setInt = [find](int value) {
            auto* var = find();
            if (var == nullptr || var->getValue() == value) {
                return;
            }
            var->setValue(value);
            config_mark_dirty();
        };
        if (!spec.isModified) {
            spec.isModified = [find] {
                const auto* var = find();
                return var != nullptr && var->getValue() != var->getDefaultValue();
            };
        }
        return true;
    }
    case UI_CONTROL_STRING:
    case UI_CONTROL_COLOR:
    case UI_CONTROL_FILE_PICKER: {
        const auto find = [modPtr, varHandle] {
            return static_cast<ConfigVar<std::string>*>(
                config_find_var(*modPtr, varHandle, CONFIG_VAR_STRING));
        };
        if (find() == nullptr) {
            return false;
        }
        spec.getString = [find]() -> Rml::String {
            const auto* var = find();
            return var != nullptr ? var->getValue() : "";
        };
        spec.setString = [find](Rml::String value) {
            auto* var = find();
            if (var == nullptr || var->getValue() == value) {
                return;
            }
            var->setValue(std::move(value));
            config_mark_dirty();
        };
        if (!spec.isModified) {
            spec.isModified = [find] {
                const auto* var = find();
                return var != nullptr && var->getValue() != var->getDefaultValue();
            };
        }
        return true;
    }
    default:
        return false;
    }
}

void on_mod_window_destroyed(uint64_t handle) {
    const auto* entry = s_slots.find(handle);
    if (entry == nullptr || entry->value.kind != UiSlotKind::Window) {
        return;
    }
    auto released = s_slots.take(handle);
    auto* mod = released->owner;
    const UiWindowClosedFn onClosed = released->value.onClosed;
    void* userData = released->value.onClosedUserData;
    if (mod != nullptr && onClosed != nullptr) {
        guarded_call(*mod, "window on_closed callback",
            [&] { onClosed(mod->context.get(), handle, userData); });
    }
}

void on_mod_dialog_destroyed(uint64_t handle) {
    auto* slot = slot_from_handle(handle);
    if (slot != nullptr && slot->kind == UiSlotKind::Dialog) {
        s_slots.erase(handle);
    }
}

class ModDialog final : public ui::Modal {
public:
    ModDialog(Props props, std::function<void()> onDestroyed)
        : Modal{std::move(props)}, m_onDestroyed{std::move(onDestroyed)} {}

    ~ModDialog() override {
        if (m_onDestroyed) {
            m_onDestroyed();
        }
    }

    void close() { pop(); }

private:
    std::function<void()> m_onDestroyed;
};

void push_stacked_document(std::unique_ptr<ui::Document> document) {
    if (auto* previousTop = ui::top_document()) {
        previousTop->push(std::move(document));
    } else {
        ui::push_document(std::move(document));
    }
}

ui::ModalAction make_dialog_action(LoadedMod& mod, uint64_t handle, const UiDialogAction& action) {
    ui::ModalAction result{
        .label = action.label,
        .onPressed =
            [modPtr = &mod, handle, fn = action.on_pressed, userData = action.user_data,
                keepOpen = action.keep_open != 0](ui::Modal& modal) {
                if (!dialog_open(handle)) {
                    return;  // already being torn down
                }
                if (fn != nullptr) {
                    guarded_call(*modPtr, "dialog action callback",
                        [&] { fn(modPtr->context.get(), handle, userData); });
                }
                // The callback may have closed the dialog already
                if (!keepOpen && dialog_open(handle)) {
                    static_cast<ModDialog&>(modal).close();
                }
            },
    };
    if (action.is_disabled != nullptr) {
        result.isDisabled = [modPtr = &mod, handle, fn = action.is_disabled,
                                userData = action.user_data] {
            if (!dialog_open(handle)) {
                return false;
            }
            return guarded_call(*modPtr, "dialog action is_disabled callback", false,
                [&] { return fn(modPtr->context.get(), userData); });
        };
    }
    return result;
}

}  // namespace

ModResult ui_register_mods_panel(LoadedMod& mod, const UiModsPanelDesc& desc) {
    s_modPanels[&mod] = {desc.build, desc.update, desc.user_data};
    return MOD_OK;
}

void ui_build_mods_panels(LoadedMod& mod, ui::Pane& pane) {
    const auto it = s_modPanels.find(&mod);
    if (it == s_modPanels.end()) {
        return;
    }
    const uint64_t paneHandle = wrap_pane(mod, pane, nullptr);
    const auto& panel = it->second;
    if (!mod.active || panel.build == nullptr) {
        return;
    }
    invoke_mod_ui_callback(mod, "mod UI panel build", [&](ModError* error) {
        return panel.build(mod.context.get(), paneHandle, panel.userData, error);
    });
}

void ui_update_mods_panels(LoadedMod& mod) {
    const auto it = s_modPanels.find(&mod);
    if (it == s_modPanels.end()) {
        return;
    }
    const auto& panel = it->second;
    if (!mod.active || panel.update == nullptr) {
        return;
    }
    invoke_mod_ui_callback(mod, "mod UI panel update",
        [&](ModError* error) { return panel.update(mod.context.get(), panel.userData, error); });
}

ModResult ui_pane_add_row(
    LoadedMod& mod, uint64_t parent, const UiRowDesc& desc, uint64_t* outRow) {
    auto* slot = resolve_container(mod, parent, "pane_add_row");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* pane = slot->pane;
    auto* helpPane = slot->helpPane;
    auto align = ui::Row::Align::Start;
    switch (desc.align) {
    case UI_ROW_ALIGN_CENTER:
        align = ui::Row::Align::Center;
        break;
    case UI_ROW_ALIGN_END:
        align = ui::Row::Align::End;
        break;
    case UI_ROW_ALIGN_SPACE_BETWEEN:
        align = ui::Row::Align::SpaceBetween;
        break;
    default:
        break;
    }
    auto& row = slot->container->add_child<ui::Row>(ui::Row::Props{
        .align = align,
        .wrap = desc.wrap,
    });
    auto& rowSlot = alloc_slot(mod, UiSlotKind::Row, *outRow);
    rowSlot.component = &row;
    rowSlot.container = &row;
    rowSlot.pane = pane;
    rowSlot.helpPane = helpPane;
    track_element(*outRow, rowSlot, *row.root());
    return MOD_OK;
}

ModResult ui_pane_add_section(LoadedMod& mod, uint64_t pane, const char* title) {
    auto* slot = resolve_container(mod, pane, "pane_add_section");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    slot->container->add_section(title);
    return MOD_OK;
}

ModResult ui_pane_add_text(LoadedMod& mod, uint64_t pane, const char* text, uint64_t* outElem) {
    auto* slot = resolve_container(mod, pane, "pane_add_text");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* elem = slot->container->add_text(text);
    if (outElem != nullptr) {
        auto& elemSlot = alloc_slot(mod, UiSlotKind::Text, *outElem);
        elemSlot.elementValue = text;
        elemSlot.hasElementValue = true;
        track_element(*outElem, elemSlot, *elem);
    }
    return MOD_OK;
}

ModResult ui_pane_add_rml(LoadedMod& mod, uint64_t pane, const char* rml, uint64_t* outElem) {
    auto* slot = resolve_container(mod, pane, "pane_add_rml");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* elem = slot->container->add_rml(rml);
    if (outElem != nullptr) {
        auto& elemSlot = alloc_slot(mod, UiSlotKind::Text, *outElem);
        elemSlot.elementValue = rml;
        elemSlot.hasElementValue = true;
        elemSlot.elementValueIsRml = true;
        track_element(*outElem, elemSlot, *elem);
    }
    return MOD_OK;
}

ModResult ui_pane_add_progress(LoadedMod& mod, uint64_t pane, float value, uint64_t* outElem) {
    auto* slot = resolve_container(mod, pane, "pane_add_progress");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* elem = ui::append(slot->element, "progress");
    elem->SetAttribute("value", value);
    if (outElem != nullptr) {
        auto& elemSlot = alloc_slot(mod, UiSlotKind::Progress, *outElem);
        elemSlot.elementFloat = value;
        elemSlot.hasElementValue = true;
        track_element(*outElem, elemSlot, *elem);
    }
    return MOD_OK;
}

ModResult ui_pane_add_control(
    LoadedMod& mod, uint64_t pane, const UiControlDesc& desc, uint64_t* outElem) {
    auto* slot = resolve_container(mod, pane, "pane_add_control");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }

    ui::ModControlSpec spec;
    spec.label = desc.label;
    spec.helpRml = desc.help_rml != nullptr ? desc.help_rml : "";
    spec.isDisabled = [modPtr = &mod, pane,
                          predicate = wrap_predicate(mod, desc.is_disabled, desc.user_data, pane)] {
        return !slot_live(pane) || !modPtr->active || (predicate && predicate());
    };
    if (desc.struct_size >= kUiControlTooltipSize && desc.tooltip != nullptr) {
        spec.tooltip = desc.tooltip;
    }
    spec.isModified = wrap_predicate(mod, desc.is_modified, desc.user_data, pane);
    switch (desc.kind) {
    case UI_CONTROL_ICON_BUTTON:
    case UI_CONTROL_BUTTON:
    case UI_CONTROL_GROUP:
        spec.kind = desc.kind == UI_CONTROL_BUTTON ? ui::ModControlSpec::Kind::Button :
                                                     ui::ModControlSpec::Kind::Group;
        if (desc.kind == UI_CONTROL_ICON_BUTTON) {
            spec.kind = ui::ModControlSpec::Kind::IconButton;
            spec.icon = desc.icon;
        }
        if (desc.struct_size >= kUiControlSelectedSize) {
            spec.isSelected = wrap_predicate(mod, desc.is_selected, desc.user_data, pane);
        }
        spec.onPressed = [modPtr = &mod, fn = desc.on_pressed, userData = desc.user_data,
                             guardHandle = pane] {
            if (!slot_live(guardHandle)) {
                return;
            }
            guarded_call(*modPtr, "control on_pressed callback",
                [&] { fn(modPtr->context.get(), userData); });
        };
        break;
    case UI_CONTROL_TOGGLE:
        spec.kind = ui::ModControlSpec::Kind::Toggle;
        break;
    case UI_CONTROL_NUMBER:
        spec.kind = ui::ModControlSpec::Kind::Number;
        if (desc.min != desc.max) {
            spec.min = clamp_to_int(desc.min);
            spec.max = clamp_to_int(desc.max);
            if (spec.max < spec.min) {
                std::swap(spec.min, spec.max);
            }
        }
        spec.step = desc.step < 1 ? 1 : clamp_to_int(desc.step);
        spec.prefix = desc.prefix != nullptr ? desc.prefix : "";
        spec.suffix = desc.suffix != nullptr ? desc.suffix : "";
        break;
    case UI_CONTROL_STRING:
        spec.kind = ui::ModControlSpec::Kind::String;
        spec.maxLength = desc.max_length < 1 ? -1 : desc.max_length;
        spec.stringSetOnChange = desc.struct_size >= kUiControlStringSetModeSize &&
                                 desc.string_set_mode == UI_STRING_SET_ON_CHANGE;
        break;
    case UI_CONTROL_COLOR:
        spec.kind = ui::ModControlSpec::Kind::Color;
        spec.colorAlpha = desc.color_alpha;
        for (size_t i = 0; i < desc.color_preset_count; ++i) {
            spec.colorPresets.emplace_back(desc.color_presets[i]);
        }
        break;
    case UI_CONTROL_FILE_PICKER:
        spec.kind = ui::ModControlSpec::Kind::FilePicker;
        spec.directoryMode = desc.directory_mode;
        for (size_t i = 0; i < desc.file_filter_count; ++i) {
            spec.fileFilters.push_back({desc.file_filters[i].name, desc.file_filters[i].pattern});
        }
        break;
    case UI_CONTROL_DROPDOWN:
        spec.kind = ui::ModControlSpec::Kind::Dropdown;
        for (size_t i = 0; i < desc.option_count; ++i) {
            spec.dropdownOptions.push_back({
                desc.options[i],
                desc.struct_size < kUiControlOptionsSize || desc.option_enabled == nullptr ||
                    desc.option_enabled[i],
            });
        }
        break;
    case UI_CONTROL_SELECT:
        spec.kind = ui::ModControlSpec::Kind::Select;
        if (slot->helpPane == nullptr) {
            Log.error("[{}] pane_add_control: SELECT controls need a help pane (mod window tabs)",
                mod.metadata.id);
            return MOD_UNSUPPORTED;
        }
        for (size_t i = 0; i < desc.option_count; ++i) {
            spec.options.emplace_back(desc.options[i]);
        }
        break;
    default:
        return MOD_INVALID_ARGUMENT;
    }

    if (desc.kind != UI_CONTROL_BUTTON && desc.kind != UI_CONTROL_GROUP &&
        desc.kind != UI_CONTROL_ICON_BUTTON)
    {
        if (desc.binding == UI_BINDING_CONFIG_VAR) {
            if (!wire_config_var_binding(mod, desc, spec)) {
                Log.error("[{}] pane_add_control: config var handle {:#x} is unknown or its type "
                          "does not match the control kind",
                    mod.metadata.id, desc.config_var);
                return MOD_INVALID_ARGUMENT;
            }
        } else {
            wire_callback_binding(mod, desc, spec, pane);
        }
    }

    // Copy the pane pointers out: allocating the control's slot below may reallocate s_slots
    auto* paneComponent = slot->pane;
    auto* helpPane = slot->helpPane;
    auto* control =
        ui::build_mod_control(*slot->container, *paneComponent, helpPane, std::move(spec));
    if (control == nullptr) {
        return MOD_UNSUPPORTED;
    }
    if (outElem != nullptr) {
        auto& elemSlot = alloc_slot(mod, UiSlotKind::Control, *outElem);
        elemSlot.component = control;
        elemSlot.controlKind = desc.kind;
        track_element(*outElem, elemSlot, *control->root());
    }
    return MOD_OK;
}

ModResult ui_pane_add_list(LoadedMod& mod, uint64_t pane, const UiListDesc& desc,
    std::vector<ui::List::Item> items, uint64_t& outHandle) {
    outHandle = 0;
    auto* paneSlot = resolve_container(mod, pane, "pane_add_list");
    if (paneSlot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* paneComponent = paneSlot->container;

    uint64_t handle = 0;
    alloc_slot(mod, UiSlotKind::List, handle);

    ui::List::Props props;
    props.items = std::move(items);
    props.onPressed = [modPtr = &mod, handle, fn = desc.on_pressed, userData = desc.user_data](
                          uint64_t key) {
        if (!slot_live(handle)) {
            return;
        }
        guarded_call(*modPtr, "list on_pressed callback",
            [&] { fn(modPtr->context.get(), handle, key, userData); });
    };
    if (desc.is_selected != nullptr) {
        props.isSelected = [modPtr = &mod, handle, fn = desc.is_selected,
                               userData = desc.user_data](uint64_t key) {
            if (!slot_live(handle)) {
                return false;
            }
            return guarded_call(*modPtr, "list is_selected callback", false,
                [&] { return fn(modPtr->context.get(), handle, key, userData); });
        };
    }
    if (desc.is_disabled != nullptr) {
        props.isDisabled = [modPtr = &mod, handle, fn = desc.is_disabled,
                               userData = desc.user_data](uint64_t key) {
            if (!slot_live(handle)) {
                return false;
            }
            return guarded_call(*modPtr, "list is_disabled callback", false,
                [&] { return fn(modPtr->context.get(), handle, key, userData); });
        };
    }

    auto& list = paneComponent->add_child<ui::List>(std::move(props));
    auto* listSlot = slot_from_handle(handle);
    if (listSlot == nullptr) {
        return MOD_ERROR;
    }
    listSlot->component = &list;
    listSlot->list = &list;
    track_element(handle, *listSlot, *list.root());
    outHandle = handle;
    return MOD_OK;
}

ModResult ui_list_set_items(LoadedMod& mod, uint64_t handle, std::vector<ui::List::Item> items) {
    auto* slot = resolve(mod, handle, UiSlotKind::List, "list_set_items");
    if (slot == nullptr || slot->list == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    slot->list->set_items(std::move(items));
    return MOD_OK;
}

ModResult ui_pane_add_group(LoadedMod& mod, uint64_t groupPaneHandle, uint64_t targetPaneHandle,
    const UiGroupDesc& desc, uint64_t* outElem) {
    auto* groupSlot = resolve(mod, groupPaneHandle, UiSlotKind::Pane, "pane_add_group");
    auto* targetSlot = resolve(mod, targetPaneHandle, UiSlotKind::Pane, "pane_add_group");
    if (groupSlot == nullptr || targetSlot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (groupSlot->helpPane != targetSlot->pane) {
        Log.error("[{}] pane_add_group: panes are not a paired tab layout", mod.metadata.id);
        return MOD_UNSUPPORTED;
    }

    auto* groupPane = groupSlot->pane;
    auto* targetPane = targetSlot->pane;
    auto& button = groupPane->add_group_button(ui::GroupButton::Props{.text = desc.label});
    groupPane->register_control(button, *targetPane,
        [modPtr = &mod, groupPaneHandle, targetPaneHandle, build = desc.build,
            userData = desc.user_data](ui::Pane&) {
            if (!slot_live(groupPaneHandle) || !slot_live(targetPaneHandle) || !modPtr->active) {
                return;
            }
            invoke_mod_ui_callback(*modPtr, "mod UI group build", [&](ModError* error) {
                return build(modPtr->context.get(), targetPaneHandle, userData, error);
            });
        });

    if (outElem != nullptr) {
        auto& elemSlot = alloc_slot(mod, UiSlotKind::Control, *outElem);
        elemSlot.component = &button;
        elemSlot.controlKind = UI_CONTROL_GROUP;
        track_element(*outElem, elemSlot, *button.root());
    }
    return MOD_OK;
}

ModResult ui_elem_set_text(LoadedMod& mod, uint64_t elem, const char* text) {
    auto* slot = resolve(mod, elem, UiSlotKind::Text, "elem_set_text");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (slot->hasElementValue && !slot->elementValueIsRml && slot->elementValue == text) {
        return MOD_OK;
    }
    slot->elementValue = text;
    slot->hasElementValue = true;
    slot->elementValueIsRml = false;
    ui::set_text_content(slot->element, slot->elementValue);
    return MOD_OK;
}

ModResult ui_elem_set_rml(LoadedMod& mod, uint64_t elem, const char* rml) {
    auto* slot = resolve(mod, elem, UiSlotKind::Text, "elem_set_rml");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (slot->hasElementValue && slot->elementValueIsRml && slot->elementValue == rml) {
        return MOD_OK;
    }
    slot->elementValue = rml;
    slot->hasElementValue = true;
    slot->elementValueIsRml = true;
    slot->element->SetInnerRML(rml);
    return MOD_OK;
}

ModResult ui_elem_set_progress(LoadedMod& mod, uint64_t elem, float value) {
    auto* slot = resolve(mod, elem, UiSlotKind::Progress, "elem_set_progress");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (slot->hasElementValue && slot->elementFloat == value) {
        return MOD_OK;
    }
    slot->elementFloat = value;
    slot->hasElementValue = true;
    slot->element->SetAttribute("value", value);
    return MOD_OK;
}

ModResult ui_elem_set_class(LoadedMod& mod, uint64_t elem, const char* name, bool active) {
    auto* entry = s_slots.find_owned(elem, mod);
    if (entry == nullptr || entry->value.element == nullptr) {
        Log.error(
            "[{}] elem_set_class: stale or invalid element handle {:#x}", mod.metadata.id, elem);
        return MOD_INVALID_ARGUMENT;
    }
    entry->value.element->SetClass(name, active);
    return MOD_OK;
}

ModResult ui_control_set_label(LoadedMod& mod, uint64_t handle, const char* label) {
    auto* slot = resolve(mod, handle, UiSlotKind::Control, "control_set_label");
    if (slot == nullptr || slot->component == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (slot->controlKind == UI_CONTROL_ICON_BUTTON) {
        if (label[0] == '\0') {
            return MOD_INVALID_ARGUMENT;
        }
        static_cast<ui::IconButton*>(slot->component)->set_label(label);
    } else if (slot->controlKind == UI_CONTROL_BUTTON) {
        static_cast<ui::Button*>(slot->component)->set_text(label);
    } else {
        static_cast<ui::SelectButton*>(slot->component)->set_key(label);
    }
    return MOD_OK;
}

ModResult ui_control_set_icon(LoadedMod& mod, uint64_t handle, const char* icon) {
    auto* slot = resolve(mod, handle, UiSlotKind::Control, "control_set_icon");
    if (slot == nullptr || slot->controlKind != UI_CONTROL_ICON_BUTTON ||
        slot->component == nullptr)
    {
        return MOD_INVALID_ARGUMENT;
    }
    static_cast<ui::IconButton*>(slot->component)->set_icon(icon);
    return MOD_OK;
}

ModResult ui_control_set_tooltip(LoadedMod& mod, uint64_t handle, const char* text) {
    auto* slot = resolve(mod, handle, UiSlotKind::Control, "control_set_tooltip");
    if (slot == nullptr || slot->component == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    slot->component->set_tooltip(text != nullptr ? text : "");
    return MOD_OK;
}

ModResult ui_control_set_options(
    LoadedMod& mod, uint64_t handle, std::vector<ui::DropdownButton::Option> options) {
    auto* slot = resolve(mod, handle, UiSlotKind::Control, "control_set_options");
    if (slot == nullptr || slot->controlKind != UI_CONTROL_DROPDOWN || slot->component == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    static_cast<ui::DropdownButton*>(slot->component)->set_options(std::move(options));
    return MOD_OK;
}

ModResult ui_elem_set_visible(LoadedMod& mod, uint64_t handle, bool visible) {
    auto* slot = resolve_element(mod, handle);
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (slot->hidden == !visible) {
        return MOD_OK;
    }
    auto element = slot->element->GetObserverPtr();
    auto* context = element->GetContext();
    bool restoreFocus = context != nullptr && element->Contains(context->GetFocusElement());
    slot->hidden = !visible;
    if (visible) {
        if (slot->displayOverride) {
            element->SetProperty(Rml::PropertyId::Display, *slot->displayOverride);
            slot->displayOverride.reset();
        } else {
            element->RemoveProperty(Rml::PropertyId::Display);
        }
    } else {
        const auto& properties = element->GetLocalStyleProperties();
        if (const auto it = properties.find(Rml::PropertyId::Display); it != properties.end()) {
            slot->displayOverride = std::make_unique<Rml::Property>(it->second);
        }
        ui::set_display(element.get(), Rml::Style::Display::None);
    }
    // Focus and menu callbacks may allocate slots.
    element->GetOwnerDocument()->UpdateDocument();
    if (!visible && element != nullptr) {
        std::vector<ui::ContextMenu*> menus;
        s_slots.for_each([&](uint64_t, const auto& entry) {
            if (entry.value.kind == UiSlotKind::ContextMenu) {
                auto* anchor = slot_from_handle(entry.value.anchorHandle);
                if (anchor != nullptr && element->Contains(anchor->element)) {
                    menus.push_back(static_cast<ui::ContextMenu*>(entry.value.document));
                }
            }
        });
        for (auto* menu : menus) {
            restoreFocus = restoreFocus || menu->has_focus();
            menu->dismiss(false);
        }
        if (restoreFocus) {
            if (auto* top = ui::top_document(); top && top->owns_element(element.get())) {
                top->focus();
            }
        }
    }
    return MOD_OK;
}

ModResult ui_elem_focus(LoadedMod& mod, uint64_t handle) {
    auto* slot = resolve_element(mod, handle);
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* element = slot->element;
    auto* top = ui::top_document();
    if (top == nullptr || !top->owns_element(element)) {
        return MOD_UNAVAILABLE;
    }
    element->GetOwnerDocument()->UpdateDocument();
    slot = resolve_element(mod, handle);
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (!element_available(*slot)) {
        return MOD_UNAVAILABLE;
    }
    slot = resolve_element(mod, handle);
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* component = slot->component;
    return (component != nullptr ? component->focus() : element->Focus(true)) ? MOD_OK :
                                                                                MOD_UNAVAILABLE;
}

class ModContextMenu final : public ui::ContextMenu {
public:
    ModContextMenu(Rml::Element* anchor, std::vector<Item> items, uint64_t anchorHandle)
        : ContextMenu{anchor, std::move(items)}, mAnchorHandle{anchorHandle} {}

    void update() override {
        auto* anchor = slot_from_handle(mAnchorHandle);
        if (visible() && (anchor == nullptr || !element_available(*anchor))) {
            dismiss(false);
        }
        ContextMenu::update();
    }

private:
    uint64_t mAnchorHandle;
};

ModResult ui_context_menu_push(LoadedMod& mod, uint64_t anchorHandle,
    const std::vector<UiContextMenuItem>& source, uint64_t& outHandle) {
    auto* anchor = resolve_element(mod, anchorHandle);
    if (anchor == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto* top = ui::top_document();
    auto* element = anchor->element;
    if (top == nullptr || !top->owns_element(anchor->element) || !element_available(*anchor)) {
        return MOD_UNAVAILABLE;
    }
    if (!slot_live(anchorHandle)) {
        return MOD_INVALID_ARGUMENT;
    }
    std::vector<ui::ContextMenu::Item> items;
    items.reserve(source.size());
    for (const auto& item : source) {
        items.push_back({
            .text = item.label,
            .icon = item.icon != nullptr ? item.icon : "",
            .onPressed =
                [modPtr = &mod, anchorHandle, fn = item.on_pressed, data = item.user_data] {
                    if (slot_live(anchorHandle) && fn != nullptr) {
                        guarded_call(*modPtr, "context menu action",
                            [&] { fn(modPtr->context.get(), data); });
                    }
                },
            .enabled = item.enabled && item.on_pressed != nullptr,
            .destructive = item.destructive,
            .separatorBefore = item.separator_before,
            .selected = item.selected,
        });
    }
    auto menu = std::make_unique<ModContextMenu>(element, std::move(items), anchorHandle);
    menu->body()->SetAttribute("mod-id", mod.metadata.id);
    auto& slot = alloc_slot(mod, UiSlotKind::ContextMenu, outHandle);
    slot.document = menu.get();
    slot.anchorHandle = anchorHandle;
    menu->on_close([handle = outHandle] { s_slots.erase(handle); });
    ui::push_document(std::move(menu));
    return MOD_OK;
}

ModResult ui_context_menu_close(LoadedMod& mod, uint64_t handle) {
    auto* slot = resolve(mod, handle, UiSlotKind::ContextMenu, "context_menu_close");
    if (slot == nullptr || slot->document == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    static_cast<ui::ContextMenu*>(slot->document)->dismiss();
    return MOD_OK;
}

ModResult ui_window_push(LoadedMod& mod, const UiWindowDesc& desc, uint64_t& outHandle) {
    outHandle = 0;
    if (!aurora::rmlui::is_initialized()) {
        return MOD_UNAVAILABLE;
    }
    if (desc.rcss != nullptr && desc.rcss[0] != '\0' &&
        Rml::Factory::InstanceStyleSheetString(desc.rcss) == nullptr)
    {
        Log.error("[{}] window_push: failed to parse window RCSS", mod.metadata.id);
        return MOD_INVALID_ARGUMENT;
    }

    uint64_t handle = 0;
    {
        auto& slot = alloc_slot(mod, UiSlotKind::Window, handle);
        slot.onClosed = desc.on_closed;
        slot.onClosedUserData = desc.user_data;
    }

    ui::ModWindow::Desc windowDesc;
    windowDesc.modId = mod.metadata.id;
    windowDesc.rcss = desc.rcss != nullptr ? desc.rcss : "";
    windowDesc.onDestroyed = [handle] { on_mod_window_destroyed(handle); };
    for (size_t i = 0; i < desc.tab_count; ++i) {
        const UiTabDesc& tab = desc.tabs[i];
        ui::ModWindow::Tab hostTab;
        hostTab.title = tab.title;
        hostTab.build = [modPtr = &mod, handle, build = tab.build, userData = tab.user_data](
                            ui::ModWindow&, ui::Pane& left, ui::Pane& right) {
            if (build == nullptr || !slot_live(handle) || !modPtr->active) {
                return;
            }
            const uint64_t leftHandle = wrap_pane(*modPtr, left, &right);
            const uint64_t rightHandle = wrap_pane(*modPtr, right, nullptr);
            invoke_mod_ui_callback(*modPtr, "mod UI tab build", [&](ModError* error) {
                return build(
                    modPtr->context.get(), handle, leftHandle, rightHandle, userData, error);
            });
        };
        if (tab.update != nullptr) {
            hostTab.update = [modPtr = &mod, handle, update = tab.update,
                                 userData = tab.user_data] {
                if (!slot_live(handle) || !modPtr->active) {
                    return;
                }
                invoke_mod_ui_callback(*modPtr, "mod UI tab update", [&](ModError* error) {
                    return update(modPtr->context.get(), userData, error);
                });
            };
        }
        windowDesc.tabs.push_back(std::move(hostTab));
    }

    // The first tab builds during construction, which can allocate slots; only
    // re-resolve the window slot afterwards.
    auto window = std::make_unique<ui::ModWindow>(std::move(windowDesc));
    if (auto* slot = slot_from_handle(handle)) {
        slot->document = window.get();
    }
    push_stacked_document(std::move(window));
    outHandle = handle;
    return MOD_OK;
}

ModResult ui_window_close(LoadedMod& mod, uint64_t handle) {
    auto* slot = resolve(mod, handle, UiSlotKind::Window, "window_close");
    if (slot == nullptr || slot->document == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    slot->document->pop();
    return MOD_OK;
}

ModResult ui_dialog_push(LoadedMod& mod, const UiDialogDesc& desc, uint64_t& outHandle) {
    outHandle = 0;
    if (!aurora::rmlui::is_initialized()) {
        return MOD_UNAVAILABLE;
    }
    uint64_t handle = 0;
    alloc_slot(mod, UiSlotKind::Dialog, handle);

    const char* defaultIcon = "";
    ui::Modal::Props props;
    switch (desc.variant) {
    case UI_DIALOG_WARNING:
        defaultIcon = "warning";
        break;
    case UI_DIALOG_DANGER:
        props.variant = "danger";
        defaultIcon = "error";
        break;
    default:
        break;
    }
    props.title = desc.title;
    props.bodyRml = desc.body_rml;
    props.icon = desc.icon != nullptr ? desc.icon : defaultIcon;
    props.onDismiss = [modPtr = &mod, handle, fn = desc.on_dismiss, userData = desc.user_data](
                          ui::Modal& modal) {
        if (!dialog_open(handle)) {
            return;  // already being torn down
        }
        if (fn != nullptr) {
            guarded_call(*modPtr, "dialog on_dismiss callback",
                [&] { fn(modPtr->context.get(), handle, userData); });
        }
        if (dialog_open(handle)) {
            static_cast<ModDialog&>(modal).close();
        }
    };
    auto* actionData = reinterpret_cast<const std::byte*>(desc.actions);
    for (size_t i = 0; i < desc.action_count; ++i) {
        const auto& action = *reinterpret_cast<const UiDialogAction*>(actionData);
        props.actions.push_back(make_dialog_action(mod, handle, action));
        actionData += action.struct_size;
    }

    auto dialog = std::make_unique<ModDialog>(
        std::move(props), [handle] { on_mod_dialog_destroyed(handle); });
    if (desc.build != nullptr) {
        auto& pane = dialog->content_pane();
        const uint64_t paneHandle = wrap_pane(mod, pane, nullptr);
        invoke_mod_ui_callback(mod, "mod UI dialog build", [&](ModError* error) {
            return desc.build(mod.context.get(), paneHandle, desc.user_data, error);
        });
        if (!mod.active) {
            return MOD_ERROR;
        }
    }
    if (auto* slot = slot_from_handle(handle)) {
        slot->document = dialog.get();
    }
    push_stacked_document(std::move(dialog));
    outHandle = handle;
    return MOD_OK;
}

ModResult ui_dialog_close(LoadedMod& mod, uint64_t handle) {
    auto* slot = resolve(mod, handle, UiSlotKind::Dialog, "dialog_close");
    if (slot == nullptr || slot->document == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    // Programmatic close: no dismiss notification, no sound
    static_cast<ModDialog*>(slot->document)->close();
    return MOD_OK;
}

ModResult ui_dialog_set_body(LoadedMod& mod, uint64_t handle, const char* rml) {
    auto* slot = resolve(mod, handle, UiSlotKind::Dialog, "dialog_set_body");
    if (slot == nullptr || slot->document == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    static_cast<ModDialog*>(slot->document)->set_body(rml);
    return MOD_OK;
}

ModResult ui_dialog_set_icon(LoadedMod& mod, uint64_t handle, const char* icon) {
    auto* slot = resolve(mod, handle, UiSlotKind::Dialog, "dialog_set_icon");
    if (slot == nullptr || slot->document == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    static_cast<ModDialog*>(slot->document)->set_icon(icon);
    return MOD_OK;
}

ModResult ui_register_menu_tab(LoadedMod& mod, const UiMenuTabDesc& desc, uint64_t& outHandle) {
    outHandle = 0;
    for (const auto& [owner, tabs] : s_modMenuTabs) {
        for (const auto& tab : tabs) {
            if (owner != &mod && tab.label == desc.label) {
                Log.warn("[{}] register_menu_tab: label '{}' is already used by [{}]",
                    mod.metadata.id, desc.label, owner->metadata.id);
            }
        }
    }
    uint64_t handle = 0;
    alloc_slot(mod, UiSlotKind::MenuTab, handle);
    s_modMenuTabs[&mod].push_back({.handle = handle,
        .label = desc.label,
        .onSelected = desc.on_selected,
        .userData = desc.user_data});
    s_menuTabsDirty = true;
    outHandle = handle;
    return MOD_OK;
}

ModResult ui_unregister_menu_tab(LoadedMod& mod, uint64_t handle) {
    auto* slot = resolve(mod, handle, UiSlotKind::MenuTab, "unregister_menu_tab");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    const auto it = s_modMenuTabs.find(&mod);
    if (it != s_modMenuTabs.end()) {
        std::erase_if(it->second, [&](const auto& tab) { return tab.handle == handle; });
        if (it->second.empty()) {
            s_modMenuTabs.erase(it);
        }
    }
    s_slots.erase_owned(handle, mod);
    s_menuTabsDirty = true;
    return MOD_OK;
}

std::vector<ModMenuTabEntry> ui_mod_menu_tabs() {
    // The consumer (a MenuBar being constructed) now reflects the current tab
    // set, so a pending rebuild for earlier mutations is moot.
    s_menuTabsDirty = false;
    std::vector<ModMenuTabEntry> entries;
    for (auto& mod : ModLoader::instance().mods()) {
        if (!mod.active) {
            continue;
        }
        const auto it = s_modMenuTabs.find(&mod);
        if (it == s_modMenuTabs.end()) {
            continue;
        }
        for (const auto& tab : it->second) {
            entries.push_back({.label = tab.label,
                .onSelected = [modPtr = &mod, handle = tab.handle, fn = tab.onSelected,
                                  userData = tab.userData] {
                    if (!slot_live(handle) || !modPtr->active) {
                        return;  // registered by a since-unloaded mod image
                    }
                    guarded_call(*modPtr, "menu tab on_selected callback",
                        [&] { fn(modPtr->context.get(), userData); });
                }});
        }
    }
    return entries;
}

void ui_sync_menu_tabs() {
    if (!s_menuTabsDirty) {
        return;
    }
    s_menuTabsDirty = false;
    if (aurora::rmlui::is_initialized()) {
        ui::MenuBar::refresh_tabs();
    }
}

bool ui_any_document_visible() {
    return ui::any_document_visible();
}

ModResult ui_register_styles(
    LoadedMod& mod, uint32_t scope, const char* rcss, uint64_t& outHandle) {
    outHandle = 0;
    ui::DocumentScope docScope;
    switch (scope) {
    case UI_SCOPE_PRELAUNCH:
        docScope = ui::DocumentScope::Prelaunch;
        break;
    case UI_SCOPE_WINDOW:
        docScope = ui::DocumentScope::Window;
        break;
    case UI_SCOPE_MENU_BAR:
        docScope = ui::DocumentScope::MenuBar;
        break;
    case UI_SCOPE_OVERLAY:
        docScope = ui::DocumentScope::Overlay;
        break;
    case UI_SCOPE_TOUCH_CONTROLS:
        docScope = ui::DocumentScope::TouchControls;
        break;
    case UI_SCOPE_GRAPHICS_TUNER:
        docScope = ui::DocumentScope::GraphicsTuner;
        break;
    default:
        return MOD_INVALID_ARGUMENT;
    }

    uint64_t handle = 0;
    auto& slot = alloc_slot(mod, UiSlotKind::Style, handle);
    slot.styleScope = docScope;
    slot.styleId = fmt::format("{}:{:x}", mod.metadata.id, handle);
    if (!ui::register_scoped_styles(docScope, slot.styleId, rcss)) {
        Log.error("[{}] register_styles: failed to parse RCSS", mod.metadata.id);
        s_slots.erase(handle);
        return MOD_INVALID_ARGUMENT;
    }
    outHandle = handle;
    return MOD_OK;
}

ModResult ui_register_styles_file(
    LoadedMod& mod, uint32_t scope, const char* path, uint64_t& outHandle) {
    outHandle = 0;
    if (mod.bundle == nullptr) {
        return MOD_UNAVAILABLE;
    }
    std::vector<u8> data;
    const std::string entry = std::string{"res/"} + path;
    try {
        data = mod.bundle->readFile(entry);
    } catch (const std::runtime_error& e) {
        Log.error("[{}] register_styles_file '{}' failed: {}", mod.metadata.id, entry, e.what());
        return MOD_UNAVAILABLE;
    }
    const std::string rcss{data.begin(), data.end()};
    return ui_register_styles(mod, scope, rcss.c_str(), outHandle);
}

ModResult ui_unregister_styles(LoadedMod& mod, uint64_t handle) {
    auto* slot = resolve(mod, handle, UiSlotKind::Style, "unregister_styles");
    if (slot == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    auto released = s_slots.take_owned(handle, mod);
    ui::unregister_scoped_styles(released->value.styleScope, released->value.styleId);
    return MOD_OK;
}

void ui_remove_mod(LoadedMod& mod) {
    s_modPanels.erase(&mod);
    if (s_modMenuTabs.erase(&mod) != 0) {
        s_menuTabsDirty = true;
    }
    bool restoreCoveredDocument = false;
    auto entries = s_slots.take_all(mod);
    for (auto& entry : entries) {
        auto& slot = entry.value;
        switch (slot.kind) {
        case UiSlotKind::Window: {
            auto* window = static_cast<ui::ModWindow*>(slot.document);
            if (window != nullptr) {
                restoreCoveredDocument |= ui::top_document() == window;
                window->force_hide(true);
            }
            break;
        }
        case UiSlotKind::Dialog: {
            auto* dialog = static_cast<ModDialog*>(slot.document);
            if (dialog != nullptr) {
                restoreCoveredDocument |= ui::top_document() == dialog;
                dialog->force_hide(true);
            }
            break;
        }
        case UiSlotKind::ContextMenu:
            if (slot.document != nullptr) {
                slot.document->force_hide(true);
            }
            break;
        case UiSlotKind::Style:
            ui::unregister_scoped_styles(slot.styleScope, slot.styleId);
            break;
        default:
            break;
        }
    }
    if (restoreCoveredDocument) {
        ui::uncover_top_document();
    }
}

ModResult ui_get_clipboard_text(
    LoadedMod& mod, char* buffer, size_t bufferSize, size_t* outLength) {
    if (outLength != nullptr) {
        *outLength = 0;
    }

    std::string text;
    if (SDL_HasClipboardText()) {
        char* textPtr = SDL_GetClipboardText();
        text = textPtr != nullptr ? textPtr : "";
        SDL_free(textPtr);
        if (text.empty()) {
            return MOD_ERROR;
        }
    }

    if (outLength != nullptr) {
        *outLength = text.size();
    }

    if (buffer == nullptr) {
        return MOD_OK;
    }
    if (bufferSize < text.size() + 1) {
        return MOD_INVALID_ARGUMENT;
    }

    memcpy(buffer, text.c_str(), text.size() + 1);
    return MOD_OK;
}

ModResult ui_set_clipboard_text(LoadedMod& mod, const char* text) {
    if (!SDL_SetClipboardText(text)) {
        return MOD_ERROR;
    }
    return MOD_OK;
}

}  // namespace dusk::mods::svc::ui_impl

namespace dusk::mods::svc {
namespace {

// Validation of the tagged control descriptor: required fields per kind/binding. Value
// translation and cvar wiring live in loader/ui.cpp.
bool valid_color_preset(const char* value, bool alpha) {
    const std::string_view text{value};
    if (text == "rainbow") {
        return true;
    }
    if (text.size() != 6 && (!alpha || text.size() != 8)) {
        return false;
    }
    return std::ranges::all_of(text, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

bool valid_control_desc(const UiControlDesc& desc) {
    constexpr size_t kLegacyDescSize = offsetof(UiControlDesc, color_presets);
    constexpr size_t kColorDescSize = offsetof(UiControlDesc, is_selected);
    if (desc.struct_size < kLegacyDescSize || desc.label == nullptr) {
        return false;
    }
    switch (desc.kind) {
    case UI_CONTROL_ICON_BUTTON:
        return desc.struct_size >= kUiControlIconSize && desc.icon != nullptr &&
               desc.label[0] != '\0' && ui::material_icon(desc.icon)[0] != '\0' &&
               desc.on_pressed != nullptr;
    case UI_CONTROL_BUTTON:
    case UI_CONTROL_GROUP:
        return desc.on_pressed != nullptr;
    case UI_CONTROL_TOGGLE:
    case UI_CONTROL_NUMBER:
    case UI_CONTROL_STRING:
    case UI_CONTROL_SELECT:
    case UI_CONTROL_DROPDOWN:
        break;
    case UI_CONTROL_COLOR:
        if (desc.struct_size < kColorDescSize) {
            return false;
        }
        break;
    case UI_CONTROL_FILE_PICKER:
        if (desc.struct_size < kUiControlFilePickerSize ||
            (desc.file_filter_count != 0 && desc.file_filters == nullptr))
        {
            return false;
        }
        for (size_t i = 0; i < desc.file_filter_count; ++i) {
            if (desc.file_filters[i].name == nullptr || desc.file_filters[i].pattern == nullptr) {
                return false;
            }
        }
        break;
    default:
        return false;
    }
    if (desc.kind == UI_CONTROL_STRING && desc.struct_size >= kUiControlStringSetModeSize &&
        desc.string_set_mode != UI_STRING_SET_ON_COMMIT &&
        desc.string_set_mode != UI_STRING_SET_ON_CHANGE)
    {
        return false;
    }
    if (desc.kind == UI_CONTROL_SELECT || desc.kind == UI_CONTROL_DROPDOWN) {
        if (desc.options == nullptr || desc.option_count == 0 || desc.option_count > INT_MAX) {
            return false;
        }
        for (size_t i = 0; i < desc.option_count; ++i) {
            if (desc.options[i] == nullptr) {
                return false;
            }
        }
    }
    if (desc.kind == UI_CONTROL_COLOR && desc.color_preset_count != 0) {
        if (desc.color_presets == nullptr) {
            return false;
        }
        for (size_t i = 0; i < desc.color_preset_count; ++i) {
            if (desc.color_presets[i] == nullptr ||
                !valid_color_preset(desc.color_presets[i], desc.color_alpha))
            {
                return false;
            }
        }
    }
    switch (desc.binding) {
    case UI_BINDING_CALLBACKS:
        return desc.get != nullptr && desc.set != nullptr;
    case UI_BINDING_CONFIG_VAR:
        return desc.config_var != 0;
    default:
        return false;
    }
}

bool copy_list_items(
    const UiListItem* items, size_t itemCount, std::vector<ui::List::Item>& outItems) {
    if (itemCount != 0 && items == nullptr) {
        return false;
    }

    std::vector<ui::List::Item> copy;
    copy.reserve(itemCount);
    std::unordered_set<uint64_t> keys;
    keys.reserve(itemCount);
    auto cursor = reinterpret_cast<uintptr_t>(items);
    for (size_t i = 0; i < itemCount; ++i) {
        const auto* item = reinterpret_cast<const UiListItem*>(cursor);
        const size_t recordSize = item->struct_size;
        if (recordSize < kUiListItemV21Size || recordSize % alignof(UiListItem) != 0 ||
            cursor > std::numeric_limits<uintptr_t>::max() - recordSize)
        {
            return false;
        }
        if (item->label == nullptr || !keys.insert(item->key).second) {
            return false;
        }
        copy.push_back({.key = item->key, .label = item->label});
        cursor += recordSize;
    }

    outItems = std::move(copy);
    return true;
}

template <typename T, typename Fn>
bool copy_records(const T* records, size_t count, size_t minimumSize, Fn&& copy) {
    if (count > INT_MAX || (count != 0 && records == nullptr)) {
        return false;
    }
    auto cursor = reinterpret_cast<uintptr_t>(records);
    for (size_t i = 0; i < count; ++i) {
        const auto* record = reinterpret_cast<const T*>(cursor);
        const size_t size = record->struct_size;
        if (size < minimumSize || size % alignof(T) != 0 ||
            cursor > std::numeric_limits<uintptr_t>::max() - size || !copy(*record))
        {
            return false;
        }
        cursor += size;
    }
    return true;
}

ModResult ui_control_set_label(ModContext* context, UiElementHandle handle, const char* label) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0 || label == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_control_set_label(*mod, handle, label);
}

ModResult ui_control_set_icon(ModContext* context, UiElementHandle handle, const char* icon) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0 || icon == nullptr || ui::material_icon(icon)[0] == '\0') {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_control_set_icon(*mod, handle, icon);
}

ModResult ui_control_set_tooltip(ModContext* context, UiElementHandle handle, const char* text) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_control_set_tooltip(*mod, handle, text);
}

ModResult ui_control_set_options(
    ModContext* context, UiElementHandle handle, const UiControlOption* options, size_t count) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    std::vector<ui::DropdownButton::Option> copy;
    if (!copy_records(options, count, offsetof(UiControlOption, enabled) + sizeof(bool),
            [&](const UiControlOption& option) {
                if (option.label == nullptr) {
                    return false;
                }
                copy.push_back({option.label, option.enabled});
                return true;
            }))
    {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_control_set_options(*mod, handle, std::move(copy));
}

ModResult ui_elem_set_visible(ModContext* context, UiElementHandle handle, bool visible) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_elem_set_visible(*mod, handle, visible);
}

ModResult ui_elem_focus(ModContext* context, UiElementHandle handle) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_elem_focus(*mod, handle);
}

ModResult ui_context_menu_push(ModContext* context, UiElementHandle anchor,
    const UiContextMenuDesc* desc, UiContextMenuHandle* outHandle) {
    if (outHandle != nullptr) {
        *outHandle = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || anchor == 0 || desc == nullptr ||
        desc->struct_size < sizeof(UiContextMenuDesc) || desc->item_count == 0)
    {
        return MOD_INVALID_ARGUMENT;
    }
    std::vector<UiContextMenuItem> items;
    if (!copy_records(desc->items, desc->item_count,
            offsetof(UiContextMenuItem, separator_before) + sizeof(bool),
            [&](const UiContextMenuItem& item) {
                if (item.label == nullptr || item.label[0] == '\0' ||
                    (item.icon != nullptr && item.icon[0] != '\0' &&
                        ui::material_icon(item.icon)[0] == '\0'))
                {
                    return false;
                }
                items.push_back(item);
                return true;
            }))
    {
        return MOD_INVALID_ARGUMENT;
    }
    uint64_t handle = 0;
    const auto result = ui_impl::ui_context_menu_push(*mod, anchor, items, handle);
    if (result == MOD_OK && outHandle != nullptr) {
        *outHandle = handle;
    }
    return result;
}

ModResult ui_context_menu_close(ModContext* context, UiContextMenuHandle handle) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_context_menu_close(*mod, handle);
}

ModResult ui_register_mods_panel(ModContext* context, const UiModsPanelDesc* desc) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || desc == nullptr || desc->struct_size < sizeof(UiModsPanelDesc) ||
        desc->build == nullptr)
    {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_register_mods_panel(*mod, *desc);
}

ModResult ui_pane_add_row(
    ModContext* context, UiElementHandle parent, const UiRowDesc* desc, UiElementHandle* outRow) {
    if (outRow != nullptr) {
        *outRow = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || parent == 0 || outRow == nullptr || desc == nullptr ||
        desc->struct_size < offsetof(UiRowDesc, wrap) + sizeof(bool) ||
        desc->align < UI_ROW_ALIGN_START || desc->align > UI_ROW_ALIGN_SPACE_BETWEEN)
    {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_pane_add_row(*mod, parent, *desc, outRow);
}

ModResult ui_pane_add_section(ModContext* context, UiElementHandle pane, const char* title) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || pane == 0 || title == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_pane_add_section(*mod, pane, title);
}

ModResult ui_pane_add_text(
    ModContext* context, UiElementHandle pane, const char* text, UiElementHandle* outElem) {
    if (outElem != nullptr) {
        *outElem = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || pane == 0 || text == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_pane_add_text(*mod, pane, text, outElem);
}

ModResult ui_pane_add_rml(
    ModContext* context, UiElementHandle pane, const char* rml, UiElementHandle* outElem) {
    if (outElem != nullptr) {
        *outElem = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || pane == 0 || rml == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_pane_add_rml(*mod, pane, rml, outElem);
}

ModResult ui_pane_add_progress(
    ModContext* context, UiElementHandle pane, float value, UiElementHandle* outElem) {
    if (outElem != nullptr) {
        *outElem = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || pane == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_pane_add_progress(*mod, pane, value, outElem);
}

ModResult ui_pane_add_control(ModContext* context, UiElementHandle pane, const UiControlDesc* desc,
    UiElementHandle* outElem) {
    if (outElem != nullptr) {
        *outElem = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || pane == 0 || desc == nullptr || !valid_control_desc(*desc)) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_pane_add_control(*mod, pane, *desc, outElem);
}

ModResult ui_pane_add_list(
    ModContext* context, UiElementHandle pane, const UiListDesc* desc, UiListHandle* outList) {
    if (outList != nullptr) {
        *outList = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || pane == 0 || desc == nullptr || desc->struct_size < kUiListDescV21Size ||
        desc->on_pressed == nullptr)
    {
        return MOD_INVALID_ARGUMENT;
    }
    std::vector<ui::List::Item> items;
    if ((desc->items != nullptr || desc->item_count > 0) &&
        !copy_list_items(desc->items, desc->item_count, items))
    {
        return MOD_INVALID_ARGUMENT;
    }

    uint64_t handle = 0;
    const ModResult result = ui_impl::ui_pane_add_list(*mod, pane, *desc, std::move(items), handle);
    if (result == MOD_OK && outList != nullptr) {
        *outList = handle;
    }
    return result;
}

ModResult ui_list_set_items(
    ModContext* context, UiListHandle list, const UiListItem* items, size_t itemCount) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || list == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    std::vector<ui::List::Item> copiedItems;
    if (!copy_list_items(items, itemCount, copiedItems)) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_list_set_items(*mod, list, std::move(copiedItems));
}

ModResult ui_pane_add_group(ModContext* context, UiElementHandle groupPane,
    UiElementHandle targetPane, const UiGroupDesc* desc, UiElementHandle* outElem) {
    if (outElem != nullptr) {
        *outElem = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || groupPane == 0 || targetPane == 0 || desc == nullptr ||
        desc->struct_size < sizeof(UiGroupDesc) || desc->label == nullptr || desc->build == nullptr)
    {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_pane_add_group(*mod, groupPane, targetPane, *desc, outElem);
}

ModResult ui_elem_set_text(ModContext* context, UiElementHandle elem, const char* text) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || elem == 0 || text == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_elem_set_text(*mod, elem, text);
}

ModResult ui_elem_set_rml(ModContext* context, UiElementHandle elem, const char* rml) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || elem == 0 || rml == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_elem_set_rml(*mod, elem, rml);
}

ModResult ui_elem_set_progress(ModContext* context, UiElementHandle elem, float value) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || elem == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_elem_set_progress(*mod, elem, value);
}

ModResult ui_elem_set_class(
    ModContext* context, UiElementHandle elem, const char* name, bool active) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || elem == 0 || name == nullptr || name[0] == '\0') {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_elem_set_class(*mod, elem, name, active);
}

ModResult ui_window_push(ModContext* context, const UiWindowDesc* desc, UiWindowHandle* outWindow) {
    if (outWindow != nullptr) {
        *outWindow = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || desc == nullptr || desc->struct_size < sizeof(UiWindowDesc) ||
        desc->tabs == nullptr || desc->tab_count == 0)
    {
        return MOD_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < desc->tab_count; ++i) {
        const UiTabDesc& tab = desc->tabs[i];
        if (tab.struct_size < sizeof(UiTabDesc) || tab.title == nullptr || tab.build == nullptr) {
            return MOD_INVALID_ARGUMENT;
        }
    }
    uint64_t handle = 0;
    const auto result = ui_impl::ui_window_push(*mod, *desc, handle);
    if (result == MOD_OK && outWindow != nullptr) {
        *outWindow = handle;
    }
    return result;
}

ModResult ui_window_close(ModContext* context, UiWindowHandle window) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || window == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_window_close(*mod, window);
}

ModResult ui_dialog_push(ModContext* context, const UiDialogDesc* desc, UiDialogHandle* outDialog) {
    if (outDialog != nullptr) {
        *outDialog = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || desc == nullptr || desc->struct_size < sizeof(UiDialogDesc) ||
        desc->title == nullptr || desc->body_rml == nullptr || desc->actions == nullptr ||
        desc->action_count == 0 || desc->variant > UI_DIALOG_DANGER)
    {
        return MOD_INVALID_ARGUMENT;
    }
    auto* actionData = reinterpret_cast<const std::byte*>(desc->actions);
    for (size_t i = 0; i < desc->action_count; ++i) {
        const auto* action = reinterpret_cast<const UiDialogAction*>(actionData);
        if (action->struct_size < sizeof(UiDialogAction) ||
            action->struct_size % alignof(UiDialogAction) != 0 || action->label == nullptr)
        {
            return MOD_INVALID_ARGUMENT;
        }
        actionData += action->struct_size;
    }
    uint64_t handle = 0;
    const auto result = ui_impl::ui_dialog_push(*mod, *desc, handle);
    if (result == MOD_OK && outDialog != nullptr) {
        *outDialog = handle;
    }
    return result;
}

ModResult ui_dialog_close(ModContext* context, UiDialogHandle dialog) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || dialog == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_dialog_close(*mod, dialog);
}

ModResult ui_is_any_document_visible(ModContext* context, bool* outVisible) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || outVisible == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    *outVisible = ui_impl::ui_any_document_visible();
    return MOD_OK;
}

ModResult ui_register_styles(
    ModContext* context, UiStyleScope scope, const char* rcss, UiStyleHandle* outStyle) {
    if (outStyle != nullptr) {
        *outStyle = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || rcss == nullptr || scope > UI_SCOPE_GRAPHICS_TUNER) {
        return MOD_INVALID_ARGUMENT;
    }
    uint64_t handle = 0;
    const auto result = ui_impl::ui_register_styles(*mod, scope, rcss, handle);
    if (result == MOD_OK && outStyle != nullptr) {
        *outStyle = handle;
    }
    return result;
}

ModResult ui_register_styles_file(
    ModContext* context, UiStyleScope scope, const char* path, UiStyleHandle* outStyle) {
    if (outStyle != nullptr) {
        *outStyle = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || path == nullptr || !is_safe_resource_path(path) ||
        scope > UI_SCOPE_GRAPHICS_TUNER)
    {
        return MOD_INVALID_ARGUMENT;
    }
    uint64_t handle = 0;
    const auto result = ui_impl::ui_register_styles_file(*mod, scope, path, handle);
    if (result == MOD_OK && outStyle != nullptr) {
        *outStyle = handle;
    }
    return result;
}

ModResult ui_unregister_styles(ModContext* context, UiStyleHandle style) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || style == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_unregister_styles(*mod, style);
}

ModResult ui_register_menu_tab(
    ModContext* context, const UiMenuTabDesc* desc, UiMenuTabHandle* outTab) {
    if (outTab != nullptr) {
        *outTab = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || desc == nullptr || desc->struct_size < sizeof(UiMenuTabDesc) ||
        desc->label == nullptr || desc->label[0] == '\0' || desc->on_selected == nullptr)
    {
        return MOD_INVALID_ARGUMENT;
    }
    uint64_t handle = 0;
    const auto result = ui_impl::ui_register_menu_tab(*mod, *desc, handle);
    if (result == MOD_OK && outTab != nullptr) {
        *outTab = handle;
    }
    return result;
}

ModResult ui_unregister_menu_tab(ModContext* context, UiMenuTabHandle tab) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || tab == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_unregister_menu_tab(*mod, tab);
}

ModResult ui_push_toast(ModContext* context, const UiToastDesc* desc) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || desc == nullptr || desc->struct_size < sizeof(UiToastDesc) ||
        ((desc->title_rml == nullptr || desc->title_rml[0] == '\0') &&
            (desc->body_rml == nullptr || desc->body_rml[0] == '\0')))
    {
        return MOD_INVALID_ARGUMENT;
    }

    constexpr uint32_t kDefaultDurationMs = 5000;
    const uint32_t durationMs = desc->duration_ms == 0 ? kDefaultDurationMs : desc->duration_ms;
    ui::push_toast({
        .type = desc->type != nullptr ? desc->type : "",
        .title = desc->title_rml != nullptr ? desc->title_rml : "",
        .content = desc->body_rml != nullptr ? desc->body_rml : "",
        .duration = std::chrono::milliseconds{durationMs},
        .modId = mod->metadata.id,
    });
    return MOD_OK;
}

ModResult ui_dialog_set_body(ModContext* context, UiDialogHandle dialog, const char* bodyRml) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || dialog == 0 || bodyRml == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_dialog_set_body(*mod, dialog, bodyRml);
}

ModResult ui_dialog_set_icon(ModContext* context, UiDialogHandle dialog, const char* icon) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || dialog == 0 || icon == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_dialog_set_icon(*mod, dialog, icon);
}

ModResult ui_get_clipboard_text(
    ModContext* ctx, char* buffer, size_t bufferSize, size_t* outLength) {
    auto* mod = mod_from_context(ctx);
    if (mod == nullptr || (buffer == nullptr && bufferSize != 0)) {
        return MOD_INVALID_ARGUMENT;
    }

    return ui_impl::ui_get_clipboard_text(*mod, buffer, bufferSize, outLength);
}

ModResult ui_set_clipboard_text(ModContext* ctx, const char* text) {
    auto* mod = mod_from_context(ctx);
    if (mod == nullptr || text == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    return ui_impl::ui_set_clipboard_text(*mod, text);
}

ModResult ui_v1_dialog_push(
    ModContext* context, const ui_v1::UiDialogDesc* desc, UiDialogHandle* outDialog) {
    if (outDialog != nullptr) {
        *outDialog = 0;
    }
    constexpr size_t kLegacyDescSize = offsetof(ui_v1::UiDialogDesc, build);
    if (desc == nullptr || desc->struct_size < kLegacyDescSize || desc->actions == nullptr ||
        desc->action_count == 0)
    {
        return MOD_INVALID_ARGUMENT;
    }

    std::vector<UiDialogAction> actions;
    actions.reserve(desc->action_count);
    for (size_t i = 0; i < desc->action_count; ++i) {
        UiDialogAction action = UI_DIALOG_ACTION_INIT;
        action.label = desc->actions[i].label;
        action.on_pressed = desc->actions[i].on_pressed;
        action.user_data = desc->actions[i].user_data;
        action.keep_open = desc->actions[i].keep_open;
        actions.push_back(action);
    }

    UiDialogDesc translated = UI_DIALOG_DESC_INIT;
    translated.title = desc->title;
    translated.body_rml = desc->body_rml;
    translated.variant = desc->variant;
    translated.icon = desc->icon;
    translated.actions = actions.data();
    translated.action_count = actions.size();
    translated.on_dismiss = desc->on_dismiss;
    translated.user_data = desc->user_data;
    translated.build = desc->struct_size >= sizeof(ui_v1::UiDialogDesc) ? desc->build : nullptr;
    return ui_dialog_push(context, &translated, outDialog);
}

ModResult ui_v1_dialog_add_action(
    ModContext* context, UiDialogHandle, const ui_v1::UiDialogAction*) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    log::write(
        mod->metadata.id, LOG_LEVEL_WARN, "UiService v1 dialog_add_action is no longer supported");
    return MOD_UNSUPPORTED;
}

constexpr ui_v1::UiService s_uiService_v1{
    .header = SERVICE_HEADER(ui_v1::UiService, ui_v1::kMajorVersion, ui_v1::kMinorVersion),
    .register_mods_panel = ui_register_mods_panel,
    .pane_add_section = ui_pane_add_section,
    .pane_add_text = ui_pane_add_text,
    .pane_add_rml = ui_pane_add_rml,
    .pane_add_progress = ui_pane_add_progress,
    .pane_add_control = ui_pane_add_control,
    .elem_set_text = ui_elem_set_text,
    .elem_set_rml = ui_elem_set_rml,
    .elem_set_progress = ui_elem_set_progress,
    .elem_set_class = ui_elem_set_class,
    .window_push = ui_window_push,
    .window_close = ui_window_close,
    .dialog_push = ui_v1_dialog_push,
    .dialog_close = ui_dialog_close,
    .dialog_set_body = ui_dialog_set_body,
    .dialog_set_icon = ui_dialog_set_icon,
    .dialog_add_action = ui_v1_dialog_add_action,
    .is_any_document_visible = ui_is_any_document_visible,
    .register_styles = ui_register_styles,
    .register_styles_file = ui_register_styles_file,
    .unregister_styles = ui_unregister_styles,
    .register_menu_tab = ui_register_menu_tab,
    .unregister_menu_tab = ui_unregister_menu_tab,
    .push_toast = ui_push_toast,
    .get_clipboard_text = ui_get_clipboard_text,
    .set_clipboard_text = ui_set_clipboard_text,
    .pane_add_group = ui_pane_add_group,
};

constexpr UiService s_uiService{
    .header = SERVICE_HEADER(UiService, UI_SERVICE_MAJOR, UI_SERVICE_MINOR),
    .register_mods_panel = ui_register_mods_panel,
    .pane_add_section = ui_pane_add_section,
    .pane_add_text = ui_pane_add_text,
    .pane_add_rml = ui_pane_add_rml,
    .pane_add_progress = ui_pane_add_progress,
    .pane_add_control = ui_pane_add_control,
    .pane_add_group = ui_pane_add_group,
    .elem_set_text = ui_elem_set_text,
    .elem_set_rml = ui_elem_set_rml,
    .elem_set_progress = ui_elem_set_progress,
    .elem_set_class = ui_elem_set_class,
    .window_push = ui_window_push,
    .window_close = ui_window_close,
    .dialog_push = ui_dialog_push,
    .dialog_close = ui_dialog_close,
    .dialog_set_body = ui_dialog_set_body,
    .dialog_set_icon = ui_dialog_set_icon,
    .is_any_document_visible = ui_is_any_document_visible,
    .register_styles = ui_register_styles,
    .register_styles_file = ui_register_styles_file,
    .unregister_styles = ui_unregister_styles,
    .register_menu_tab = ui_register_menu_tab,
    .unregister_menu_tab = ui_unregister_menu_tab,
    .push_toast = ui_push_toast,
    .get_clipboard_text = ui_get_clipboard_text,
    .set_clipboard_text = ui_set_clipboard_text,
    .pane_add_list = ui_pane_add_list,
    .list_set_items = ui_list_set_items,
    .pane_add_row = ui_pane_add_row,
    .control_set_label = ui_control_set_label,
    .control_set_icon = ui_control_set_icon,
    .control_set_options = ui_control_set_options,
    .control_set_tooltip = ui_control_set_tooltip,
    .elem_set_visible = ui_elem_set_visible,
    .elem_focus = ui_elem_focus,
    .context_menu_push = ui_context_menu_push,
    .context_menu_close = ui_context_menu_close,
};

}  // namespace

void ui_build_mods_panels(LoadedMod& mod, ui::Pane& pane) {
    ui_impl::ui_build_mods_panels(mod, pane);
}

void ui_update_mods_panels(LoadedMod& mod) {
    ui_impl::ui_update_mods_panels(mod);
}

std::vector<ModMenuTabEntry> ui_mod_menu_tabs() {
    return ui_impl::ui_mod_menu_tabs();
}

constinit const ServiceModule g_uiModule_v1{
    .id = UI_SERVICE_ID,
    .majorVersion = ui_v1::kMajorVersion,
    .minorVersion = ui_v1::kMinorVersion,
    .service = &s_uiService_v1,
};

constinit const ServiceModule g_uiModule{
    .id = UI_SERVICE_ID,
    .majorVersion = UI_SERVICE_MAJOR,
    .minorVersion = UI_SERVICE_MINOR,
    .service = &s_uiService,
    .modDetached = ui_impl::ui_remove_mod,
    .frameEnd = ui_impl::ui_sync_menu_tabs,
};

}  // namespace dusk::mods::svc
