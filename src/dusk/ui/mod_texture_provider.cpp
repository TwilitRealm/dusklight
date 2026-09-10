#include "mod_texture_provider.hpp"

#include "dusk/mod_loader.hpp"
#include "runtime_image.hpp"

#include <fmt/format.h>

namespace dusk::ui {

std::string mod_image_source(const mods::LoadedMod& mod, std::string_view bundlePath) {
    return fmt::format("mod://{}/{}?rev={}", mod.metadata.id, bundlePath, mod.cacheGeneration);
}

}  // namespace dusk::ui

#ifdef AURORA_ENABLE_RMLUI

#include <RmlUi/Core.h>
#include <aurora/rmlui.hpp>
#include <borealis/log.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "dusk/mods/loader/loader.hpp"

namespace dusk::ui {
namespace {

constexpr borealis::Log Log{"dusk::ui::modTexture"};
constexpr std::string_view kScheme = "mod";
constexpr std::string_view kSourcePrefix = "mod://";
constexpr size_t kMaxCachedImages = 64;
constexpr size_t kMaxCachedImageBytes = 64 * 1024 * 1024;
constexpr size_t kMaxImageFileSize = 16 * 1024 * 1024;

std::unordered_map<std::string, DecodedImage>& image_cache() {
    static std::unordered_map<std::string, DecodedImage> cache;
    return cache;
}

std::string_view strip_query(std::string_view path) noexcept {
    const auto queryPos = path.find_first_of("?#");
    if (queryPos != std::string_view::npos) {
        path = path.substr(0, queryPos);
    }
    return path;
}

std::optional<DecodedImage> load_mod_image(std::string_view idAndPath, std::string_view source) {
    const auto slash = idAndPath.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= idAndPath.size()) {
        Log.warn("Malformed mod image source '{}'", source);
        return std::nullopt;
    }
    const std::string modId{idAndPath.substr(0, slash)};
    const std::string path{idAndPath.substr(slash + 1)};
    if (!mods::is_safe_resource_path(path)) {
        Log.warn("Unsafe path in mod image source '{}'", source);
        return std::nullopt;
    }

    std::shared_ptr<mods::ModBundle> bundle;
    for (const auto& mod : mods::ModLoader::instance().mods()) {
        if (mod.metadata.id == modId) {
            bundle = mod.bundle;
            break;
        }
    }
    if (bundle == nullptr) {
        Log.warn("Unknown mod in image source '{}'", source);
        return std::nullopt;
    }

    std::vector<u8> data;
    try {
        if (bundle->getFileSize(path) > kMaxImageFileSize) {
            Log.warn("Image '{}' exceeds the {} MiB limit", source, kMaxImageFileSize >> 20);
            return std::nullopt;
        }
        data = bundle->readFile(path);
    } catch (const std::runtime_error& e) {
        Log.warn("Failed to read image '{}': {}", source, e.what());
        return std::nullopt;
    }

    return decode_png(std::span{data.data(), data.size()}, source);
}

std::optional<aurora::rmlui::RuntimeTexture> mod_texture_provider(std::string_view source) {
    if (!source.starts_with(kSourcePrefix)) {
        return std::nullopt;
    }

    auto& cache = image_cache();
    const std::string key{source};
    auto it = cache.find(key);
    if (it == cache.end()) {
        auto image = load_mod_image(strip_query(source.substr(kSourcePrefix.size())), source);
        if (!image) {
            return std::nullopt;
        }
        if (image->pixels.size() > kMaxCachedImageBytes) {
            return std::nullopt;
        }
        size_t cachedBytes = 0;
        for (const auto& [source, cached] : cache) {
            cachedBytes += cached.pixels.size();
        }
        while (cache.size() >= kMaxCachedImages ||
               cachedBytes > kMaxCachedImageBytes - image->pixels.size())
        {
            const auto victim = cache.begin();
            cachedBytes -= victim->second.pixels.size();
            Rml::ReleaseTexture(victim->first);
            cache.erase(victim);
        }
        it = cache.emplace(key, std::move(*image)).first;
    }

    const auto& image = it->second;
    return aurora::rmlui::RuntimeTexture{
        .width = image.width,
        .height = image.height,
        .rgba8 =
            std::span{reinterpret_cast<const std::byte*>(image.pixels.data()), image.pixels.size()},
        .premultipliedAlpha = true,
        .generateMipmaps = true,
    };
}

}  // namespace

void register_mod_texture_provider() noexcept {
    aurora::rmlui::register_texture_provider(std::string{kScheme}, mod_texture_provider);
}

void unregister_mod_texture_provider() noexcept {
    aurora::rmlui::unregister_texture_provider(kScheme);
    for (const auto& [source, image] : image_cache()) {
        Rml::ReleaseTexture(source);
    }
    image_cache().clear();
}

}  // namespace dusk::ui

#else

namespace dusk::ui {

void register_mod_texture_provider() noexcept {}
void unregister_mod_texture_provider() noexcept {}

}  // namespace dusk::ui

#endif
