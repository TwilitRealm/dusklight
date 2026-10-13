#pragma once

namespace dusk::mouse {
void read();
void get_aim_deltas(float& out_yaw, float& out_pitch);
void get_camera_deltas(float& out_yaw, float& out_pitch);
}  // namespace dusk::mouse
