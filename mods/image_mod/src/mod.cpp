#include <webgpu/webgpu_cpp.h>

#include "mods/service.hpp"
#include "mods/svc/config.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/image.hpp"
#include "mods/svc/log.h"
#include "mods/svc/resource.hpp"
#include "mods/svc/ui.h"

DEFINE_MOD();
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(GfxService, svc_gfx);
IMPORT_SERVICE(ImageService, svc_image);
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(UiService, svc_ui);

using namespace std::string_view_literals;

namespace {

GfxDrawTypeHandle g_drawType = 0;
GfxStageHookHandle g_stageHook = 0;
wgpu::Device g_device;
wgpu::TextureView g_texture;
wgpu::Sampler g_sampler;
wgpu::RenderPipeline g_pipeline;

mods::config::Handle<bool> g_cvarEnabled;

void stage_hook(ModContext*, const GfxStageContext* stage_ctx, void*) {
    if (!g_cvarEnabled.get().value()) {
        return;
    }

    auto result = svc_gfx->push_draw(mod_ctx, g_drawType, nullptr, 0);
    if (result != MOD_OK) {
        mods::log::warn("push_draw failed");
    }
}

void draw_hook(ModContext*, const GfxDrawContext* draw_ctx, const void*, size_t, void*) {
    auto const device = wgpu::Device(draw_ctx->device);
    auto const bind_group_layout = g_pipeline.GetBindGroupLayout(0);

    wgpu::BindGroupEntry const entries[] {
        {
            .binding = 0,
            .textureView = g_texture,
        },
        {
            .binding = 1,
            .sampler = g_sampler,
        },
    };
    wgpu::BindGroupDescriptor const bg_desc {
        .layout = bind_group_layout,
        .entryCount = std::size(entries),
        .entries = entries,
    };
    auto const bind_group = device.CreateBindGroup(&bg_desc);

    auto const encoder = wgpu::RenderPassEncoder(draw_ctx->pass);
    encoder.SetPipeline(g_pipeline);
    encoder.SetBindGroup(0, bind_group);
    encoder.Draw(4);
}

ModResult register_hooks(ModError* error) {
    GfxStageHookDesc stage_desc GFX_STAGE_HOOK_DESC_INIT;
    stage_desc.callback = stage_hook;
    auto result =
        svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_FRAME_AFTER_HUD, &stage_desc, &g_stageHook);
    if (result != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "failed to register stage hook");
    }

    GfxDrawTypeDesc draw_desc GFX_DRAW_TYPE_DESC_INIT;
    draw_desc.draw = draw_hook;
    result = svc_gfx->register_draw_type(mod_ctx, &draw_desc, &g_drawType);
    if (result != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "failed to register draw type");
    }

    return MOD_OK;
}

ModResult create_pipeline(ModError* error) {
    mods::resource::LoadedResource shaderSource;
    try {
        shaderSource = mods::resource::load("image.wgsl").value();
    } catch (std::bad_expected_access<ModResult> const&) {
        return mods::set_error(error, MOD_ERROR, "failed to load shader");
    }

    GfxDeviceInfo device_info GFX_DEVICE_INFO_INIT;
    if (svc_gfx->get_device_info(mod_ctx, &device_info) != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "failed to load device info");
    }

    g_device = wgpu::Device(device_info.device);

    wgpu::ShaderSourceWGSL shaderSourceWgsl;
    shaderSourceWgsl.code = wgpu::StringView(
        static_cast<const char*>(shaderSource.buffer.data), shaderSource.buffer.size);
    wgpu::ShaderModuleDescriptor const shaderDesc{
        .nextInChain = &shaderSourceWgsl,
        .label = "image.wgsl"sv,
    };

    auto shader = g_device.CreateShaderModule(&shaderDesc);

    wgpu::ColorTargetState const targets[]{
        {
            .format = static_cast<wgpu::TextureFormat>(device_info.color_format),
        },
    };
    wgpu::DepthStencilState const depth_stencil{
        .format = static_cast<wgpu::TextureFormat>(device_info.depth_format),
        .depthWriteEnabled = false,
        .depthCompare = wgpu::CompareFunction::Always,
    };
    wgpu::FragmentState const fragment{
        .module = shader, .targetCount = std::size(targets), .targets = targets};
    wgpu::RenderPipelineDescriptor const desc{
        .label = "image_mod pipeline"sv,
        .vertex =
            {
                .module = shader,
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleStrip,
            },
        .depthStencil = &depth_stencil,
        .fragment = &fragment,
    };

    g_pipeline = g_device.CreateRenderPipeline(&desc);

    return MOD_OK;
}

ModResult load_texture(ModError* error) {
    try {
        // Load PNG bytes to memory.
        auto const image_data = mods::resource::load("image.png").value();

        // Open the image, parsing the metadata.
        auto image = mods::image::open(image_data.span()).value();
        auto const info = image.get_info().value();

        // We can validate the image data before decoding it.
        if (info.width > 1024 || info.height > 1024) {
            return mods::set_error(error, MOD_ERROR, "image too large!");
        }

        // Actually decode image data into buffer.
        std::vector<unsigned char> buf(info.width * info.height * IMAGE_PIXEL_RGBA8_BYTES);
        image.decode_rgba8(buf).value();

        // Allocate WebGPU texture and upload.
        wgpu::TextureDescriptor const texture_desc{
            .label = "Silly"sv,
            .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding,
            .dimension = wgpu::TextureDimension::e2D,
            .size =
                {
                    .width = info.width,
                    .height = info.height,
                },
            .format = wgpu::TextureFormat::RGBA8UnormSrgb,
        };
        auto const texture = g_device.CreateTexture(&texture_desc);
        g_texture = texture.CreateView();

        wgpu::TexelCopyTextureInfo const dest_info{
            .texture = texture,
        };

        wgpu::TexelCopyBufferLayout const src_layout{
            .offset = 0,
            .bytesPerRow = info.width * IMAGE_PIXEL_RGBA8_BYTES,
            .rowsPerImage = info.height,
        };

        g_device.GetQueue().WriteTexture(
            &dest_info, buf.data(), buf.size(), &src_layout, &texture_desc.size);

        return MOD_OK;

    } catch (std::bad_expected_access<ModResult> const&) {
        return mods::set_error(error, MOD_ERROR, "failed to load image");
    }
}

void create_sampler() {
    wgpu::SamplerDescriptor constexpr sampler_desc {
        .addressModeU = wgpu::AddressMode::Repeat,
        .addressModeV = wgpu::AddressMode::Repeat,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
    };

    g_sampler = g_device.CreateSampler(&sampler_desc);
}

ModResult build_mod_panel(ModContext*, UiElementHandle pane, void*, ModError* out_error) {
    UiControlDesc desc UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_TOGGLE;
    desc.label = "Enabled";
    desc.binding = UI_BINDING_CONFIG_VAR;
    desc.config_var = g_cvarEnabled.handle;

    auto const result = svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr);
    if (result != MOD_OK) {
        mods::set_error(out_error, MOD_ERROR, "Failed to add control");
        return result;
    }

    return MOD_OK;
}

ModResult register_mod_panel(ModError* error) {
    UiModsPanelDesc panel_desc UI_MODS_PANEL_DESC_INIT;
    panel_desc.build = build_mod_panel;
    auto const result = svc_ui->register_mods_panel(mod_ctx, &panel_desc);
    if (result != MOD_OK) {
        mods::set_error(error, MOD_ERROR, "Failed to add control");
        return result;
    }

    return MOD_OK;
}

}  // namespace

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    try {
        g_cvarEnabled = mods::config::register_var("overlayEnabled", false).value();
    } catch (std::bad_expected_access<ModResult> const&) {
        return mods::set_error(error, MOD_ERROR, "failed to register option");
    }

    if (create_pipeline(error) != MOD_OK) {
        return error->code;
    }

    create_sampler();

    if (load_texture(error) != MOD_OK) {
        return error->code;
    }

    if (register_hooks(error) != MOD_OK) {
        return error->code;
    }

    if (register_mod_panel(error) != MOD_OK) {
        return error->code;
    }

    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    g_cvarEnabled = {};
    g_device = {};
    g_texture = {};
    g_sampler = {};
    g_pipeline = {};

    return MOD_OK;
}
}
