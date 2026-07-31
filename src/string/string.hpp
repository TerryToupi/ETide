#ifndef STRING_HPP_
#define STRING_HPP_

namespace ETide::String {

typedef struct String8 String8;
struct String8 {
    char* str;
    U64   size;
};

typedef struct String8View String8View;
struct String8View {
    const char* str;
    U64         size;
};

typedef struct String8Node String8Node;
struct String8Node {
    String8Node* next;
    String8      string;
};

typedef struct String8List String8List;
struct String8List {
    String8Node* first;
    String8Node* last;
    U64          node_count;
    U64          total_size;
};

global String8 str8_empty = {};

internal constexpr String8 str8(char* str, U64 size) {
    return {.str = str, .size = size};
}

template <U64 N>
internal constexpr String8 str8(char (&str)[N]) {
    return {.str = str, .size = N};
}

template <U64 N>
internal constexpr String8View str8_literal(const char (&str)[N]) {
    return {.str = str, .size = N - 1};
}

internal String8Node* str8_list_push_node(String8List* list, String8Node* node);
internal String8Node* str8_list_push_node_set_string(String8List* list,
                                                     String8Node* node,
                                                     String8      string);
internal String8Node* str8_list_push(Arena::Arena* arena, String8List* list, String8 string);
internal String8      str8_list_join(Arena::Arena* arena, String8List* list);

internal void    str8_serial_begin(Arena::Arena* arena, String8List* list);
internal String8 str8_serial_end(Arena::Arena* arena, String8List* list);
internal String8 str8_serial_end(Arena::Arena* arena, String8List& list);
internal void    str8_serial_push_char(Arena::Arena* arena, String8List* list, char c);
internal void    str8_serial_push_str8(Arena::Arena* arena, String8List* list, String8 string);

internal String8 str8_cstr(char* str);
internal String8 str8_mut(String8View string);
internal String8 str8_alloc(Arena::Arena* arena, U64 size);
internal String8 str8_cstr_alloc(Arena::Arena* arena, U64 size);
internal String8 str8_copy(Arena::Arena* arena, String8 string);
internal B32     str8_match_exact(String8 a, String8 b);

}  // namespace ETide::String

#endif
