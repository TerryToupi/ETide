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
#include <atomic>
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
#include <piece_tree/piece_tree.hpp>
#include <ui/ui.hpp>

// .cpp
#include <core/core.cpp>
#include <piece_tree/piece_tree.cpp>
#include <ui/ui.cpp>

using namespace ETide;

typedef struct TextBuffer TextBuffer;
struct TextBuffer {
    Arena::Arena* arena;
    char*         data;
    U64           capacity;
    U64           max_capacity;
    B32           allocation_failed;
};

struct ApplicationState {
    Arena::Arena* arena;
    bool          explorer_visible;
    bool          find_visible;
    bool          word_wrap;
    TextBuffer    document;
    char          search[128];
    char          replacement[128];
};

internal B32 text_buffer_init(TextBuffer* buffer, U64 capacity, U64 max_capacity) {
    Arena::Arena* arena = Arena::allocate({.flags        = Arena::ArenaFlags_NoChain,
                                           .reserve_size = max_capacity + Arena::arena_header_size,
                                           .commit_size  = KB(64)});
    if (arena == 0) { return 0; }

    buffer->arena = arena;
    buffer->data  = static_cast<char*>(Arena::push(arena, capacity, 1, 1));
    if (buffer->data == 0) {
        Arena::release(arena);
        *buffer = {};
        return 0;
    }

    buffer->capacity     = capacity;
    buffer->max_capacity = max_capacity;
    return 1;
}

internal void text_buffer_release(TextBuffer* buffer) {
    if (buffer->arena != 0) { Arena::release(buffer->arena); }
    *buffer = {};
}

internal B32 text_buffer_reserve(TextBuffer* buffer, U64 required_capacity) {
    if (required_capacity <= buffer->capacity) { return 1; }
    if (required_capacity > buffer->max_capacity) {
        SDL_Log("Document reached its %llu byte capacity",
                static_cast<unsigned long long>(buffer->max_capacity));
        buffer->allocation_failed = 1;
        return 0;
    }

    U64 capacity = buffer->capacity;
    while (capacity < required_capacity) { capacity += capacity / 2; }
    capacity = std::min(capacity, buffer->max_capacity);

    U64   additional_capacity = capacity - buffer->capacity;
    char* expected_extension  = buffer->data + buffer->capacity;
    char* extension = static_cast<char*>(Arena::push(buffer->arena, additional_capacity, 1, 0));
    if (extension == 0) {
        SDL_Log("Failed to grow the document buffer to %llu bytes",
                static_cast<unsigned long long>(capacity));
        buffer->allocation_failed = 1;
        return 0;
    }
    if (extension != expected_extension) {
        Arena::pop(buffer->arena, additional_capacity);
        SDL_Log("Document arena no longer has a contiguous tail");
        buffer->allocation_failed = 1;
        return 0;
    }

    buffer->capacity = capacity;
    return 1;
}

internal int text_buffer_resize(ImGuiInputTextCallbackData* callback_data) {
    if (callback_data->EventFlag != ImGuiInputTextFlags_CallbackResize) { return 0; }

    TextBuffer* buffer = static_cast<TextBuffer*>(callback_data->UserData);
    SDL_assert(callback_data->Buf == buffer->data);

    if (!text_buffer_reserve(buffer, callback_data->BufSize)) {
        callback_data->BufSize = static_cast<int>(buffer->capacity);
        return 1;
    }

    callback_data->Buf     = buffer->data;
    callback_data->BufSize = static_cast<int>(buffer->capacity);
    return 0;
}

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

            ImGuiInputTextFlags input_flags =
                ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize;
            if (state->word_wrap) { input_flags |= ImGuiInputTextFlags_WordWrap; }
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.055f, 0.063f, 0.075f, 1.0f));
            ImGui::InputTextMultiline("##Document",
                                      state->document.data,
                                      state->document.capacity,
                                      ImGui::GetContentRegionAvail(),
                                      input_flags,
                                      text_buffer_resize,
                                      &state->document);
            ImGui::PopStyleColor();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    size_t character_count = SDL_strlen(state->document.data);
    U32    line_count      = 1;
    for (size_t idx = 0; idx < character_count; ++idx) {
        if (state->document.data[idx] == '\n') { ++line_count; }
    }
    ImGui::Separator();
    ImGui::Text("Ln %u, Col 1", line_count);
    ImGui::SameLine();
    ImGui::TextDisabled(
        "  |  %zu characters  |  %llu byte capacity  |  UTF-8  |  C++  |  "
        "Spaces: 4",
        character_count,
        static_cast<unsigned long long>(state->document.capacity));
    if (state->document.allocation_failed) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "  |  Buffer growth failed");
    }

    ImGui::End();
}

internal void piece_tree_example() {
    Arena::Arena*          storage = Arena::allocate({});
    PieceTree::TreeBuilder builder = PieceTree::tree_builder_start(storage);

    char first[]  = "Hello\n";
    char second[] = "world";
    PieceTree::tree_builder_accept(0, &builder, str8_cstr(first));
    PieceTree::tree_builder_accept(0, &builder, str8_cstr(second));

    PieceTree::Tree* tree = PieceTree::tree_builder_finish(&builder);

    char inserted[] = "persistent ";
    tree->insert(PieceTree::CharOffset{6}, str8_cstr(inserted));

    Arena::Scratch scratch = Arena::ScratchBegin(0, 0);
    String8        line1   = tree->get_line_content(scratch.arena, PieceTree::Line{0});
    String8        line2   = tree->get_line_content(scratch.arena, PieceTree::Line{1});
    SDL_Log("%.*s", static_cast<int>(line1.size), line1.str);  // persistent world
    SDL_Log("%.*s", static_cast<int>(line2.size), line2.str);  // persistent world
    Arena::ScratchEnd(scratch);

    PieceTree::release_tree(tree);
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

    piece_tree_example();
    if (!UI::init()) { return SDL_APP_FAILURE; }

    state->explorer_visible = true;
    state->word_wrap        = false;
    char initial_document[] =
        "#include <SDL3/SDL.h>\n"
        "#include <imgui.h>\n"
        "\n"
        "int main(int argc, char** argv) {\n"
        "    // A lightweight place for ideas to become code.\n"
        "    SDL_Log(\"Welcome to TextPad\");\n"
        "    return 0;\n"
        "}\n";

    if (!text_buffer_init(&state->document, 128, MB(64)) ||
        !text_buffer_reserve(&state->document, sizeof(initial_document))) {
        SDL_Log("Failed to allocate the document buffer");
        return SDL_APP_FAILURE;
    }
    SDL_strlcpy(state->document.data, initial_document, state->document.capacity);

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
    if (state != 0) {
        text_buffer_release(&state->document);
        Arena::release(state->arena);
    }
}
