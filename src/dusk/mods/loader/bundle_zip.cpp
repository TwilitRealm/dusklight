#include "loader.hpp"

#include <stdexcept>

namespace dusk::mods {

ModBundleZip::ModBundleZip(const std::filesystem::path& path) : m_archive{path} {
    if (m_archive.package_format() != archive::PackageFormat::Mod) {
        throw std::runtime_error("Archive is not a valid mod package");
    }
}

std::vector<u8> ModBundleZip::readFile(const std::string& fileName) {
    return m_archive.read_file(fileName);
}

std::vector<std::string> ModBundleZip::getFileNames() {
    return m_archive.file_names();
}

size_t ModBundleZip::getFileSize(const std::string& fileName) {
    return m_archive.file_size(fileName);
}

}  // namespace dusk::mods
