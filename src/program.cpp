// Importing SDL
#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_main.h>
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
#include <ui/ui.hpp>

// .cpp
#include <core/core.cpp>
#include <ui/ui.cpp>

using namespace ETide;

struct ApplicationState {
    Arena::Arena* arena;
    bool          explorer_visible;
    bool          find_visible;
    bool          word_wrap;
    char          document[32 * 1024];
    char          search[128];
    char          replacement[128];
};

internal void draw_text_pad(ApplicationState* state) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration |
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
            if (ImGui::MenuItem("Find and Replace", "Ctrl+F")) { state->find_visible = true; }
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
        ImGui::BeginChild("Explorer", ImVec2(220.0f, content_height), ImGuiChildFlags_Borders);
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
                ImGui::InputTextWithHint("##Search", "Find", state->search, sizeof(state->search));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputTextWithHint("##Replace",
                                         "Replace",
                                         state->replacement,
                                         sizeof(state->replacement));
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
            ImGui::InputTextMultiline("##Document",
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
    Arena::Arena* arena = Arena::allocate({});
    if (arena == 0) {
        SDL_Log("Failed to allocate the application arena");
        return SDL_APP_FAILURE;
    }

    ApplicationState* state = Arena::push_array<ApplicationState>(arena, 1);
    if (state == 0) {
        SDL_Log("Failed to allocate the application state");
        Arena::release(arena);
        return SDL_APP_FAILURE;
    }
    state->arena = arena;
    *appstate    = state;

    if (!UI::init()) { return SDL_APP_FAILURE; }

    state->explorer_visible = true;
    state->word_wrap        = false;
    SDL_strlcpy(state->document,
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
    UI::process_event(event);
    if (event->type == SDL_EVENT_QUIT) { return SDL_APP_SUCCESS; }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate) {
    ApplicationState* state = static_cast<ApplicationState*>(appstate);

    UI::begin_frame();
    draw_text_pad(state);
    if (!UI::end_frame()) { return SDL_APP_FAILURE; }

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    ApplicationState* state = static_cast<ApplicationState*>(appstate);
    UI::shutdown();
    if (state != 0) { Arena::release(state->arena); }
}
