#include "manifest.hpp"

#include "loader.hpp"
#include "natives.hpp"
#include "packages.hpp"

#include "dusk/mods/log_buffer.hpp"
#include "dusk/utilities.hpp"

#include <borealis/io.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <utility>

using namespace std::string_literals;
namespace fs = std::filesystem;

namespace dusk::mods {
namespace {

class InvalidModDataException : public std::runtime_error {
public:
    explicit InvalidModDataException(const std::string& msg) : runtime_error(msg) {}

    explicit InvalidModDataException(const char* msg) : runtime_error(msg) {}
};

bool bundle_has_file(ModBundle& bundle, const std::string& path) {
    try {
        bundle.getFileSize(path);
        return true;
    } catch (const std::runtime_error&) {
        return false;
    }
}

std::string resolve_image_path(ModBundle& bundle, const std::string& modId, std::string_view key,
    const std::string& manifestPath, const std::string& defaultPath) {
    if (!manifestPath.empty()) {
        if (!utils::is_safe_resource_path(manifestPath)) {
            log::write(
                modId, LOG_LEVEL_WARN, "invalid {} path '{}' in mod.json", key, manifestPath);
        } else if (!bundle_has_file(bundle, manifestPath)) {
            log::write(
                modId, LOG_LEVEL_WARN, "{} path '{}' not found in bundle", key, manifestPath);
        } else {
            return manifestPath;
        }
    }
    if (bundle_has_file(bundle, defaultPath)) {
        return defaultPath;
    }
    return {};
}

uint16_t parse_runtime_version_component(std::string_view text, std::string_view fieldName) {
    uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() ||
        value > UINT16_MAX)
    {
        throw InvalidModDataException(fmt::format("Invalid {} in runtime version pin", fieldName));
    }
    return static_cast<uint16_t>(value);
}

std::optional<DelegatedModRuntime> parse_runtime(const nlohmann::json& manifest) {
    const auto field = manifest.find("runtime");
    if (field == manifest.end()) {
        return std::nullopt;
    }
    if (!field->is_string()) {
        throw InvalidModDataException("runtime must be a string");
    }

    const std::string pin = field->get<std::string>();
    const auto at = pin.rfind('@');
    if (at == std::string::npos || at == 0 || at + 1 == pin.size() || pin.find('@') != at ||
        at >= MOD_META_SERVICE_ID_SIZE)
    {
        throw InvalidModDataException(
            "runtime must be a service id followed by @major or @major.minor");
    }

    const std::string_view version{pin.data() + at + 1, pin.size() - at - 1};
    const auto dot = version.find('.');
    if (dot != std::string_view::npos && version.find('.', dot + 1) != std::string_view::npos) {
        throw InvalidModDataException("runtime version pin has too many components");
    }

    DelegatedModRuntime result;
    result.id = pin.substr(0, at);
    result.major = parse_runtime_version_component(
        dot == std::string_view::npos ? version : version.substr(0, dot), "major version");
    if (dot != std::string_view::npos) {
        result.minMinor = parse_runtime_version_component(version.substr(dot + 1), "minor version");
    }
    return result;
}

}  // namespace

LoadedManifest load_manifest(const std::filesystem::path& modPath, ModBundle& bundle) {
    const auto metaJson = bundle.readFile("mod.json");
    auto j = nlohmann::json::parse(metaJson);

    std::string metaId = j.value("id", "");
    std::string metaName = j.value("name", "");
    std::string metaVersion = j.value("version", "");
    std::string metaAuthor = j.value("author", "");
    std::string metaDescription = j.value("description", "");
    std::string metaIcon = j.value("icon", "");
    std::string metaBanner = j.value("banner", "");

    if (!utils::is_valid_mod_id(metaId)) {
        throw InvalidModDataException{fmt::format(
            "Invalid mod ID '{}'; expected lowercase letters, digits, or underscores separated "
            "by single periods, with no leading or trailing period.",
            metaId)};
    }

    if (metaName.empty()) {
        metaName = borealis::io::fs_path_to_string(modPath.stem());
    }
    if (metaVersion.empty()) {
        metaVersion = "?"s;
    }
    if (metaAuthor.empty()) {
        metaAuthor = "unknown"s;
    }

    std::string iconPath = resolve_image_path(bundle, metaId, "icon", metaIcon, "res/icon.png"s);
    std::string bannerPath =
        resolve_image_path(bundle, metaId, "banner", metaBanner, "res/banner.png"s);

    return LoadedManifest{
        .metadata =
            {
                std::move(metaId),
                std::move(metaName),
                std::move(metaVersion),
                std::move(metaAuthor),
                std::move(metaDescription),
                std::move(iconPath),
                std::move(bannerPath),
            },
        .runtime = parse_runtime(j),
    };
}

bool inspect_mod_bundle(
    const fs::path& path, ModMetadata& metadata, std::string& error, bool* hasNative) noexcept {
    try {
        auto bundle = load_bundle(path, false);
        metadata = load_manifest(path, *bundle).metadata;
        if (hasNative != nullptr) {
            *hasNative = std::ranges::any_of(bundle->getFileNames(),
                [](const auto& name) { return has_native_library_extension(name); });
        }
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
    } catch (...) {
        error = "Unknown bundle validation error";
    }
    return false;
}

}  // namespace dusk::mods
