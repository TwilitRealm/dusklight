#include "catalog.hpp"

#include "dusk/app_info.hpp"
#include "fmt/format.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace dusk::mods::catalog {
namespace {

using json = nlohmann::json;
using namespace std::chrono_literals;

constexpr std::string_view catalogUrl =
    "https://staging.twilitrealm.workers.dev/api/v1/games/dusklight/mods";

std::string_view sort_value(Sort sort) noexcept {
    switch (sort) {
    case Sort::Endorsements:
        return "endorsements";
    case Sort::Updated:
        return "updated";
    case Sort::Newest:
        return "newest";
    case Sort::Name:
        return "name";
    case Sort::Downloads:
    default:
        return "downloads";
    }
}

std::string_view catalog_platform() noexcept {
#if defined(_WIN32) && defined(_M_ARM64)
    return "windows-arm64";
#elif defined(_WIN32) && defined(_M_X64)
    return "windows-amd64";
#elif defined(__ANDROID__) && defined(__aarch64__)
    return "android-aarch64";
#elif defined(__APPLE__) && TARGET_OS_IOS
    return "ios-arm64";
#elif defined(__APPLE__) && !TARGET_OS_TV && defined(__aarch64__)
    return "macos-arm64";
#elif defined(__APPLE__) && !TARGET_OS_TV && defined(__x86_64__)
    return "macos-x86_64";
#elif defined(__linux__) && defined(__aarch64__)
    return "linux-aarch64";
#elif defined(__linux__) && defined(__x86_64__)
    return "linux-x86_64";
#else
    // The catalog rejects platforms outside its published package matrix.
    return {};
#endif
}

std::string url_encode(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (const unsigned char c : value) {
        const bool unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
                                c == '~';
        if (unreserved) {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0f]);
        }
    }
    return encoded;
}

void append_query(std::string& url, std::string_view name, std::string_view value) {
    fmt::format_to(std::back_inserter(url), "{}{}={}",
        url.find('?') == std::string::npos ? '?' : '&', name, url_encode(value));
}

std::string make_url(const Query& query) {
    std::string url{catalogUrl};
    if (!query.search.empty()) {
        append_query(url, "q", query.search);
    }
    if (!query.category.empty()) {
        append_query(url, "category", query.category);
    }
    append_query(url, "sort", sort_value(query.sort));
    append_query(url, "page", fmt::format("{}", std::max(query.page, 1)));
    if (query.thisDevice) {
        const auto platform = catalog_platform();
        if (!platform.empty()) {
            append_query(url, "platform", platform);
        }
    }
    return url;
}

std::string make_detail_url(std::string_view id) {
    return fmt::format("{}/{}", catalogUrl, url_encode(id));
}

const json& required_field(const json& object, const char* name) {
    if (!object.is_object()) {
        throw std::runtime_error{"expected an object"};
    }
    const auto iter = object.find(name);
    if (iter == object.end()) {
        throw std::runtime_error{fmt::format("missing field '{}'", name)};
    }
    return *iter;
}

std::string required_string(const json& object, const char* name) {
    const auto& value = required_field(object, name);
    if (!value.is_string()) {
        throw std::runtime_error{fmt::format("field '{}' is not a string", name)};
    }
    return value.get<std::string>();
}

bool required_bool(const json& object, const char* name) {
    const auto& value = required_field(object, name);
    if (!value.is_boolean()) {
        throw std::runtime_error{fmt::format("field '{}' is not a boolean", name)};
    }
    return value.get<bool>();
}

uint64_t required_count(const json& object, const char* name) {
    const auto& value = required_field(object, name);
    if (value.is_number_unsigned()) {
        return value.get<uint64_t>();
    }
    if (value.is_number_integer()) {
        const auto count = value.get<int64_t>();
        if (count >= 0) {
            return static_cast<uint64_t>(count);
        }
    }
    throw std::runtime_error{fmt::format("field '{}' is not a non-negative integer", name)};
}

int required_int(const json& object, const char* name) {
    const uint64_t value = required_count(object, name);
    if (value > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error{fmt::format("field '{}' is too large", name)};
    }
    return static_cast<int>(value);
}

std::optional<std::string> optional_string(const json& object, const char* name) {
    const auto& value = required_field(object, name);
    if (value.is_null()) {
        return std::nullopt;
    }
    if (!value.is_string()) {
        throw std::runtime_error{fmt::format("field '{}' is not a string or null", name)};
    }
    return value.get<std::string>();
}

uint16_t required_u16(const json& object, const char* name) {
    const auto value = required_count(object, name);
    if (value > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error{fmt::format("field '{}' is too large", name)};
    }
    return static_cast<uint16_t>(value);
}

Image parse_image(const json& value) {
    const auto width = required_count(value, "width");
    const auto height = required_count(value, "height");
    if (width > std::numeric_limits<uint32_t>::max() ||
        height > std::numeric_limits<uint32_t>::max())
    {
        throw std::runtime_error{"image dimensions are too large"};
    }
    Image image{
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };
    const auto& sources = required_field(value, "sources");
    if (!sources.is_array()) {
        throw std::runtime_error{"field 'sources' is not an array"};
    }
    image.sources.reserve(sources.size());
    for (const auto& source : sources) {
        const auto sourceWidth = required_count(source, "width");
        if (sourceWidth > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error{"image source width is too large"};
        }
        image.sources.push_back({
            .width = static_cast<uint32_t>(sourceWidth),
            .pngUrl = required_string(source, "png_url"),
        });
    }
    if (image.sources.empty()) {
        throw std::runtime_error{"image has no sources"};
    }
    return image;
}

Category parse_category(const json& value) {
    return {
        .slug = required_string(value, "slug"),
        .name = required_string(value, "name"),
        .modCount = required_count(value, "mod_count"),
    };
}

Category parse_mod_category(const json& value) {
    return {
        .slug = required_string(value, "slug"),
        .name = required_string(value, "name"),
    };
}

Tag parse_tag(const json& value) {
    return {
        .slug = required_string(value, "slug"),
        .name = required_string(value, "name"),
    };
}

Author parse_author(const json& value) {
    return {
        .name = required_string(value, "name"),
        .handle = required_string(value, "handle"),
        .official = required_bool(value, "official"),
    };
}

Mod parse_mod(const json& value) {
    Mod mod{
        .id = required_string(value, "id"),
        .name = required_string(value, "name"),
        .version = required_string(value, "version"),
        .author = parse_author(required_field(value, "author")),
        .summary = required_string(value, "summary"),
        .downloads = required_count(value, "downloads"),
        .endorsements = required_count(value, "endorsements"),
        .publishedAt = required_string(value, "published_at"),
        .updatedAt = required_string(value, "updated_at"),
        .packageSize = required_count(value, "package_size"),
        .containsNativeCode = required_bool(value, "contains_native_code"),
    };

    const auto& category = required_field(value, "category");
    if (!category.is_null()) {
        mod.category = parse_mod_category(category);
    }

    const auto& tags = required_field(value, "tags");
    if (!tags.is_array()) {
        throw std::runtime_error{"field 'tags' is not an array"};
    }
    mod.tags.reserve(tags.size());
    for (const auto& tag : tags) {
        mod.tags.push_back(parse_tag(tag));
    }

    const auto& platforms = required_field(value, "supported_platforms");
    if (!platforms.is_array()) {
        throw std::runtime_error{"field 'supported_platforms' is not an array"};
    }
    mod.supportedPlatforms.reserve(platforms.size());
    for (const auto& platform : platforms) {
        if (!platform.is_string()) {
            throw std::runtime_error{"supported platform is not a string"};
        }
        mod.supportedPlatforms.push_back(platform.get<std::string>());
    }

    const auto& icon = required_field(value, "icon");
    if (!icon.is_null()) {
        mod.icon = parse_image(icon);
    }
    const auto& banner = required_field(value, "banner");
    if (!banner.is_null()) {
        mod.banner = parse_image(banner);
    }
    return mod;
}

Detail parse_detail(std::string_view body) {
    const json root = json::parse(body);
    Detail detail{
        .mod = parse_mod(root),
        .siteUrl = required_string(root, "site_url"),
        .sourceUrl = optional_string(root, "source_url"),
        .license = optional_string(root, "license"),
        .descriptionHtml = required_string(root, "description_html"),
        .changelogHtml = required_string(root, "changelog_html"),
    };

    const auto& download = required_field(root, "download");
    if (!download.is_object()) {
        throw std::runtime_error{"field 'download' is not an object"};
    }
    detail.download = {
        .url = required_string(download, "url"),
        .sha256 = required_string(download, "sha256"),
        .size = required_count(download, "size"),
    };

    const auto& modAbi = required_field(root, "mod_abi");
    if (!modAbi.is_null()) {
        const auto value = required_count(root, "mod_abi");
        if (value > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error{"field 'mod_abi' is too large"};
        }
        detail.modAbi = static_cast<uint32_t>(value);
    }

    const auto& screenshots = required_field(root, "screenshots");
    if (!screenshots.is_array()) {
        throw std::runtime_error{"field 'screenshots' is not an array"};
    }
    detail.screenshots.reserve(screenshots.size());
    for (const auto& screenshot : screenshots) {
        detail.screenshots.push_back({
            .altText = required_string(screenshot, "alt_text"),
            .image = parse_image(required_field(screenshot, "image")),
        });
    }

    const auto& imports = required_field(root, "service_imports");
    if (!imports.is_array()) {
        throw std::runtime_error{"field 'service_imports' is not an array"};
    }
    detail.serviceImports.reserve(imports.size());
    for (const auto& import : imports) {
        detail.serviceImports.push_back({
            .id = required_string(import, "id"),
            .major = required_u16(import, "major"),
            .minMinor = required_u16(import, "min_minor"),
            .optional = required_bool(import, "optional"),
        });
    }
    return detail;
}

Page parse_page(std::string_view body) {
    const json root = json::parse(body);
    const auto& game = required_field(root, "game");
    if (required_string(game, "id") != "dusklight") {
        throw std::runtime_error{"catalog response is for a different game"};
    }

    Page page;
    const auto& categories = required_field(root, "categories");
    if (!categories.is_array()) {
        throw std::runtime_error{"field 'categories' is not an array"};
    }
    page.categories.reserve(categories.size());
    for (const auto& category : categories) {
        page.categories.push_back(parse_category(category));
    }

    const auto& mods = required_field(root, "mods");
    if (!mods.is_array()) {
        throw std::runtime_error{"field 'mods' is not an array"};
    }
    page.mods.reserve(mods.size());
    for (const auto& mod : mods) {
        page.mods.push_back(parse_mod(mod));
    }

    const auto& pagination = required_field(root, "pagination");
    page.pagination = {
        .page = required_int(pagination, "page"),
        .pageSize = required_int(pagination, "page_size"),
        .pageCount = required_int(pagination, "page_count"),
        .total = required_count(pagination, "total"),
    };
    return page;
}

std::string api_error(const borealis::http::Response& response) {
    try {
        const auto body = json::parse(response.body);
        const auto& error = required_field(body, "error");
        return required_string(error, "message");
    } catch (...) {
        return fmt::format("The catalog returned HTTP {}.", response.statusCode);
    }
}

FetchResult finish_request(borealis::http::Result result) {
    if (result.error != borealis::http::Error::None) {
        return {.error = result.message.empty() ? "The catalog request failed." :
                                                  std::move(result.message)};
    }
    if (result.response.statusCode != 200) {
        return {.error = api_error(result.response)};
    }
    try {
        return {.page = parse_page(result.response.body)};
    } catch (const std::exception& exception) {
        return {.error = fmt::format("The catalog response was invalid: {}", exception.what())};
    } catch (...) {
        return {.error = "The catalog response was invalid."};
    }
}

DetailFetchResult finish_detail_request(borealis::http::Result result) {
    if (result.error != borealis::http::Error::None) {
        return {.error =
                    result.message.empty() ? "The mod request failed." : std::move(result.message)};
    }
    if (result.response.statusCode != 200) {
        return {.error = api_error(result.response)};
    }
    try {
        return {.detail = parse_detail(result.response.body)};
    } catch (const std::exception& exception) {
        return {.error = fmt::format("The mod response was invalid: {}", exception.what())};
    } catch (...) {
        return {.error = "The mod response was invalid."};
    }
}

borealis::http::Request make_request(std::string url) {
    return {
        .url = std::move(url),
        .headers =
            {
                {.name = "User-Agent", .value = borealis::user_agent(dusk::AppInfo)},
                {.name = "Accept", .value = "application/json"},
            },
        .connectTimeout = 10s,
        .idleTimeout = 10s,
        .totalTimeout = 20s,
    };
}

}  // namespace

borealis::Task<FetchResult> fetch_page(Query query) {
    return borealis::http::start(make_request(make_url(query))).map(finish_request);
}

borealis::Task<DetailFetchResult> fetch_detail(std::string id) {
    return borealis::http::start(make_request(make_detail_url(id))).map(finish_detail_request);
}

}  // namespace dusk::mods::catalog
