#pragma once

#include <cstdint>
#include <string_view>

namespace dusk::save_manager {
struct Result;
struct Storage;
}  // namespace dusk::save_manager

namespace dusk::mods::svc {

void save_slot_new(uint32_t slot);
void save_slot_loaded(uint32_t slot);
void save_slot_written(uint32_t slot);
void save_slot_copied(uint32_t fromSlot, uint32_t toSlot);
void save_slot_erased(uint32_t slot);
void save_no_slot();
void invalidate_save(std::string_view saveName);

// Migrates the old mod_saves.json to the new per-file, per-mod JSON.
save_manager::Result migrate_legacy_sidecar(
    const save_manager::Storage& storage, std::string_view maker, std::string_view game);

}  // namespace dusk::mods::svc
