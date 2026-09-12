#include "archive.hpp"

#include <borealis/io.hpp>
#include <fmt/format.h>
#include <miniz.h>

#include <array>
#include <mutex>
#include <span>
#include <stdexcept>
#include <system_error>

namespace dusk::archive {
namespace {

constexpr std::array ZipMagic{'P', 'K', '\x03', '\x04'};

PackageFormat detect_package_format(mz_zip_archive& zip) {
    size_t modManifests = 0;
    for (mz_uint index = 0, count = mz_zip_reader_get_num_files(&zip); index < count; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, index, &stat) ||
            mz_zip_reader_is_file_a_directory(&zip, index))
        {
            continue;
        }
        const std::string_view name{stat.m_filename};
        modManifests += name == "mod.json";
    }
    if (modManifests == 1) {
        return PackageFormat::Mod;
    }
    return PackageFormat::Unknown;
}

}  // namespace

struct ZipArchive::Impl {
    ~Impl() {
        if (open) {
            mz_zip_reader_end(&zip);
        }
    }

    static size_t read_zip(
        void* opaque, mz_uint64 offset, void* buffer, const size_t size) {
        auto& archive = *static_cast<Impl*>(opaque);
        std::error_code error;
        return archive.file.read_at(offset, {static_cast<std::byte*>(buffer), size}, error);
    }

    borealis::io::RandomAccessFile file;
    mz_zip_archive zip{};
    PackageFormat format = PackageFormat::Unknown;
    bool open = false;
    std::mutex mutex;
};

ZipArchive::ZipArchive(const std::filesystem::path& path) : m_impl{std::make_unique<Impl>()} {
    auto opened = borealis::io::RandomAccessFile::open(path);
    if (opened.status != borealis::io::Status::Ok) {
        throw std::runtime_error(opened.message);
    }
    m_impl->file = std::move(opened.file);

    std::array<char, ZipMagic.size()> header{};
    std::error_code error;
    const auto read = m_impl->file.read_at(
        0, {reinterpret_cast<std::byte*>(header.data()), header.size()}, error);
    if (error) {
        throw std::runtime_error(fmt::format("Reading ZIP magic failed: {}", error.message()));
    }
    if (read != header.size() || header != ZipMagic) {
        throw std::runtime_error("File does not have ZIP magic");
    }

    m_impl->zip.m_pRead = Impl::read_zip;
    m_impl->zip.m_pIO_opaque = m_impl.get();
    if (!mz_zip_reader_init(&m_impl->zip, m_impl->file.size(), 0)) {
        const auto zipError = mz_zip_get_last_error(&m_impl->zip);
        throw std::runtime_error(
            fmt::format("Opening ZIP failed: {}", mz_zip_get_error_string(zipError)));
    }
    m_impl->open = true;
    m_impl->format = detect_package_format(m_impl->zip);
}

ZipArchive::~ZipArchive() = default;

ZipArchive::ZipArchive(ZipArchive&&) noexcept = default;
ZipArchive& ZipArchive::operator=(ZipArchive&&) noexcept = default;

PackageFormat ZipArchive::package_format() const noexcept {
    return m_impl->format;
}

std::vector<uint8_t> ZipArchive::read_file(const std::string_view name) {
    std::lock_guard lock{m_impl->mutex};
    const std::string fileName{name};
    size_t size = 0;
    void* extracted = mz_zip_reader_extract_file_to_heap(&m_impl->zip, fileName.c_str(), &size, 0);
    if (extracted == nullptr) {
        throw std::runtime_error(fmt::format("File does not exist: {}", name));
    }

    const std::unique_ptr<void, decltype(&mz_free)> owner{extracted, &mz_free};
    const std::span data{static_cast<const uint8_t*>(owner.get()), size};
    std::vector<uint8_t> result;
    result.assign(data.begin(), data.end());
    return result;
}

std::vector<std::string> ZipArchive::file_names() {
    std::lock_guard lock{m_impl->mutex};
    std::vector<std::string> results;
    for (mz_uint index = 0, count = mz_zip_reader_get_num_files(&m_impl->zip); index < count;
        ++index)
    {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&m_impl->zip, index, &stat) ||
            mz_zip_reader_is_file_a_directory(&m_impl->zip, index))
        {
            continue;
        }
        results.emplace_back(stat.m_filename);
    }
    return results;
}

size_t ZipArchive::file_size(const std::string_view name) {
    std::lock_guard lock{m_impl->mutex};
    const std::string fileName{name};
    const auto index = mz_zip_reader_locate_file(&m_impl->zip, fileName.c_str(), nullptr, 0);
    if (index < 0) {
        throw std::runtime_error(fmt::format("Unable to locate file in ZIP: {}", name));
    }

    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&m_impl->zip, static_cast<mz_uint>(index), &stat)) {
        throw std::runtime_error(fmt::format("Unable to inspect file in ZIP: {}", name));
    }
    return static_cast<size_t>(stat.m_uncomp_size);
}

}  // namespace dusk::archive
