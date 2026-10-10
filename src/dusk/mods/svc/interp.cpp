#include "mods/svc/interp.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "dusk/interp/menus.h"
#include "internal.hpp"
#include "registry.hpp"

namespace dusk::mods::svc {

namespace {

struct ModDatum {
    absl::flat_hash_set<void const*> registered_matrices;
};

absl::flat_hash_map<LoadedMod const*, ModDatum> modData;

static_assert(std::is_same_v<InterpMtx, Mtx>);

bool lookup_replacement_impl(const void* key, InterpMtx out) {
    return interp::lookup_replacement(key, out);
}

ModResult record_final_mtx_keyed(ModContext* context, InterpMtx matrix, void const* key) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || !key || !matrix) {
        return MOD_INVALID_ARGUMENT;
    }

    auto& modDatum = modData[mod];
    modDatum.registered_matrices.insert(key);

    interp::record_final_mtx(matrix, key);

    return MOD_OK;
}

ModResult record_final_mtx(ModContext* context, InterpMtx matrix) {
    return record_final_mtx_keyed(context, matrix, matrix);
}

ModResult forget_mtx(ModContext* context, void const* key) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || !key) {
        return MOD_INVALID_ARGUMENT;
    }

    auto& modDatum = modData[mod];
    if (!modDatum.registered_matrices.erase(key)) {
        // Don't report an error in this case — avoids overcomplicating lifecycle management by
        // requiring callers to track if they've ever called record_final_mtx.
        return MOD_OK;
    }

    interp::forget_mtx(key);
    return MOD_OK;
}

void mod_detached(LoadedMod& mod) {
    auto const found = modData.find(&mod);
    if (found == modData.end()) {
        return;
    }

    auto& datum = found->second;
    for (auto const key : datum.registered_matrices) {
        interp::forget_mtx(key);
    }

    modData.erase(found);
}

constexpr InterpService s_interpService {
    .header = SERVICE_HEADER(InterpService, INTERP_SERVICE_MAJOR, INTERP_SERVICE_MINOR),
    .record_mtx_keyed = record_final_mtx_keyed,
    .record_mtx = record_final_mtx,
    .forget_mtx = forget_mtx,
    .lookup_replacement_mtx = lookup_replacement_impl,
};

}

ServiceModule const g_interpModule {
    .id = INTERP_SERVICE_ID,
    .majorVersion = INTERP_SERVICE_MAJOR,
    .minorVersion = INTERP_SERVICE_MINOR,
    .service = &s_interpService,
    .modDetached = mod_detached
};

}
