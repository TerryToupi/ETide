# UI

`ETide::UI` owns ETide's SDL window, SDL_GPU device, ImGui context, SDL platform
backend, and SDL_GPU renderer backend.

The module exposes a small C-style frame lifecycle:

```cpp
UI::init();
UI::process_event(event);
UI::begin_frame();
UI::end_frame();
UI::shutdown();
```

## Complete application flow

```cpp
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!UI::init()) {
        UI::shutdown();
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    UI::process_event(event);

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    UI::begin_frame();

    ImGui::Begin("Example");
    ImGui::TextUnformatted("Hello ETide");
    ImGui::End();

    if (!UI::end_frame()) {
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    UI::shutdown();
}
```

Call functions in this order:

1. Call `init` once.
2. Forward every SDL event through `process_event`.
3. Call `begin_frame` once per frame.
4. Submit ImGui widgets.
5. Call `end_frame` once.
6. Call `shutdown` during application exit or after partial initialization failure.

## Initialization

```cpp
B32 UI::init(void);
```

`init` creates and configures:

- SDL video.
- A resizable 1280 x 720 high-pixel-density window titled `ETide`.
- An SDL_GPU device supporting SPIR-V, DXBC, MSL, and Metal library shaders.
- The window's GPU swapchain.
- An ImGui context.
- ImGui docking.
- ImGui's SDL3 platform backend.
- ImGui's SDL_GPU renderer backend.

It also selects the swapchain texture format and sets the default clear color:

```cpp
SDL_FColor{0.10f, 0.11f, 0.13f, 1.00f}
```

The function returns nonzero on success. Failures are logged with SDL and return zero.
Call `shutdown` after failure so any successfully initialized earlier stages are
released.

## Event processing

```cpp
void UI::process_event(SDL_Event* event);
```

Forwards an SDL event to ImGui's SDL3 backend after the platform backend has been
initialized.

The application still owns event-level behavior such as quitting, file drops, or
application shortcuts:

```cpp
UI::process_event(event);

if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
}
```

Forward events before application-specific handling so ImGui receives current input
state.

## Beginning a frame

```cpp
void UI::begin_frame(void);
```

Starts the SDL_GPU backend frame, the SDL3 platform frame, and the ImGui frame. It also
creates a dockspace over the main viewport.

All ImGui drawing calls for the frame belong after `begin_frame` and before
`end_frame`.

```cpp
UI::begin_frame();

ImGui::Begin("Inspector");
ImGui::Text("Frame time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
ImGui::End();

UI::end_frame();
```

## Ending and rendering a frame

```cpp
B32 UI::end_frame(void);
```

`end_frame`:

1. Finalizes ImGui draw data.
2. Acquires an SDL_GPU command buffer.
3. Waits for and acquires the swapchain texture.
4. Prepares ImGui draw data.
5. Begins a color render pass.
6. Clears the swapchain using `gpu_device.clear_color`.
7. Renders ImGui.
8. Ends the render pass.
9. Submits the command buffer.

It returns nonzero when the frame completed successfully.

When SDL returns no swapchain texture, such as while a window is minimized, the command
buffer is cancelled and the function returns success because there is no frame to
render.

Command acquisition, swapchain acquisition, render-pass, and submission failures are
logged and return zero.

The function resets transient command-buffer, swapchain-texture, and render-pass
pointers after submission or failure.

## Shutdown

```cpp
void UI::shutdown(void);
```

Shutdown waits for the GPU to become idle and releases initialized resources in reverse
order:

1. ImGui SDL_GPU renderer backend.
2. ImGui SDL3 platform backend.
3. ImGui context.
4. Window claim from the GPU device.
5. GPU device.
6. SDL window.
7. SDL.

Initialization flags allow shutdown to clean up a partially initialized module.

After shutdown, transient GPU pointers are zeroed and the swapchain format is reset to
`SDL_GPU_TEXTUREFORMAT_INVALID`.

## `UI::Device`

The global `UI::gpu_device` stores all UI and GPU state:

```cpp
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
```

### Persistent fields

- `window` is the SDL application window.
- `handle` is the SDL_GPU device.
- `swapchain_format` is the format required by the claimed window.
- `clear_color` is used at the start of every render pass.

### Frame-local fields

- `command_buffer` is valid only while recording the current frame.
- `swapchain_texture` is valid only for the acquired frame.
- `render_pass` is valid between `SDL_BeginGPURenderPass` and
  `SDL_EndGPURenderPass`.

Do not retain these transient pointers outside the frame.

### Initialization flags

Each flag records whether its corresponding subsystem needs shutdown:

- `sdl_initialized`
- `window_claimed`
- `imgui_context_initialized`
- `imgui_platform_initialized`
- `imgui_renderer_initialized`

## Customization

### Clear color

The clear color can be changed after initialization:

```cpp
UI::gpu_device.clear_color = {
    0.06f,
    0.08f,
    0.10f,
    1.0f,
};
```

### ImGui configuration

The ImGui context exists after `UI::init`, so application configuration can be adjusted
there:

```cpp
if (UI::init()) {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}
```

Docking is already enabled by the UI module.

## Ownership and integration rules

- `UI` owns the SDL window, GPU device, and ImGui context.
- Do not destroy those objects directly from application code.
- Call `process_event` for every SDL event.
- Perform ImGui calls only between `begin_frame` and `end_frame`.
- Check the result of `init` and `end_frame`.
- Call `shutdown` once when the UI is no longer needed.
- Wait for `end_frame` to submit before modifying frame-local GPU state.
- The module currently owns the frame's only swapchain render pass.

