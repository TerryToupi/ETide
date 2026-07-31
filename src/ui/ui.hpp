#ifndef UI_HPP_
#define UI_HPP_

namespace ETide::UI {

typedef struct Device Device;
struct Device {
    SDL_Window*           window;
    SDL_GPUDevice*        handle;
    SDL_GPUCommandBuffer* command_buffer;
    SDL_GPUTexture*       swapchain_texture;
    SDL_GPURenderPass*    render_pass;
    SDL_GPUTextureFormat  swapchain_format;
    SDL_FColor            clear_color;
    B32                   sdl_initialized;
    B32                   window_claimed;
    B32                   imgui_context_initialized;
    B32                   imgui_platform_initialized;
    B32                   imgui_renderer_initialized;
};

global Device gpu_device = {};

internal B32  init(void);
internal void process_event(SDL_Event* event);
internal void begin_frame(void);
internal B32  end_frame(void);
internal void shutdown(void);

}  // namespace ETide::UI

#endif
