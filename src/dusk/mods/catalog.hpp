#pragma once

#include <borealis/http.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dusk::mods::catalog {

enum class Sort {
    Downloads,
    Endorsements,
    Updated,
    Newest,
    Name,
};

struct Query {
    std::string search;
    std::string category;
    Sort sort = Sort::Downloads;
    int page = 1;
    bool thisDevice = true;
};

struct Category {
    std::string slug;
    std::string name;
    uint64_t modCount = 0;
};

struct Tag {
    std::string slug;
    std::string name;
};

struct Author {
    std::string name;
    std::string handle;
    bool official = false;
};

struct ImageSource {
    uint32_t width = 0;
    std::string pngUrl;
};

struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<ImageSource> sources;
};

struct Mod {
    std::string id;
    std::string name;
    std::string version;
    Author author;
    std::string summary;
    std::optional<Category> category;
    std::vector<Tag> tags;
    uint64_t downloads = 0;
    uint64_t endorsements = 0;
    std::string publishedAt;
    std::string updatedAt;
    uint64_t packageSize = 0;
    bool containsNativeCode = false;
    std::vector<std::string> supportedPlatforms;
    std::optional<Image> icon;
    std::optional<Image> banner;
};

struct Screenshot {
    std::string altText;
    Image image;
};

struct ServiceImport {
    std::string id;
    uint16_t major = 0;
    uint16_t minMinor = 0;
    bool optional = false;
};

struct Download {
    std::string url;
    std::string sha256;
    uint64_t size = 0;
};

struct Detail {
    Mod mod;
    std::string siteUrl;
    std::optional<std::string> sourceUrl;
    std::optional<std::string> license;
    std::string descriptionHtml;
    std::string changelogHtml;
    Download download;
    std::optional<uint32_t> modAbi;
    std::vector<Screenshot> screenshots;
    std::vector<ServiceImport> serviceImports;
};

struct Pagination {
    int page = 1;
    int pageSize = 0;
    int pageCount = 0;
    uint64_t total = 0;
};

struct Page {
    std::vector<Category> categories;
    std::vector<Mod> mods;
    Pagination pagination;
};

struct FetchResult {
    std::optional<Page> page;
    std::string error;
};

struct DetailFetchResult {
    std::optional<Detail> detail;
    std::string error;
};

/** Fetches one filtered page from the Dusklight catalog. */
borealis::Task<FetchResult> fetch_page(Query query);

/** Fetches the full catalog record for one mod. */
borealis::Task<DetailFetchResult> fetch_detail(std::string id);

}  // namespace dusk::mods::catalog
