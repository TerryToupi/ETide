// Importing SDL
#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_mutex.h>

// Importing ImGui
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

// STL
#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

// .hpp
#include <core/core.hpp>

// .cpp
#include <core/core.cpp>

using namespace ETide;

struct ApplicationState {
    SDL_Window*    window;
    SDL_GPUDevice* gpu_device;
    bool           imgui_context_initialized;
    bool           imgui_platform_initialized;
    bool           imgui_renderer_initialized;
    bool           explorer_visible;
    bool           find_visible;
    bool           word_wrap;
    char           document[32 * 1024];
    char           search[128];
    char           replacement[128];
};

internal void draw_text_pad(ApplicationState* state) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("TextPad", NULL, window_flags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            ImGui::MenuItem("New", "Ctrl+N");
            ImGui::MenuItem("Open...", "Ctrl+O");
            ImGui::Separator();
            ImGui::MenuItem("Save", "Ctrl+S");
            ImGui::MenuItem("Save As...", "Ctrl+Shift+S");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::Separator();
            if (ImGui::MenuItem("Find and Replace", "Ctrl+F")) {
                state->find_visible = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Explorer", NULL, &state->explorer_visible);
            ImGui::MenuItem("Word Wrap", NULL, &state->word_wrap);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::SetCursorPos(ImVec2(10.0f, ImGui::GetCursorPosY() + 7.0f));
    ImGui::Button("New");
    ImGui::SameLine();
    ImGui::Button("Open");
    ImGui::SameLine();
    ImGui::Button("Save");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::Button("Find")) { state->find_visible = !state->find_visible; }
    ImGui::SameLine();
    ImGui::TextDisabled("workspace/textpad.cpp");
    ImGui::Separator();

    float status_bar_height = ImGui::GetFrameHeightWithSpacing();
    float content_height    = ImGui::GetContentRegionAvail().y - status_bar_height;

    if (state->explorer_visible) {
        ImGui::BeginChild(
            "Explorer",
            ImVec2(220.0f, content_height),
            ImGuiChildFlags_Borders);
        ImGui::TextDisabled("EXPLORER");
        ImGui::Separator();
        if (ImGui::TreeNodeEx("ETide", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("CMakeLists.txt");
            if (ImGui::TreeNodeEx("src", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Selectable("program.cpp", true);
                ImGui::Selectable("core/core.hpp");
                ImGui::Selectable("core/core.cpp");
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("libs")) {
                ImGui::BulletText("imgui");
                ImGui::BulletText("SDL");
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImGui::BeginChild("Editor", ImVec2(0.0f, content_height));
    if (ImGui::BeginTabBar("Documents")) {
        if (ImGui::BeginTabItem("program.cpp  *")) {
            if (state->find_visible) {
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputTextWithHint(
                    "##Search", "Find", state->search, sizeof(state->search));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputTextWithHint(
                    "##Replace", "Replace", state->replacement, sizeof(state->replacement));
                ImGui::SameLine();
                ImGui::Button("Previous");
                ImGui::SameLine();
                ImGui::Button("Next");
                ImGui::SameLine();
                ImGui::Button("Replace");
                ImGui::SameLine();
                if (ImGui::Button("Close")) { state->find_visible = false; }
                ImGui::Separator();
            }

            ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_AllowTabInput;
            if (state->word_wrap) { input_flags |= ImGuiInputTextFlags_WordWrap; }
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.055f, 0.063f, 0.075f, 1.0f));
            ImGui::InputTextMultiline(
                "##Document",
                state->document,
                sizeof(state->document),
                ImGui::GetContentRegionAvail(),
                input_flags);
            ImGui::PopStyleColor();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    size_t character_count = SDL_strlen(state->document);
    U32    line_count      = 1;
    for (size_t idx = 0; idx < character_count; ++idx) {
        if (state->document[idx] == '\n') { ++line_count; }
    }
    ImGui::Separator();
    ImGui::Text("Ln %u, Col 1", line_count);
    ImGui::SameLine();
    ImGui::TextDisabled("  |  %zu characters  |  UTF-8  |  C++  |  Spaces: 4", character_count);

    ImGui::End();
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    ApplicationState* state = static_cast<ApplicationState*>(SDL_calloc(1, sizeof(ApplicationState)));
    if (state == NULL) {
        SDL_Log("Failed to allocate application state: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->window = SDL_CreateWindow(
        "ETide",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (state->window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUShaderFormat shader_formats =
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC |
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB;
    state->gpu_device = SDL_CreateGPUDevice(shader_formats, false, NULL);
    if (state->gpu_device == NULL) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(state->gpu_device, state->window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    state->imgui_context_initialized = true;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLGPU(state->window)) {
        SDL_Log("ImGui SDL platform initialization failed");
        return SDL_APP_FAILURE;
    }
    state->imgui_platform_initialized = true;

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = state->gpu_device;
    init_info.ColorTargetFormat =
        SDL_GetGPUSwapchainTextureFormat(state->gpu_device, state->window);
    if (!ImGui_ImplSDLGPU3_Init(&init_info)) {
        SDL_Log("ImGui SDL_GPU renderer initialization failed");
        return SDL_APP_FAILURE;
    }
    state->imgui_renderer_initialized = true;
    state->explorer_visible           = true;
    state->word_wrap                  = false;
    SDL_strlcpy(
        state->document,
        "#include <SDL3/SDL.h>\n"
        "#include <imgui.h>\n"
        "\n"
        "int main(int argc, char** argv) {\n"
        "    // A lightweight place for ideas to become code.\n"
        "    SDL_Log(\"Welcome to TextPad\");\n"
        "    return 0;\n"
        "}\n",
        sizeof(state->document));

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
    if (event->type == SDL_EVENT_QUIT) { return SDL_APP_SUCCESS; }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate) {
    ApplicationState* state = static_cast<ApplicationState*>(appstate);

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    draw_text_pad(state);
    ImGui::Render();

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);
    if (command_buffer == NULL) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture* swapchain_texture = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            command_buffer, state->window, &swapchain_texture, NULL, NULL)) {
        SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return SDL_APP_FAILURE;
    }

    if (swapchain_texture == NULL) {
        SDL_CancelGPUCommandBuffer(command_buffer);
        return SDL_APP_CONTINUE;
    }

    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture                = swapchain_texture;
    target_info.clear_color            = {0.10f, 0.11f, 0.13f, 1.00f};
    target_info.load_op                = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op               = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* render_pass =
        SDL_BeginGPURenderPass(command_buffer, &target_info, 1, NULL);
    if (render_pass == NULL) {
        SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(command_buffer);
        return SDL_APP_FAILURE;
    }

    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
    SDL_EndGPURenderPass(render_pass);

    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    ApplicationState* state = static_cast<ApplicationState*>(appstate);
    if (state == NULL) {
        SDL_Quit();
        return;
    }

    if (state->gpu_device != NULL) { SDL_WaitForGPUIdle(state->gpu_device); }
    if (state->imgui_renderer_initialized) { ImGui_ImplSDLGPU3_Shutdown(); }
    if (state->imgui_platform_initialized) { ImGui_ImplSDL3_Shutdown(); }
    if (state->imgui_context_initialized) { ImGui::DestroyContext(); }
    if (state->gpu_device != NULL && state->window != NULL) {
        SDL_ReleaseWindowFromGPUDevice(state->gpu_device, state->window);
    }
    if (state->gpu_device != NULL) { SDL_DestroyGPUDevice(state->gpu_device); }
    if (state->window != NULL) { SDL_DestroyWindow(state->window); }

    SDL_free(state);
    SDL_Quit();
}
