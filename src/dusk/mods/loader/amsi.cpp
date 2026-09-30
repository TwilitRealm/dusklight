#if _WIN32

#include "amsi.hpp"
#include "dusk/app_info.hpp"

#include "fmt/format.h"

#include <amsi.h>

#include "borealis/log.hpp"

namespace dusk::mods::loader::amsi {

namespace {

constexpr borealis::Log Log{"dusk::mods::loader::amsi"};

struct AmsiContext {
    HAMSICONTEXT context{};
    AmsiContext() {
        auto result = AmsiInitialize(AppNameW, &context);
        if (result != S_OK) {
            throw std::runtime_error(fmt::format("AMSI initialization failed: {}", result));
        }
    }

    ~AmsiContext() { AmsiUninitialize(context); }
};

bool check_for_malware_core(std::filesystem::path const& targetPath, std::span<u8 const> data) {
    AmsiContext context;

    auto targetPathWide = targetPath.wstring();
    auto dataPtr = const_cast<void*>(static_cast<void const*>(data.data()));
    AMSI_RESULT amsiResult = AMSI_RESULT_NOT_DETECTED;
    auto result = AmsiScanBuffer(
        context.context, dataPtr, data.size(), targetPathWide.c_str(), nullptr, &amsiResult);

    if (result != S_OK) {
        throw std::runtime_error(fmt::format("AMSI scanning failed: {}", result));
    }

    return AmsiResultIsMalware(amsiResult);
}

}  // namespace

bool check_for_malware(std::filesystem::path const& targetPath, std::span<u8 const> data) {
    try {
        Log.debug("Checking mod native with AMSI...");
        return check_for_malware_core(targetPath, data);
    } catch (std::runtime_error const& e) {
        Log.warn("AMSI checking threw an exception, allowing through: {}", e.what());
        return false;
    }
}

}  // namespace dusk::mods::loader::amsi

#endif