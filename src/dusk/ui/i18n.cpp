#include "i18n.hpp"

#include "dusk/logging.h"

#include <nlohmann/json.hpp>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_locale.h>

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace dusk::ui::i18n {
namespace {

using Json = nlohmann::json;

using StringMap = std::unordered_map<std::string, std::string>;

constexpr std::string_view kResourceDir = "res/l10n";

StringMap gTranslations;
StringMap gEnglish;
bool gTranslated = false;
MenuLanguage gCurrentLanguage = MenuLanguage::English;
std::atomic<bool> gRefreshRequested{false};

/*
 * Build the path for a locale file. Assets resolve relative to SDL_GetBasePath()
 * (or DUSK_ASSET_DIR when installed), mirroring ImGuiEngine::GetAssetPath.
 */
std::string locale_path(std::string_view language) {
    std::string path;
#ifdef DUSK_ASSET_DIR
    const char* basePath = DUSK_ASSET_DIR;
#else
    const char* basePath = SDL_GetBasePath();
#endif
    if (basePath != nullptr && basePath[0] != '\0') {
        path = basePath;
        if (path.back() != '/' && path.back() != '\\') {
            path += '/';
        }
    }
    path += kResourceDir;
    path += '/';
    path += language;
    path += ".json";
    return path;
}

void flatten(const std::string& prefix, const Json& node, StringMap& out) {
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it.key();
            if (!prefix.empty()) {
                key = prefix + "." + key;
            }
            flatten(key, it.value(), out);
        }
        return;
    }
    if (node.is_string()) {
        out.emplace(prefix, node.get<std::string>());
    }
}

bool load_language_file(std::string_view language, StringMap& out) {
    const auto path = locale_path(language);
    SDL_IOStream* stream = SDL_IOFromFile(path.c_str(), "rb");
    if (stream == nullptr) {
        return false;
    }
    const Sint64 size = SDL_GetIOSize(stream);
    std::string contents;
    if (size > 0) {
        contents.resize(static_cast<size_t>(size));
        const size_t read = SDL_ReadIO(stream, contents.data(), contents.size());
        contents.resize(read);
    }
    SDL_CloseIO(stream);

    try {
        const Json json = Json::parse(contents);
        flatten("", json, out);
    } catch (const Json::parse_error& e) {
        DuskLog.error("i18n: failed to parse {}: {}", path, e.what());
        return false;
    }
    return true;
}

/*
 * Detect the system locale with SDL and map it onto a supported language.
 */
MenuLanguage detect_system_language() {
    int localeCount = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&localeCount);
    if (locales == nullptr || localeCount <= 0) {
        return MenuLanguage::English;
    }

    MenuLanguage result = MenuLanguage::English;
    for (int i = 0; i < localeCount; ++i) {
        SDL_Locale* locale = locales[i];
        if (locale == nullptr) {
            continue;
        }
        const std::string_view language = locale->language != nullptr ? locale->language : "";
        if (language == "es") {
            result = MenuLanguage::Spanish;
            break;
        }
        if (language == "en") {
            result = MenuLanguage::English;
            break;
        }
    }
    SDL_free(locales);
    return result;
}

MenuLanguage resolve_language() {
    const auto configured = getSettings().ui.menuLanguage.getValue();
    if (configured == MenuLanguage::Auto) {
        return detect_system_language();
    }
    return configured;
}

void apply_language() {
    gTranslations.clear();
    gEnglish.clear();

    gCurrentLanguage = resolve_language();
    gTranslated = gCurrentLanguage != MenuLanguage::English;

    // Always load English: it doubles as the fallback for missing keys.
    if (!load_language_file("en", gEnglish)) {
        DuskLog.warn("i18n: English locale file missing; menu strings will show their keys");
    }

    if (gTranslated) {
        const auto name = gCurrentLanguage == MenuLanguage::Spanish ? "es" : "en";
        if (!load_language_file(name, gTranslations)) {
            DuskLog.warn("i18n: failed to load locale '{}'; falling back to English", name);
            gTranslations.clear();
            gTranslated = false;
            gCurrentLanguage = MenuLanguage::English;
        }
    }
}

const std::string* find_string(const StringMap& map, std::string_view key) {
    const auto it = map.find(std::string{key});
    return it != map.end() ? &it->second : nullptr;
}

}  // namespace

void initialize() noexcept {
    apply_language();
}

void shutdown() noexcept {
    gTranslations.clear();
    gEnglish.clear();
    gTranslated = false;
}

std::string_view tr(std::string_view key) noexcept {
    if (gTranslated) {
        if (const auto* value = find_string(gTranslations, key)) {
            return *value;
        }
    }
    if (const auto* value = find_string(gEnglish, key)) {
        return *value;
    }
    return key;
}

std::string_view tr_source(std::string_view key) noexcept {
    // The source text lives under the same key in the English table; keep a separate
    // accessor so callers document intent (raw source for composites vs translation).
    if (const auto* value = find_string(gEnglish, key)) {
        return *value;
    }
    return key;
}

MenuLanguage current_language() noexcept {
    return gCurrentLanguage;
}

bool is_translated() noexcept {
    return gTranslated;
}

void request_refresh() noexcept {
    // Reload the translation tables so rebuilt documents pick up the new language.
    apply_language();
    gRefreshRequested.store(true, std::memory_order_release);
}

bool consume_refresh_requested() noexcept {
    return gRefreshRequested.exchange(false, std::memory_order_acq_rel);
}

std::string_view language_setting_name() noexcept {
    return "ui.menuLanguage";
}

}  // namespace dusk::ui::i18n
