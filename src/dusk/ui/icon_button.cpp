#include "icon_button.hpp"

#include <utility>

namespace dusk::ui {

const char* material_icon(std::string_view name) {
    static constexpr std::pair<std::string_view, const char*> icons[] = {
        {"play_arrow", "\uE037"},
        {"pause", "\uE034"},
        {"stop", "\uE047"},
        {"replay", "\uE042"},
        {"skip_next", "\uE044"},
        {"skip_previous", "\uE045"},
        {"note_add", "\uE89C"},
        {"create_new_folder", "\uE2CC"},
        {"delete", "\uE872"},
        {"add", "\uE145"},
        {"remove", "\uE15B"},
        {"close", "\uE5CD"},
        {"check", "\uE5CA"},
        {"refresh", "\uE5D5"},
        {"file_download", "\uE2C4"},
        {"download", "\uF090"},
        {"schedule", "\uE8B5"},
        {"warning", "\uE002"},
        {"check_circle", "\uE86C"},
        {"favorite", "\uE87D"},
        {"arrow_back", "\uE5C4"},
        {"arrow_forward", "\uE5C8"},
        {"expand_less", "\uE5CE"},
        {"expand_more", "\uE5CF"},
        {"open_in_new", "\uE89E"},
        {"settings", "\uE8B8"},
        {"folder_open", "\uE2C8"},
        {"history", "\uE889"},
        {"queue_music", "\uE03D"},
        {"volume_up", "\uE050"},
        {"volume_off", "\uE04F"},
        {"shuffle", "\uE043"},
        {"repeat", "\uE040"},
        {"search", "\uE8B6"},
        {"info", "\uE88E"},
        {"description", "\uE873"},
        {"notes", "\uE26C"},
        {"resume", "\uF7D0"},
    };
    for (const auto& [key, glyph] : icons) {
        if (name == key) {
            return glyph;
        }
    }
    return "";
}

IconButton::IconButton(Rml::Element* parent, Props props)
    : ControlledButton{parent, ControlledButton::Props{
                                   .text = "",
                                   .isSelected = std::move(props.isSelected),
                                   .isDisabled = std::move(props.isDisabled),
                               }} {
    mRoot->SetClass("icon-button", true);
    set_label(props.label);
    mIcon = append(mRoot, "icon");
    mIcon->SetAttribute("aria-hidden", "true");
    set_icon(props.icon);
}

void IconButton::set_icon(std::string_view icon) {
    if (mIconName == icon) {
        return;
    }
    mIconName = icon;
    set_text_content(mIcon, material_icon(icon));
}

void IconButton::set_label(const Rml::String& label) {
    if (mLabel == label) {
        return;
    }
    mRoot->SetAttribute("aria-label", label);
    mLabel = label;
    if (mTooltipText.empty()) {
        Component::set_tooltip(label);
    }
}

void IconButton::set_tooltip(const Rml::String& text) {
    mTooltipText = text;
    Component::set_tooltip(text.empty() ? mLabel : text);
}

}  // namespace dusk::ui
