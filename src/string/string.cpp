namespace ETide::String {

internal String8Node* str8_list_push_node(String8List* list, String8Node* node) {
    if (list == 0 || node == 0) { return 0; }
    if (UINT64_MAX - list->total_size < node->string.size) {
        SDL_SetError("String8List size overflow");
        return 0;
    }

    SLLQueuePush(list->first, list->last, node);
    ++list->node_count;
    list->total_size += node->string.size;
    return node;
}

internal String8Node* str8_list_push_node_set_string(String8List* list,
                                                     String8Node* node,
                                                     String8      string) {
    if (node == 0) { return 0; }
    node->string = string;
    return str8_list_push_node(list, node);
}

internal String8Node* str8_list_push(Arena::Arena* arena, String8List* list, String8 string) {
    if (arena == 0 || list == 0) { return 0; }
    String8Node* node = Arena::push_array<String8Node>(arena, 1);
    if (node == 0) { return 0; }
    return str8_list_push_node_set_string(list, node, string);
}

internal String8 str8_list_join(Arena::Arena* arena, String8List* list) {
    if (arena == 0 || list == 0) { return {}; }
    if (list->total_size == UINT64_MAX) {
        SDL_SetError("String8List is too large to terminate");
        return {};
    }

    String8 result = str8_cstr_alloc(arena, list->total_size);
    if (result.str == 0) { return {}; }

    U64 offset = 0;
    for (String8Node* node = list->first; node != 0; node = node->next) {
        if (node->string.size != 0) {
            SDL_memcpy(result.str + offset, node->string.str, node->string.size);
            offset += node->string.size;
        }
    }
    return result;
}

internal void str8_serial_begin(Arena::Arena* arena, String8List* list) {
    if (list == 0) { return; }
    *list = {};
    (void)arena;
}

internal String8 str8_serial_end(Arena::Arena* arena, String8List* list) {
    return str8_list_join(arena, list);
}

internal String8 str8_serial_end(Arena::Arena* arena, String8List& list) {
    return str8_list_join(arena, &list);
}

internal void str8_serial_push_char(Arena::Arena* arena, String8List* list, char c) {
    String8 string = str8_alloc(arena, 1);
    if (string.str == 0) { return; }
    string.str[0] = c;
    str8_list_push(arena, list, string);
}

internal void str8_serial_push_str8(Arena::Arena* arena, String8List* list, String8 string) {
    str8_list_push(arena, list, string);
}

internal String8 str8_cstr(char* str) {
    if (str == 0) { return {}; }
    return {.str = str, .size = SDL_strlen(str)};
}

internal String8 str8_mut(String8View string) {
    return {.str = const_cast<char*>(string.str), .size = string.size};
}

internal String8 str8_alloc(Arena::Arena* arena, U64 size) {
    if (arena == 0) { return {}; }
    char* memory = Arena::push_array_no_zero_aligned<char>(arena, size, 1);
    if (memory == 0 && size != 0) { return {}; }
    return {.str = memory, .size = size};
}

internal String8 str8_cstr_alloc(Arena::Arena* arena, U64 size) {
    if (arena == 0 || size == UINT64_MAX) { return {}; }
    char* memory = Arena::push_array_no_zero_aligned<char>(arena, size + 1, 1);
    if (memory == 0) { return {}; }
    memory[size] = 0;
    return {.str = memory, .size = size};
}

internal String8 str8_copy(Arena::Arena* arena, String8 string) {
    String8 result = str8_cstr_alloc(arena, string.size);
    if (result.str != 0 && string.size != 0) { SDL_memcpy(result.str, string.str, string.size); }
    return result;
}

internal B32 str8_match_exact(String8 a, String8 b) {
    if (a.size != b.size) { return 0; }
    if (a.size == 0) { return 1; }
    if (a.str == 0 || b.str == 0) { return 0; }
    return SDL_memcmp(a.str, b.str, a.size) == 0;
}

}  // namespace ETide::String
