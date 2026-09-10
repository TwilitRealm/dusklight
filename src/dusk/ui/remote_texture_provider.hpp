#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace dusk::ui {

[[nodiscard]] std::string remote_image_source(
    std::string_view url, uint32_t width, uint32_t height);

void register_remote_texture_provider() noexcept;
void unregister_remote_texture_provider() noexcept;
void update_remote_texture_provider() noexcept;

}  // namespace dusk::ui
