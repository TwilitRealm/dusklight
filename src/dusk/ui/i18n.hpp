#pragma once

#include "dusk/settings.h"

#include <string>
#include <string_view>

namespace dusk::ui::i18n {

using dusk::MenuLanguage;

/*
 * Loads res/l10n/<lang>.json for the configured menu language, falling back to
 * res/l10n/en.json for any missing keys. Call once during UI startup.
 */
void initialize() noexcept;
void shutdown() noexcept;

/*
 * Translate a string by its dot path (e.g. "menu.reset"). The English table stores
 * the source strings, so English keys double as source text.
 * Fallback order: current language -> English -> the key itself.
 */
std::string_view tr(std::string_view key) noexcept;

/*
 * Convenience wrapper returning a std::string for direct use with Rml::String APIs.
 */
inline std::string tr_str(std::string_view key) noexcept {
    return std::string{tr(key)};
}

/*
 * Raw source string for composite texts (e.g. "Press {key} to reset the game.").
 * Always comes from the English table so placeholders are formatted against the source.
 */
std::string_view tr_source(std::string_view key) noexcept;

/* Language currently in effect after Auto resolution. */
MenuLanguage current_language() noexcept;
/* Whether the current translation table comes from a non-English language. */
bool is_translated() noexcept;

/*
 * Rebuild open menus after a language change. Requested by the language selector and
 * consumed by ui::update() so documents rebuild outside of config callbacks.
 */
void request_refresh() noexcept;
bool consume_refresh_requested() noexcept;

/* The name of the CVar backing the menu language, for subscribers. */
std::string_view language_setting_name() noexcept;

}  // namespace dusk::ui::i18n
