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

// Importing tree-sitter
#include <tree_sitter/api.h>

// STL
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

// The module headers are not self-contained: each one depends on the ones
// above it, so this order is load-bearing and must not be sorted.
// clang-format off

// .hpp
#include <core/core.hpp>
#include <thread/thread.hpp>
#include <arena/arena.hpp>
#include <string/string.hpp>
#include <containers/containers.hpp>
#include <piece_tree/piece_tree.hpp>
#include <ui/ui.hpp>

// .cpp
#include <core/core.cpp>
#include <thread/thread.cpp>
#include <arena/arena.cpp>
#include <string/string.cpp>
#include <piece_tree/piece_tree.cpp>
#include <ui/ui.cpp>

// clang-format on

// Each vendored grammar in languages/ is generated code exposing one C entry
// point. There is no header to include, so they are declared by hand.
extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);
extern "C" const TSLanguage* tree_sitter_json(void);
extern "C" const TSLanguage* tree_sitter_odin(void);
extern "C" const TSLanguage* tree_sitter_rust(void);

using namespace ETide;

namespace fs = std::filesystem;

/* ===========================================================================
 * Reference example: one code buffer.
 *
 *   std::filesystem  finds the file on disk
 *   PieceTree        stores it and answers every edit
 *   tree-sitter      classifies it into colored spans
 *   RenderBuffer     a dummy flat buffer the renderer draws from
 *
 * The piece tree is the single source of truth. Nothing below ever mutates
 * text in place; edits go to the tree and the render buffer is rebuilt from
 * it.
 * ======================================================================== */

/* ===========================================================================
 * Languages
 * ======================================================================== */

typedef struct Language Language;
struct Language {
    const char* name;
    const char* extensions;  // Space separated, leading dots, lowercase.
    const TSLanguage* (*grammar)(void);
};

global const Language languages[] = {
    {"C", ".c .h", tree_sitter_c},
    {"C++", ".cpp .cc .cxx .c++ .hpp .hh .hxx .h++ .ipp .inl", tree_sitter_cpp},
    {"JSON", ".json", tree_sitter_json},
    {"Odin", ".odin", tree_sitter_odin},
    {"Rust", ".rs", tree_sitter_rust},
};

// Matches one space-separated entry of `extensions` in full, so ".h" never
// matches ".hpp".
internal B32 extension_list_contains(const char* extensions, const char* extension) {
    U64 length = SDL_strlen(extension);
    for (const char* scan = extensions; *scan != 0;) {
        const char* end = SDL_strchr(scan, ' ');
        U64         run = end != 0 ? static_cast<U64>(end - scan) : SDL_strlen(scan);
        if (run == length && SDL_strncasecmp(scan, extension, run) == 0) { return 1; }
        if (end == 0) { break; }
        scan = end + 1;
    }
    return 0;
}

internal const Language* language_from_path(const fs::path& path) {
    std::string extension = path.extension().string();
    if (extension.empty()) { return 0; }

    for (U64 idx = 0; idx < SDL_arraysize(languages); ++idx) {
        if (extension_list_contains(languages[idx].extensions, extension.c_str())) {
            return &languages[idx];
        }
    }
    return 0;
}

/* ===========================================================================
 * Syntax classification
 * ======================================================================== */

typedef enum TokenKind : U32 {
    TokenKind_Text,
    TokenKind_Keyword,
    TokenKind_Type,
    TokenKind_String,
    TokenKind_Number,
    TokenKind_Comment,
    TokenKind_Preproc,
    TokenKind_Punctuation,
    TokenKind_COUNT,
} TokenKind;

global const ImU32 token_palette[TokenKind_COUNT] = {
    IM_COL32(212, 212, 212, 255),  // TokenKind_Text
    IM_COL32(197, 134, 192, 255),  // TokenKind_Keyword
    IM_COL32(78, 201, 176, 255),   // TokenKind_Type
    IM_COL32(206, 145, 120, 255),  // TokenKind_String
    IM_COL32(181, 206, 168, 255),  // TokenKind_Number
    IM_COL32(106, 153, 85, 255),   // TokenKind_Comment
    IM_COL32(155, 155, 255, 255),  // TokenKind_Preproc
    IM_COL32(133, 133, 133, 255),  // TokenKind_Punctuation
};

// One classifier serves every grammar. Node type names are per-grammar strings
// with no cross-language standard, so the tables below list the names each
// vendored grammar actually uses; an unrecognised name simply falls back to
// TokenKind_Text rather than failing.
typedef struct NodeRule NodeRule;
struct NodeRule {
    const char* type;
    TokenKind   kind;
};

// Nodes colored as a single token: the walk stops here instead of descending,
// so the quotes around a string stay string-colored rather than punctuation.
global const NodeRule atomic_rules[] = {
    {"comment", TokenKind_Comment},            // C, C++, Odin, JSON
    {"line_comment", TokenKind_Comment},       // Rust
    {"block_comment", TokenKind_Comment},      // Rust
    {"string_literal", TokenKind_String},      // C, C++, Rust, Odin
    {"char_literal", TokenKind_String},        // C, C++, Rust
    {"raw_string_literal", TokenKind_String},  // C++, Rust
    {"interpreted_string_literal", TokenKind_String},
    {"system_lib_string", TokenKind_String},  // C, C++ (<stdio.h>)
    {"string", TokenKind_String},             // JSON
    {"preproc_arg", TokenKind_Preproc},       // C, C++ macro bodies
};

global const NodeRule leaf_rules[] = {
    {"primitive_type", TokenKind_Type},        // C, C++, Rust
    {"type_identifier", TokenKind_Type},       // C, C++, Rust
    {"sized_type_specifier", TokenKind_Type},  // C, C++
    {"number_literal", TokenKind_Number},      // C, C++
    {"integer_literal", TokenKind_Number},     // Rust
    {"float_literal", TokenKind_Number},       // Rust
    {"number", TokenKind_Number},              // JSON, Odin
    {"character", TokenKind_String},           // C, C++
    {"string_content", TokenKind_String},
    {"escape_sequence", TokenKind_String},
    {"preproc_directive", TokenKind_Preproc},  // C, C++
    {"true", TokenKind_Keyword},               // JSON names these, so they are
    {"false", TokenKind_Keyword},              // not caught by the anonymous
    {"null", TokenKind_Keyword},               // leaf rule above.
};

internal B32 node_rule_lookup(const NodeRule* rules, U64 count, const char* type, TokenKind* kind) {
    for (U64 idx = 0; idx < count; ++idx) {
        if (SDL_strcmp(rules[idx].type, type) == 0) {
            *kind = rules[idx].kind;
            return 1;
        }
    }
    return 0;
}

internal B32 node_is_atomic(const char* type, TokenKind* kind) {
    return node_rule_lookup(atomic_rules, SDL_arraysize(atomic_rules), type, kind);
}

internal TokenKind token_kind_from_leaf(TSNode node) {
    const char* type = ts_node_type(node);

    // Anonymous leaves are the grammar's literal tokens: keywords such as
    // `return`, directives such as `#include`, and punctuation. Their type
    // string is the literal text, so its first byte is enough to tell them
    // apart.
    if (!ts_node_is_named(node)) {
        char first = type[0];
        if (first == '#') { return TokenKind_Preproc; }
        if (first == '_' || SDL_isalpha(static_cast<unsigned char>(first))) {
            return TokenKind_Keyword;
        }
        return TokenKind_Punctuation;
    }

    TokenKind kind = TokenKind_Text;
    node_rule_lookup(leaf_rules, SDL_arraysize(leaf_rules), type, &kind);
    return kind;
}

/* ===========================================================================
 * Render buffer (the dummy buffer)
 * ======================================================================== */

typedef struct RenderSpan RenderSpan;
struct RenderSpan {
    U64       offset;  // Document byte offset.
    U64       size;
    TokenKind kind;
};

typedef struct RenderLine RenderLine;
struct RenderLine {
    U64 offset;      // Document offset of the first character on the line.
    U64 size;        // Bytes on the line, line ending excluded.
    U64 first_span;  // Index of the first span touching the line.
    U64 span_count;  // A span crossing a line break appears on both lines.
};

// A flat snapshot of the document plus the derived data the renderer needs.
// Rebuilt from the piece tree on every edit; the renderer reads nothing else.
typedef struct RenderBuffer RenderBuffer;
struct RenderBuffer {
    Arena::Arena*   arena;
    String::String8 text;
    RenderSpan*     spans;
    U64             span_count;
    RenderLine*     lines;
    U64             line_count;
};

// Emits one span per colored token, in document order. Called twice: with
// `spans == 0` to count, then with an exactly sized array to fill.
internal void collect_spans(TSNode node, RenderSpan* spans, U64 capacity, U64* count) {
    TokenKind kind        = TokenKind_Text;
    B32       atomic      = node_is_atomic(ts_node_type(node), &kind);
    U32       child_count = ts_node_child_count(node);

    if (!atomic && child_count != 0) {
        for (U32 idx = 0; idx < child_count; ++idx) {
            collect_spans(ts_node_child(node, idx), spans, capacity, count);
        }
        return;
    }
    if (!atomic) { kind = token_kind_from_leaf(node); }

    U64 start = ts_node_start_byte(node);
    U64 end   = ts_node_end_byte(node);
    if (end <= start) { return; }

    if (spans != 0 && *count < capacity) {
        spans[*count] = {.offset = start, .size = end - start, .kind = kind};
    }
    *count += 1;
}

internal void render_buffer_flatten(RenderBuffer* render, PieceTree::Tree* tree) {
    U64 size = PieceTree::rep(tree->length());

    // str8_cstr_alloc reserves the extra byte and terminates it, which is what
    // tree-sitter wants.
    render->text = String::str8_cstr_alloc(render->arena, size);
    if (render->text.str == 0) { return; }

    // Walk characters straight out of the pieces; no intermediate copy.
    Arena::Arena*  conflicts[] = {render->arena};
    Arena::Scratch scratch     = Arena::ScratchBegin(conflicts, 1);

    PieceTree::TreeWalker walker{scratch.arena, tree, PieceTree::CharOffset{0}};
    for (U64 idx = 0; idx < size && !walker.exhausted(); ++idx) {
        render->text.str[idx] = walker.next();
    }

    Arena::ScratchEnd(scratch);
}

internal void render_buffer_collect_lines(RenderBuffer* render) {
    String::String8 text = render->text;

    U64 line_count = 1;
    for (U64 idx = 0; idx < text.size; ++idx) {
        if (text.str[idx] == '\n') { ++line_count; }
    }

    render->lines = Arena::push_array<RenderLine>(render->arena, line_count);
    if (render->lines == 0) { return; }
    render->line_count = line_count;

    U64 line_index  = 0;
    U64 line_start  = 0;
    U64 span_cursor = 0;

    for (U64 idx = 0; idx <= text.size; ++idx) {
        if (idx != text.size && text.str[idx] != '\n') { continue; }

        U64 line_end = idx;
        if (line_end > line_start && text.str[line_end - 1] == '\r') { --line_end; }

        // Spans are sorted and non-overlapping, so the spans touching a line
        // form one contiguous range and both cursors only move forward.
        while (span_cursor < render->span_count &&
               render->spans[span_cursor].offset + render->spans[span_cursor].size <= line_start) {
            ++span_cursor;
        }
        U64 span_last = span_cursor;
        while (span_last < render->span_count && render->spans[span_last].offset < idx) {
            ++span_last;
        }

        render->lines[line_index] = {.offset     = line_start,
                                     .size       = line_end - line_start,
                                     .first_span = span_cursor,
                                     .span_count = span_last - span_cursor};

        ++line_index;
        line_start = idx + 1;
        if (line_index == line_count) { break; }
    }
}

internal void render_buffer_rebuild(RenderBuffer*    render,
                                    PieceTree::Tree* tree,
                                    TSParser*        parser,
                                    TSTree**         syntax_tree) {
    Arena::Arena* arena = render->arena;
    Arena::clear(arena);
    *render       = {};
    render->arena = arena;

    render_buffer_flatten(render, tree);
    if (render->text.str == 0) {
        SDL_Log("Failed to flatten the document into the render buffer");
        return;
    }

    // A full reparse per edit. Correct and simple; the incremental path would
    // keep the old tree, call ts_tree_edit for each change, and pass it back
    // in as the second argument.
    if (*syntax_tree != 0) {
        ts_tree_delete(*syntax_tree);
        *syntax_tree = 0;
    }
    // No parser means no vendored grammar for this extension; the document
    // still renders, just without spans.
    if (parser != 0) {
        *syntax_tree = ts_parser_parse_string(parser,
                                              0,
                                              render->text.str,
                                              static_cast<U32>(render->text.size));
    }

    if (*syntax_tree != 0) {
        TSNode root   = ts_tree_root_node(*syntax_tree);
        U64    needed = 0;
        collect_spans(root, 0, 0, &needed);

        if (needed != 0) {
            render->spans = Arena::push_array<RenderSpan>(arena, needed);
            if (render->spans != 0) {
                U64 written = 0;
                collect_spans(root, render->spans, needed, &written);
                render->span_count = std::min(written, needed);
            }
        }
    }

    render_buffer_collect_lines(render);
}

/* ===========================================================================
 * Editor state
 * ======================================================================== */

typedef struct EditorState EditorState;
struct EditorState {
    Arena::Arena*    arena;
    PieceTree::Tree* tree;
    TSParser*        parser;
    TSTree*          syntax_tree;
    const Language*  language;  // 0 when the extension has no vendored grammar.
    RenderBuffer     render;
    U64              cursor;  // Document byte offset of the caret.
    U64              goal_column;
    B32              dirty;
    B32              scroll_to_caret;
    char             path[512];
};

internal U64 editor_length(EditorState* editor) {
    return PieceTree::rep(editor->tree->length());
}

internal U64 editor_caret_line(EditorState* editor) {
    return PieceTree::rep(editor->tree->line_at(PieceTree::CharOffset{editor->cursor}));
}

internal void editor_insert(EditorState* editor, char* text, U64 size) {
    if (size == 0) { return; }

    // The piece tree copies the bytes into its modification buffer, so a
    // caller-owned buffer is fine here.
    editor->tree->insert(PieceTree::CharOffset{editor->cursor}, String::str8(text, size));
    editor->cursor += size;
    editor->dirty           = 1;
    editor->scroll_to_caret = 1;
}

internal void editor_remove(EditorState* editor, U64 offset, U64 count) {
    U64 length = editor_length(editor);
    if (count == 0 || offset >= length) { return; }
    count = std::min(count, length - offset);

    editor->tree->remove(PieceTree::CharOffset{offset}, PieceTree::Length{count});
    editor->cursor          = offset;
    editor->dirty           = 1;
    editor->scroll_to_caret = 1;
}

internal void editor_move_to(EditorState* editor, U64 offset, B32 keep_goal_column) {
    editor->cursor          = std::min(offset, editor_length(editor));
    editor->scroll_to_caret = 1;
    if (!keep_goal_column) {
        U64 line = editor_caret_line(editor);
        if (line < editor->render.line_count) {
            editor->goal_column = editor->cursor - editor->render.lines[line].offset;
        }
    }
}

internal void editor_move_vertical(EditorState* editor, I64 delta) {
    RenderBuffer* render = &editor->render;
    if (render->line_count == 0) { return; }

    I64 line = static_cast<I64>(editor_caret_line(editor)) + delta;
    line     = std::clamp(line, static_cast<I64>(0), static_cast<I64>(render->line_count) - 1);

    RenderLine* target = &render->lines[line];
    editor_move_to(editor, target->offset + std::min(editor->goal_column, target->size), 1);
}

internal void editor_handle_input(EditorState* editor) {
    ImGuiIO& io = ImGui::GetIO();

    // Text. The example stays byte-oriented, so only printable ASCII is
    // accepted; UTF-8 input would need multi-byte offsets throughout.
    for (int idx = 0; idx < io.InputQueueCharacters.Size; ++idx) {
        ImWchar character = io.InputQueueCharacters[idx];
        if (character < 32 || character > 126) { continue; }
        char text = static_cast<char>(character);
        editor_insert(editor, &text, 1);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        char newline = '\n';
        editor_insert(editor, &newline, 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        char spaces[] = {' ', ' ', ' ', ' '};
        editor_insert(editor, spaces, sizeof(spaces));
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && editor->cursor > 0) {
        editor_remove(editor, editor->cursor - 1, 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) { editor_remove(editor, editor->cursor, 1); }

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && editor->cursor > 0) {
        editor_move_to(editor, editor->cursor - 1, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) { editor_move_to(editor, editor->cursor + 1, 0); }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) { editor_move_vertical(editor, -1); }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { editor_move_vertical(editor, 1); }

    U64 line = editor_caret_line(editor);
    if (line < editor->render.line_count) {
        RenderLine* current = &editor->render.lines[line];
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) { editor_move_to(editor, current->offset, 0); }
        if (ImGui::IsKeyPressed(ImGuiKey_End)) {
            editor_move_to(editor, current->offset + current->size, 0);
        }
    }

    // The piece tree keeps persistent roots, so undo and redo are free.
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        PieceTree::UndoRedoResult undo =
            editor->tree->try_undo(PieceTree::CharOffset{editor->cursor});
        if (undo.success) {
            editor->cursor = std::min(PieceTree::rep(undo.op_offset), editor_length(editor));
            editor->dirty  = 1;
            editor->scroll_to_caret = 1;
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        PieceTree::UndoRedoResult redo =
            editor->tree->try_redo(PieceTree::CharOffset{editor->cursor});
        if (redo.success) {
            editor->cursor = std::min(PieceTree::rep(redo.op_offset), editor_length(editor));
            editor->dirty  = 1;
            editor->scroll_to_caret = 1;
        }
    }

    editor->cursor = std::min(editor->cursor, editor_length(editor));
}

/* ===========================================================================
 * Rendering
 * ======================================================================== */

internal void draw_code_buffer(EditorState* editor) {
    RenderBuffer* render = &editor->render;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("CodeBuffer", 0, window_flags);
    ImGui::PopStyleVar(2);

    float line_height  = ImGui::GetTextLineHeight();
    float glyph_width  = ImGui::CalcTextSize("0").x;  // The default font is monospace.
    float gutter_width = glyph_width * 6.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.063f, 0.075f, 1.0f));
    ImGui::BeginChild("Code",
                      ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    B32 hovered = ImGui::IsWindowHovered();
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { ImGui::SetWindowFocus(); }

    // Focus is taken from the whole window tree, not this child alone, so the
    // buffer keeps accepting keys no matter which part of it was clicked.
    B32 focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (focused) { editor_handle_input(editor); }
    if (editor->dirty) {
        render_buffer_rebuild(render, editor->tree, editor->parser, &editor->syntax_tree);
        editor->dirty = 0;
    }

    ImVec2      origin      = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list   = ImGui::GetWindowDrawList();
    float       view_height = ImGui::GetContentRegionAvail().y;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && render->line_count != 0) {
        ImVec2 mouse   = ImGui::GetMousePos();
        I64    clicked = static_cast<I64>((mouse.y - origin.y) / line_height);
        clicked =
            std::clamp(clicked, static_cast<I64>(0), static_cast<I64>(render->line_count) - 1);

        RenderLine* line   = &render->lines[clicked];
        float       local  = (mouse.x - origin.x - gutter_width) / glyph_width;
        U64         column = local <= 0.0f ? 0 : static_cast<U64>(local + 0.5f);
        editor_move_to(editor, line->offset + std::min(column, line->size), 0);
        editor->scroll_to_caret = 0;  // Clicking already put the caret in view.
    }

    U64 caret_line   = editor_caret_line(editor);
    U64 caret_column = 0;
    if (caret_line < render->line_count) {
        caret_column = editor->cursor - render->lines[caret_line].offset;
    }

    if (editor->scroll_to_caret) {
        float caret_y = static_cast<float>(caret_line) * line_height;
        float top     = ImGui::GetScrollY();
        if (caret_y < top) {
            ImGui::SetScrollY(caret_y);
        } else if (caret_y + line_height > top + view_height) {
            ImGui::SetScrollY(caret_y + line_height - view_height);
        }
        editor->scroll_to_caret = 0;
    }

    // Only the visible lines are emitted; the rest of the buffer costs nothing.
    U64 first_line = static_cast<U64>(ImGui::GetScrollY() / line_height);
    U64 last_line =
        std::min(render->line_count, first_line + static_cast<U64>(view_height / line_height) + 2);

    for (U64 line_index = first_line; line_index < last_line; ++line_index) {
        RenderLine* line = &render->lines[line_index];
        float       y    = origin.y + static_cast<float>(line_index) * line_height;

        char number[16];
        SDL_snprintf(number,
                     sizeof(number),
                     "%4llu",
                     static_cast<unsigned long long>(line_index + 1));
        draw_list->AddText(ImVec2(origin.x, y), IM_COL32(90, 95, 105, 255), number);

        float x        = origin.x + gutter_width;
        U64   line_end = line->offset + line->size;
        U64   drawn    = line->offset;

        for (U64 idx = line->first_span; idx < line->first_span + line->span_count; ++idx) {
            RenderSpan* span  = &render->spans[idx];
            U64         begin = std::max(span->offset, line->offset);
            U64         end   = std::min(span->offset + span->size, line_end);
            if (end <= begin) { continue; }

            // Bytes no span covers: indentation, and anything inside an ERROR
            // node the grammar could not classify.
            if (begin > drawn) { x += static_cast<float>(begin - drawn) * glyph_width; }

            draw_list->AddText(ImVec2(x, y),
                               token_palette[span->kind],
                               render->text.str + begin,
                               render->text.str + end);
            x += static_cast<float>(end - begin) * glyph_width;
            drawn = end;
        }

        if (drawn < line_end) {
            draw_list->AddText(ImVec2(x, y),
                               token_palette[TokenKind_Text],
                               render->text.str + drawn,
                               render->text.str + line_end);
        }
    }

    if (focused) {
        float caret_x = origin.x + gutter_width + static_cast<float>(caret_column) * glyph_width;
        float caret_y = origin.y + static_cast<float>(caret_line) * line_height;
        draw_list->AddRectFilled(ImVec2(caret_x, caret_y),
                                 ImVec2(caret_x + 1.5f, caret_y + line_height),
                                 IM_COL32(220, 220, 220, 255));
    }

    // Nothing above moved the ImGui cursor, so one dummy item establishes the
    // scrollable extent of the whole document.
    ImGui::Dummy(ImVec2(gutter_width + 160.0f * glyph_width,
                        static_cast<float>(render->line_count) * line_height));

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Text("%s", editor->path);
    ImGui::SameLine();
    ImGui::TextDisabled(
        "  |  %s  |  Ln %llu, Col %llu  |  %llu lines  |  %llu bytes  |  %llu spans  |  %s",
        editor->language != 0 ? editor->language->name : "Plain text",
        static_cast<unsigned long long>(caret_line + 1),
        static_cast<unsigned long long>(caret_column + 1),
        static_cast<unsigned long long>(render->line_count),
        static_cast<unsigned long long>(editor_length(editor)),
        static_cast<unsigned long long>(render->span_count),
        focused ? "Ctrl+Z undo, Ctrl+Y redo" : "click the buffer to type");

    ImGui::End();
}

/* ===========================================================================
 * File discovery and loading
 * ======================================================================== */

// std::filesystem picks the file: an explicit argument, otherwise one of
// ETide's own sources found by walking up to the repository root.
internal B32 find_source_file(int argc, char* argv[], fs::path* result) {
    std::error_code error;

    if (argc > 1) {
        fs::path candidate = fs::absolute(fs::path(argv[1]), error);
        if (!error && fs::is_regular_file(candidate, error)) {
            *result = candidate;
            return 1;
        }
        SDL_Log("'%s' is not a readable file", argv[1]);
    }

    fs::path directory = fs::current_path(error);
    if (error) { return 0; }

    const char* preferred[] = {"src/core/core.hpp",
                               "src/piece_tree/piece_tree.hpp",
                               "src/program.cpp"};

    for (U32 depth = 0; depth < 8; ++depth) {
        if (fs::is_regular_file(directory / "src" / "program.cpp", error)) {
            for (U64 idx = 0; idx < SDL_arraysize(preferred); ++idx) {
                fs::path candidate = directory / preferred[idx];
                if (fs::is_regular_file(candidate, error)) {
                    *result = candidate;
                    return 1;
                }
            }
        }
        if (!directory.has_parent_path() || directory.parent_path() == directory) { break; }
        directory = directory.parent_path();
    }
    return 0;
}

internal PieceTree::Tree* load_document(Arena::Arena* storage, const fs::path& path) {
    PieceTree::TreeBuilder builder = PieceTree::tree_builder_start(storage);

    size_t size = 0;
    void*  data = SDL_LoadFile(path.string().c_str(), &size);
    if (data == 0) {
        SDL_Log("SDL_LoadFile failed: %s", SDL_GetError());
    } else {
        // The builder copies the bytes into the document's immutable buffer.
        PieceTree::tree_builder_accept(0, &builder, String::str8(static_cast<char*>(data), size));
        SDL_free(data);
    }

    return PieceTree::tree_builder_finish(&builder);
}

/* ===========================================================================
 * Application
 * ======================================================================== */

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    Arena::Arena* arena = Arena::allocate({});
    if (arena == 0) {
        SDL_Log("Failed to allocate the application arena");
        return SDL_APP_FAILURE;
    }

    EditorState* editor = Arena::push_array<EditorState>(arena, 1);
    if (editor == 0) {
        SDL_Log("Failed to allocate the editor state");
        Arena::release(arena);
        return SDL_APP_FAILURE;
    }
    editor->arena = arena;
    *appstate     = editor;

    if (!UI::init()) { return SDL_APP_FAILURE; }

    fs::path path;
    if (!find_source_file(argc, argv, &path)) {
        SDL_Log("Could not find a source file to open; pass one on the command line");
        return SDL_APP_FAILURE;
    }
    SDL_strlcpy(editor->path, path.string().c_str(), sizeof(editor->path));

    // The document arena becomes the tree's; release_tree frees it.
    Arena::Arena* document_arena = Arena::allocate({});
    if (document_arena == 0) {
        SDL_Log("Failed to allocate the document arena");
        return SDL_APP_FAILURE;
    }
    editor->tree = load_document(document_arena, path);

    editor->render.arena = Arena::allocate({});
    if (editor->render.arena == 0) {
        SDL_Log("Failed to allocate the render arena");
        return SDL_APP_FAILURE;
    }

    // The file extension picks the grammar. Without a match the buffer still
    // opens and edits, just uncolored.
    editor->language = language_from_path(path);
    if (editor->language != 0) {
        editor->parser = ts_parser_new();
        if (editor->parser == 0 ||
            !ts_parser_set_language(editor->parser, editor->language->grammar())) {
            SDL_Log("Failed to set the tree-sitter %s grammar", editor->language->name);
            return SDL_APP_FAILURE;
        }
    }

    editor->dirty = 1;
    SDL_Log("Opened %s (%llu bytes, %llu lines, %s)",
            editor->path,
            static_cast<unsigned long long>(PieceTree::rep(editor->tree->length())),
            static_cast<unsigned long long>(PieceTree::rep(editor->tree->line_count())),
            editor->language != 0 ? editor->language->name : "no grammar");

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
    EditorState* editor = static_cast<EditorState*>(appstate);

    UI::begin_frame();
    draw_code_buffer(editor);
    if (!UI::end_frame()) { return SDL_APP_FAILURE; }

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    EditorState* editor = static_cast<EditorState*>(appstate);
    UI::shutdown();
    if (editor == 0) { return; }

    if (editor->syntax_tree != 0) { ts_tree_delete(editor->syntax_tree); }
    if (editor->parser != 0) { ts_parser_delete(editor->parser); }
    if (editor->tree != 0) { PieceTree::release_tree(editor->tree); }
    if (editor->render.arena != 0) { Arena::release(editor->render.arena); }
    Arena::release(editor->arena);
}
