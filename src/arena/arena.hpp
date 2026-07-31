#ifndef ARENA_HPP_
#define ARENA_HPP_

namespace ETide::Arena {

typedef U64 Flags;
enum {
    ArenaFlags_None    = 0,
    ArenaFlags_NoChain = (1 << 0),
};

enum {
    ScratchArenaCount = 2,
};

struct Params {
    Flags              flags        = ArenaFlags_None;
    U64                reserve_size = MB(64);
    U64                commit_size  = KB(64);
    Memory::Allocator* allocator    = &Memory::default_allocator;
};

struct Arena {
    Arena*             prev;
    Arena*             current;
    Flags              flags;
    Memory::Allocator* allocator;
    U64                cmt_size;
    U64                res_size;
    U64                base_pos;
    U64                pos;
    U64                cmt;
    U64                res;
};

struct Temp {
    Arena* arena;
    U64    pos;
};

typedef Temp Scratch;

struct ThreadState {
    Arena* scratch_arenas[ScratchArenaCount];

    ~ThreadState();
};

global thread_local ThreadState thread_state      = {};
global U64                      arena_header_size = 128;

internal Arena* allocate(Params params);
internal void   release(Arena* arena);

internal void* push(Arena* arena, U64 size, U64 align, B32 zero);
internal U64   pos(Arena* arena);
internal void  pop_to(Arena* arena, U64 pos);
internal void  pop(Arena* arena, U64 amt);
internal void  clear(Arena* arena);

internal Temp begin(Arena* arena);
internal void end(Temp temp);

internal Scratch ScratchBegin(Arena** conflicts, U64 count);
internal void    ScratchEnd(Scratch scratch);

template <typename T>
internal inline constexpr T* push_array_no_zero_aligned(Arena* arena, U64 count, U64 align) {
    static_assert(std::is_trivially_destructible_v<T>);
    return static_cast<T*>(push(arena, sizeof(T) * count, align, 0));
}

template <typename T>
internal inline constexpr T* push_array_aligned(Arena* arena, U64 count, U64 align) {
    static_assert(std::is_trivially_destructible_v<T>);
    return static_cast<T*>(push(arena, sizeof(T) * count, align, 1));
}

template <typename T>
internal inline constexpr T* push_array_no_zero(Arena* arena, U64 count) {
    return push_array_no_zero_aligned<T>(arena, count, std::max(8ull, (U64)alignof(T)));
}

template <typename T>
internal inline constexpr T* push_array(Arena* arena, U64 count) {
    return push_array_aligned<T>(arena, count, std::max(8ull, (U64)alignof(T)));
}

}  // namespace ETide::Arena

#endif
