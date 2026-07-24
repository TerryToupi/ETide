#if SDL_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/mman.h>
#endif

namespace ETide::Memory {

void* DefaultAllocator::reserve(U64 size) {
    void* result = 0;
#if SDL_PLATFORM_WINDOWS
    result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
#else
    result = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED) { result = 0; }
#endif
    return result;
}

B32 DefaultAllocator::commit(void* ptr, U64 size) {
    B32 result = 0;
#if SDL_PLATFORM_WINDOWS
    result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);
#else
    result = (mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0);
#endif
    return result;
}

void DefaultAllocator::decommit(void* ptr, U64 size) {
#if SDL_PLATFORM_WINDOWS
    VirtualFree(ptr, size, MEM_DECOMMIT);
#else
    madvise(ptr, size, MADV_DONTNEED);
    mprotect(ptr, size, PROT_NONE);
#endif
}

void DefaultAllocator::release(void* ptr, U64 size) {
#if SDL_PLATFORM_WINDOWS
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

}  // namespace ETide::Memory

namespace ETide::Arena {

internal Arena* allocate(Params params) {
    Memory::Allocator* allocator    = params.allocator;
    U64                reserve_size = params.reserve_size;
    U64                commit_size  = params.commit_size;

    void* base = 0;

    reserve_size = AlignPow2(reserve_size, default_page_size);
    commit_size  = AlignPow2(commit_size, default_page_size);

    base = allocator->reserve(reserve_size);
    SDL_assert(allocator->commit(base, commit_size));

    Arena* arena     = static_cast<Arena*>(base);
    arena->current   = arena;
    arena->prev      = 0;
    arena->flags     = params.flags;
    arena->cmt_size  = params.commit_size;
    arena->res_size  = params.reserve_size;
    arena->base_pos  = 0;
    arena->pos       = arena_header_size;
    arena->cmt       = commit_size;
    arena->res       = reserve_size;
    arena->allocator = allocator;

    return arena;
}

internal void release(Arena* arena) {
    Memory::Allocator* allocator = arena->allocator;

    for (Arena *n = arena->current, *prev = 0; n != 0; n = prev) {
        prev = n->prev;
        allocator->release(n, n->res);
    }
}

internal void* push(Arena* arena, U64 size, U64 align, B32 zero) {
    Memory::Allocator* allocator = arena->allocator;

    Arena* current = arena->current;
    U64    pos_pre = AlignPow2(current->pos, align);
    U64    pos_pst = pos_pre + size;

    U64 size_to_zero = 0;
    if (zero) { size_to_zero = std::min(current->cmt, pos_pst) - pos_pre; }

    if (current->res < pos_pst && !(arena->flags & ArenaFlags_NoChain)) {
        U64 res_size = current->res_size;
        U64 cmt_size = current->cmt_size;
        if (size + arena_header_size > res_size) {
            res_size = AlignPow2(size + arena_header_size, align);
            cmt_size = AlignPow2(size + arena_header_size, align);
        }
        Arena* new_block = allocate({.flags        = current->flags,
                                     .reserve_size = res_size,
                                     .commit_size  = cmt_size,
                                     .allocator    = allocator});

        size_to_zero = 0;

        new_block->base_pos = current->base_pos + current->res;
        SLLStackPush_N(arena->current, new_block, prev);

        current = new_block;
        pos_pre = AlignPow2(current->pos, align);
        pos_pst = pos_pre + size;
    }

    if (current->cmt < pos_pst) {
        U64 cmt_pst_aligned = pos_pst + current->cmt_size - 1;
        cmt_pst_aligned -= cmt_pst_aligned % current->cmt_size;
        U64 cmt_pst_clamped = std::min(cmt_pst_aligned, current->res);
        U64 cmt_size        = cmt_pst_clamped - current->cmt;
        U8* cmt_ptr         = (U8*)current + current->cmt;

        SDL_assert(allocator->commit(cmt_ptr, cmt_size));
        current->cmt = cmt_pst_clamped;
    }

    void* result = 0;
    if (current->cmt >= pos_pst) {
        result       = (U8*)current + pos_pre;
        current->pos = pos_pst;
        memset(result, 0, size_to_zero);
    }

    return result;
}

internal U64 pos(Arena* arena) {
    Arena* current = arena->current;
    U64    pos     = current->base_pos + current->pos;
    return pos;
}

internal void pop_to(Arena* arena, U64 pos) {
    Memory::Allocator* allocator = arena->allocator;
    U64                big_pos   = std::max(arena_header_size, pos);
    Arena*             current   = arena->current;

    for (Arena* prev = 0; current->base_pos >= big_pos; current = prev) {
        prev = current->prev;
        allocator->release(current, current->res);
    }

    arena->current = current;
    U64 new_pos    = big_pos - current->base_pos;
    SDL_assert(new_pos <= current->pos);
    current->pos = new_pos;
}

internal void pop(Arena* arena, U64 amt) {
    U64 pos_old = pos(arena);
    U64 pos_new = pos_old;
    if (amt < pos_old) { pos_new = pos_old - amt; }
    pop_to(arena, pos_new);
}

internal void clear(Arena* arena) {
    pop_to(arena, 0);
}

internal Temp begin(Arena* arena) {
    U64  position = pos(arena);
    Temp temp     = {.arena = arena, .pos = position};
    return temp;
}

internal void end(Temp temp) {
    pop_to(temp.arena, temp.pos);
}

}  // namespace ETide::Arena