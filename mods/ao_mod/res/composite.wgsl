// Fullscreen composite: multiplies the denoised ambient-occlusion visibility over the scene.
//
// Debug views:
//   1 = raw AO visibility as grayscale
//   2 = view-space normals
//   3 = the preprocessed depth input
//   4 = depth staircase detector

struct Uniforms {
    projection: mat4x4f,
    inverse_projection: mat4x4f,
    size: vec2f,        // AO texture size in pixels (may be half the render size)
    inv_size: vec2f,
    depth_scale: vec2f,
    effect_radius: f32,
    intensity: f32,
    slice_count: f32,
    samples_per_slice_side: f32,
    debug_view: u32,
    _pad: f32,
}

@group(0) @binding(0) var ambient_occlusion: texture_2d<f32>;
@group(0) @binding(1) var preprocessed_depth: texture_2d<f32>;
@group(0) @binding(2) var scene_depth_raw: texture_2d<f32>;
@group(0) @binding(3) var<uniform> uniforms: Uniforms;
@group(0) @binding(4) var scene_normals: texture_2d<f32>;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> VertexOutput {
    // Fullscreen triangle
    var out: VertexOutput;
    let uv = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    out.position = vec4f(uv * vec2f(2.0, -2.0) + vec2f(-1.0, 1.0), 0.0, 1.0);
    out.uv = uv;
    return out;
}

// Manual bilinear sample (r32float is unfilterable without optional device features)
fn sample_visibility(uv: vec2f) -> f32 {
    let coordinates = uv * uniforms.size - 0.5;
    let base = floor(coordinates);
    let fraction = coordinates - base;
    let max_coordinates = vec2i(uniforms.size) - 1i;
    let p00 = clamp(vec2i(base), vec2i(0i), max_coordinates);
    let p11 = clamp(vec2i(base) + 1i, vec2i(0i), max_coordinates);
    let v00 = textureLoad(ambient_occlusion, vec2i(p00.x, p00.y), 0i).r;
    let v10 = textureLoad(ambient_occlusion, vec2i(p11.x, p00.y), 0i).r;
    let v01 = textureLoad(ambient_occlusion, vec2i(p00.x, p11.y), 0i).r;
    let v11 = textureLoad(ambient_occlusion, vec2i(p11.x, p11.y), 0i).r;
    let top = mix(v00, v10, fraction.x);
    let bottom = mix(v01, v11, fraction.x);
    return mix(top, bottom, fraction.y);
}

fn load_depth(pixel_coordinates: vec2<i32>) -> f32 {
    let coordinates = clamp(pixel_coordinates, vec2<i32>(0i), vec2<i32>(uniforms.size) - 1i);
    return textureLoad(preprocessed_depth, coordinates, 0i).r;
}

fn reconstruct_view_space_position(depth: f32, uv: vec2f) -> vec3f {
    let clip_xy = vec2f(uv.x * 2.0 - 1.0, 1.0 - 2.0 * uv.y);
    let t = uniforms.inverse_projection * vec4f(clip_xy, depth, 1.0);
    return t.xyz / t.w;
}

fn view_position_at(pixel_coordinates: vec2<i32>) -> vec3f {
    let depth = load_depth(pixel_coordinates);
    let uv = (vec2f(pixel_coordinates) + 0.5) * uniforms.inv_size;
    return reconstruct_view_space_position(depth, uv);
}

// Raw-snapshot variant of load_depth for the staircase view
fn load_raw_depth(pixel_coordinates: vec2<i32>) -> f32 {
    let size = vec2<i32>(textureDimensions(scene_depth_raw));
    let coordinates = clamp(pixel_coordinates, vec2<i32>(0i), size - 1i);
    return textureLoad(scene_depth_raw, coordinates, 0i).r;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let normal_size = vec2i(textureDimensions(scene_normals));
    let normal_pixel = clamp(vec2i(in.uv * vec2f(normal_size)), vec2i(0), normal_size - 1);
    let encoded_normal = textureLoad(scene_normals, normal_pixel, 0);
    if uniforms.debug_view == 2u {
        return vec4f(select(vec3f(0.0), encoded_normal.xyz, encoded_normal.a >= 0.5), 1.0);
    }
    if uniforms.debug_view == 3u {
        // Preprocessed depth as an exponential distance gradient (white = near, black = far)
        let pixel = vec2<i32>(in.uv * uniforms.size);
        let position = view_position_at(pixel);
        let value = exp(-max(-position.z, 0.0) * 0.0003);
        return vec4f(value, value, value, 1.0);
    }
    if uniforms.debug_view == 4u {
        // Staircase detector on the raw snapshot depth
        let size = vec2f(textureDimensions(scene_depth_raw));
        let pixel = vec2<i32>(in.uv * size);
        let d_center = load_raw_depth(pixel);
        let d_left = load_raw_depth(pixel + vec2<i32>(-1i, 0i));
        let d_right = load_raw_depth(pixel + vec2<i32>(1i, 0i));
        let d_top = load_raw_depth(pixel + vec2<i32>(0i, -1i));
        let d_bottom = load_raw_depth(pixel + vec2<i32>(0i, 1i));
        let gradient_x = abs(d_right - d_left) * 0.5;
        let curvature_x = abs(d_right - 2.0 * d_center + d_left);
        let gradient_y = abs(d_bottom - d_top) * 0.5;
        let curvature_y = abs(d_bottom - 2.0 * d_center + d_top);
        let ratio_x = curvature_x / max(gradient_x, 1e-12);
        let ratio_y = curvature_y / max(gradient_y, 1e-12);
        return vec4f(saturate(ratio_x), saturate(ratio_y), 0.0, 1.0);
    }

    if encoded_normal.a < 0.5 {
        return vec4f(1.0);
    }
    let visibility = sample_visibility(in.uv);
    if uniforms.debug_view == 1u {
        return vec4f(visibility, visibility, visibility, 1.0);
    }
    let value = mix(1.0, visibility, uniforms.intensity);
    return vec4f(value, value, value, 1.0);
}
