@group(0) @binding(0) var tex0_t : texture_2d<f32>;
@group(0) @binding(1) var tex0_s : sampler;

struct VtxOutput {
    @builtin(position) pos: vec4f,
    @location(0) texCoord: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> VtxOutput {
    var output: VtxOutput;

    switch (idx) {
    case 0: {
        // Bottom left
        output.pos = vec4f(-1, -1, 0, 1);
        output.texCoord = vec2f(0, 1);
    }
    case 1: {
        // Bottom right
        output.pos = vec4f(1, -1, 0, 1);
        output.texCoord = vec2f(1, 1);
    }
    case 2: {
        // Top left
        output.pos = vec4f(-1, 1, 0, 1);
        output.texCoord = vec2f(0, 0);
    }
    default: {
        // Top right
        output.pos = vec4f(1, 1, 0, 1);
        output.texCoord = vec2f(1, 0);
    }
    }

    return output;
}

@fragment
fn fs_main(input: VtxOutput) -> @location(0) vec4f {
    return textureSample(tex0_t, tex0_s, input.texCoord);
}
