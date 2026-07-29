namespace ETide::UI {

internal B32 init(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 0;
    }
    gpu_device.sdl_initialized = 1;

    gpu_device.window =
        SDL_CreateWindow("ETide", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (gpu_device.window == 0) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 0;
    }

    SDL_GPUShaderFormat shader_formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC |
                                         SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB;
    gpu_device.handle                  = SDL_CreateGPUDevice(shader_formats, 0, 0);
    if (gpu_device.handle == 0) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return 0;
    }

    if (!SDL_ClaimWindowForGPUDevice(gpu_device.handle, gpu_device.window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return 0;
    }
    gpu_device.window_claimed = 1;
    gpu_device.swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(gpu_device.handle, gpu_device.window);
    gpu_device.clear_color = {0.10f, 0.11f, 0.13f, 1.00f};

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    gpu_device.imgui_context_initialized = 1;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLGPU(gpu_device.window)) {
        SDL_Log("ImGui SDL platform initialization failed");
        return 0;
    }
    gpu_device.imgui_platform_initialized = 1;

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = gpu_device.handle;
    init_info.ColorTargetFormat          = gpu_device.swapchain_format;
    if (!ImGui_ImplSDLGPU3_Init(&init_info)) {
        SDL_Log("ImGui SDL_GPU renderer initialization failed");
        return 0;
    }
    gpu_device.imgui_renderer_initialized = 1;

    return 1;
}

internal void process_event(SDL_Event* event) {
    if (gpu_device.imgui_platform_initialized) { ImGui_ImplSDL3_ProcessEvent(event); }
}

internal void begin_frame(void) {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

internal B32 end_frame(void) {
    ImGui::Render();

    gpu_device.command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device.handle);
    if (gpu_device.command_buffer == 0) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return 0;
    }

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(gpu_device.command_buffer,
                                               gpu_device.window,
                                               &gpu_device.swapchain_texture,
                                               0,
                                               0)) {
        SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(gpu_device.command_buffer);
        gpu_device.command_buffer = 0;
        return 0;
    }

    if (gpu_device.swapchain_texture == 0) {
        SDL_CancelGPUCommandBuffer(gpu_device.command_buffer);
        gpu_device.command_buffer = 0;
        return 1;
    }

    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, gpu_device.command_buffer);

    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture                = gpu_device.swapchain_texture;
    target_info.clear_color            = gpu_device.clear_color;
    target_info.load_op                = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op               = SDL_GPU_STOREOP_STORE;

    gpu_device.render_pass = SDL_BeginGPURenderPass(gpu_device.command_buffer, &target_info, 1, 0);
    if (gpu_device.render_pass == 0) {
        SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(gpu_device.command_buffer);
        gpu_device.command_buffer    = 0;
        gpu_device.swapchain_texture = 0;
        return 0;
    }

    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, gpu_device.command_buffer, gpu_device.render_pass);
    SDL_EndGPURenderPass(gpu_device.render_pass);
    gpu_device.render_pass = 0;

    if (!SDL_SubmitGPUCommandBuffer(gpu_device.command_buffer)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        gpu_device.command_buffer    = 0;
        gpu_device.swapchain_texture = 0;
        return 0;
    }

    gpu_device.command_buffer    = 0;
    gpu_device.swapchain_texture = 0;
    return 1;
}

internal void shutdown(void) {
    if (gpu_device.handle != 0) { SDL_WaitForGPUIdle(gpu_device.handle); }
    if (gpu_device.imgui_renderer_initialized) {
        ImGui_ImplSDLGPU3_Shutdown();
        gpu_device.imgui_renderer_initialized = 0;
    }
    if (gpu_device.imgui_platform_initialized) {
        ImGui_ImplSDL3_Shutdown();
        gpu_device.imgui_platform_initialized = 0;
    }
    if (gpu_device.imgui_context_initialized) {
        ImGui::DestroyContext();
        gpu_device.imgui_context_initialized = 0;
    }
    if (gpu_device.window_claimed) {
        SDL_ReleaseWindowFromGPUDevice(gpu_device.handle, gpu_device.window);
        gpu_device.window_claimed = 0;
    }
    if (gpu_device.handle != 0) {
        SDL_DestroyGPUDevice(gpu_device.handle);
        gpu_device.handle = 0;
    }
    if (gpu_device.window != 0) {
        SDL_DestroyWindow(gpu_device.window);
        gpu_device.window = 0;
    }
    if (gpu_device.sdl_initialized) {
        SDL_Quit();
        gpu_device.sdl_initialized = 0;
    }

    gpu_device.command_buffer    = 0;
    gpu_device.swapchain_texture = 0;
    gpu_device.render_pass       = 0;
    gpu_device.swapchain_format  = SDL_GPU_TEXTUREFORMAT_INVALID;
}

}  // namespace ETide::UI
