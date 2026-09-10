#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::archive {

enum class PackageFormat {
    Unknown,
    Mod,
    // Save,
};

class ZipArchive {
public:
    explicit ZipArchive(const std::filesystem::path& path);
    ~ZipArchive();

    ZipArchive(ZipArchive&&) noexcept;
    ZipArchive& operator=(ZipArchive&&) noexcept;
    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    PackageFormat package_format() const noexcept;
    std::vector<uint8_t> read_file(std::string_view name);
    std::vector<std::string> file_names();
    size_t file_size(std::string_view name);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace dusk::archive
