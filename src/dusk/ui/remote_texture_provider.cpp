#include "remote_texture_provider.hpp"

#include <fmt/format.h>

namespace dusk::ui {

std::string remote_image_source(std::string_view url, uint32_t width, uint32_t height) {
    url = url.substr(0, url.find('#'));
    return fmt::format("{}#size={}x{}", url, width == 0 ? 1 : width, height == 0 ? 1 : height);
}

}  // namespace dusk::ui

#ifdef AURORA_ENABLE_RMLUI

#include "dusk/app_info.hpp"
#include "runtime_image.hpp"

#include <RmlUi/Core.h>
#include <aurora/rmlui.hpp>
#include <borealis/http.hpp>
#include <borealis/log.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace dusk::ui {
namespace {

using namespace std::chrono_literals;

constexpr borealis::Log Log{"dusk::ui"};
constexpr std::string_view kScheme = "https";
constexpr std::string_view kAllowedPrefix = "https://staging.twilitrealm.workers.dev/images/v1/";
constexpr size_t kMaxCachedImages = 64;
constexpr size_t kMaxImageFileSize = 16 * 1024 * 1024;
// RmlUi caches the first texture dimensions in image decorators, so the async
// placeholder must preserve the final image's aspect ratio.
constexpr uint32_t kPlaceholderMaxDimension = 256;
constexpr std::array<std::byte, kPlaceholderMaxDimension * kPlaceholderMaxDimension * 4>
    kTransparentPixels{};

enum class State {
    Unrequested,
    Pending,
    Ready,
    Failed,
};

struct Entry {
    borealis::Task<borealis::http::Result> request;
    DecodedImage image;
    State state = State::Unrequested;
    uint64_t lastUsed = 0;
    uint32_t placeholderWidth = 1;
    uint32_t placeholderHeight = 1;
};

struct RemoteSource {
    std::string_view requestUrl;
    uint32_t placeholderWidth = 1;
    uint32_t placeholderHeight = 1;
};

std::unordered_map<std::string, Entry>& image_cache() {
    static std::unordered_map<std::string, Entry> cache;
    return cache;
}

uint64_t& use_counter() {
    static uint64_t counter = 0;
    return counter;
}

RemoteSource parse_remote_source(std::string_view source) noexcept {
    constexpr std::string_view marker{"#size="};
    const auto fragment = source.find('#');
    RemoteSource result{.requestUrl = source.substr(0, fragment)};
    if (fragment == std::string_view::npos || !source.substr(fragment).starts_with(marker)) {
        return result;
    }

    const auto dimensions = source.substr(fragment + marker.size());
    const auto separator = dimensions.find('x');
    if (separator == std::string_view::npos) {
        return result;
    }
    uint32_t width = 0;
    uint32_t height = 0;
    const auto widthResult =
        std::from_chars(dimensions.data(), dimensions.data() + separator, width);
    const auto heightResult = std::from_chars(
        dimensions.data() + separator + 1, dimensions.data() + dimensions.size(), height);
    if (widthResult.ec != std::errc{} || widthResult.ptr != dimensions.data() + separator ||
        heightResult.ec != std::errc{} ||
        heightResult.ptr != dimensions.data() + dimensions.size() || width == 0 || height == 0)
    {
        return result;
    }

    const auto maxDimension = std::max(width, height);
    if (maxDimension > kPlaceholderMaxDimension) {
        width = std::max(
            1u, static_cast<uint32_t>(
                    (static_cast<uint64_t>(width) * kPlaceholderMaxDimension + maxDimension / 2) /
                    maxDimension));
        height = std::max(
            1u, static_cast<uint32_t>(
                    (static_cast<uint64_t>(height) * kPlaceholderMaxDimension + maxDimension / 2) /
                    maxDimension));
    }
    result.placeholderWidth = width;
    result.placeholderHeight = height;
    return result;
}

bool make_cache_room() {
    auto& cache = image_cache();
    if (cache.size() < kMaxCachedImages) {
        return true;
    }
    const auto victim = std::ranges::min_element(cache, {}, [](const auto& pair) {
        return pair.second.state == State::Pending ? std::numeric_limits<uint64_t>::max() :
                                                     pair.second.lastUsed;
    });
    if (victim == cache.end() || victim->second.state == State::Pending) {
        return false;
    }
    cache.erase(victim);
    return true;
}

aurora::rmlui::RuntimeTexture transparent_texture(const Entry& entry) {
    const auto size = static_cast<size_t>(entry.placeholderWidth) *
                      static_cast<size_t>(entry.placeholderHeight) * 4;
    return {
        .width = entry.placeholderWidth,
        .height = entry.placeholderHeight,
        .rgba8 = std::span{kTransparentPixels}.first(size),
        .premultipliedAlpha = true,
    };
}

borealis::Task<borealis::http::Result> start_request(std::string source) {
    return borealis::http::start({
        .url = std::move(source),
        .headers =
            {
                {.name = "User-Agent", .value = borealis::user_agent(dusk::AppInfo)},
                {.name = "Accept", .value = "image/png"},
            },
        .connectTimeout = 10s,
        .idleTimeout = 10s,
        .totalTimeout = 30s,
        .maxBodyBytes = kMaxImageFileSize,
    });
}

std::optional<aurora::rmlui::RuntimeTexture> remote_texture_provider(std::string_view source) {
    const auto parsed = parse_remote_source(source);
    if (!parsed.requestUrl.starts_with(kAllowedPrefix)) {
        return std::nullopt;
    }

    auto& cache = image_cache();
    const std::string key{source};
    auto iter = cache.find(key);
    if (iter == cache.end()) {
        if (!make_cache_room()) {
            Log.warn("Remote image cache is full; skipping '{}'", source);
            return transparent_texture(Entry{
                .placeholderWidth = parsed.placeholderWidth,
                .placeholderHeight = parsed.placeholderHeight,
            });
        }
        iter = cache
                   .emplace(key,
                       Entry{
                           .placeholderWidth = parsed.placeholderWidth,
                           .placeholderHeight = parsed.placeholderHeight,
                       })
                   .first;
    }
    if (iter->second.state == State::Unrequested) {
        iter->second.request = start_request(std::string{parsed.requestUrl});
        iter->second.state = State::Pending;
    }
    iter->second.lastUsed = ++use_counter();
    if (iter->second.state != State::Ready) {
        return transparent_texture(iter->second);
    }

    const auto& image = iter->second.image;
    return aurora::rmlui::RuntimeTexture{
        .width = image.width,
        .height = image.height,
        .rgba8 =
            std::span{reinterpret_cast<const std::byte*>(image.pixels.data()), image.pixels.size()},
        .premultipliedAlpha = true,
    };
}

void finish_request(const std::string& source, Entry& entry, borealis::http::Result result) {
    if (result.error != borealis::http::Error::None) {
        entry.state = State::Failed;
        Log.warn("Failed to fetch image '{}': {}", source, result.message);
        return;
    }
    if (result.response.statusCode != 200) {
        entry.state = State::Failed;
        Log.warn("Image '{}' returned HTTP {}", source, result.response.statusCode);
        return;
    }
    const auto* data = reinterpret_cast<const uint8_t*>(result.response.body.data());
    auto image = decode_png(std::span{data, result.response.body.size()}, source);
    if (!image) {
        entry.state = State::Failed;
        return;
    }
    entry.image = std::move(*image);
    entry.state = State::Ready;

    // RmlUi cached the transparent placeholder while the request was in flight.
    Rml::ReleaseTexture(source);
}

}  // namespace

void register_remote_texture_provider() noexcept {
    aurora::rmlui::register_texture_provider(std::string{kScheme}, remote_texture_provider);
}

void unregister_remote_texture_provider() noexcept {
    aurora::rmlui::unregister_texture_provider(kScheme);
    image_cache().clear();
    use_counter() = 0;
}

void update_remote_texture_provider() noexcept {
    for (auto& [source, entry] : image_cache()) {
        if (entry.state != State::Pending || !entry.request.ready()) {
            continue;
        }
        try {
            if (auto result = entry.request.try_take()) {
                finish_request(source, entry, std::move(*result));
            }
        } catch (const std::exception& exception) {
            entry.state = State::Failed;
            Log.warn("Failed to fetch image '{}': {}", source, exception.what());
        } catch (...) {
            entry.state = State::Failed;
            Log.warn("Failed to fetch image '{}'", source);
        }
        entry.request = {};
    }
}

}  // namespace dusk::ui

#else

namespace dusk::ui {

void register_remote_texture_provider() noexcept {}
void unregister_remote_texture_provider() noexcept {}
void update_remote_texture_provider() noexcept {}

}  // namespace dusk::ui

#endif
