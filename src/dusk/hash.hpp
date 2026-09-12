#pragma once

#include <picosha2.h>

#include <cstdint>
#include <span>
#include <string>

namespace dusk::hash {

class Sha256 {
public:
    void update(std::span<const uint8_t> bytes) {
        mHasher.process(bytes.begin(), bytes.end());
    }

    std::string finish() {
        mHasher.finish();
        return picosha2::get_hash_hex_string(mHasher);
    }

private:
    picosha2::hash256_one_by_one mHasher;
};

}  // namespace dusk::hash
