namespace ETide::Arena {

internal Arena* allocate(Params params) {
    Memory::Allocator* allocator    = params.allocator;
    U64                reserve_size = params.reserve_size;
    U64                commit_size  = params.commit_size;

    void* base = 0;
    U64   page = ETide::page_size();

    reserve_size = AlignPow2(reserve_size, page);
    commit_size  = AlignPow2(commit_size, page);

    base = allocator->reserve(reserve_size);
    if (base == 0) { return 0; }

    B32 commit_succeeded = allocator->commit(base, commit_size);
    SDL_assert(commit_succeeded);
    if (!commit_succeeded) {
        allocator->release(base, reserve_size);
        return 0;
    }

    Arena* arena     = static_cast<Arena*>(base);
    arena->current   = arena;
    arena->prev      = 0;
    arena->flags     = params.flags;
    arena->cmt_size  = commit_size;
    arena->res_size  = reserve_size;
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
        if (new_block == 0) { return 0; }

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

        B32 commit_succeeded = allocator->commit(cmt_ptr, cmt_size);
        SDL_assert(commit_succeeded);
        if (!commit_succeeded) { return 0; }

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

ThreadState::~ThreadState() {
    for (U32 idx = 0; idx < ScratchArenaCount; ++idx) {
        if (scratch_arenas[idx] != 0) {
            release(scratch_arenas[idx]);
            scratch_arenas[idx] = 0;
        }
    }
}

internal Scratch ScratchBegin(Arena** conflicts, U64 count) {
    Scratch result = {};
    if (conflicts == 0 && count > 0) { return result; }

    for (U32 scratch_idx = 0; scratch_idx < ScratchArenaCount; ++scratch_idx) {
        Arena* scratch_arena = thread_state.scratch_arenas[scratch_idx];
        if (scratch_arena == 0) {
            scratch_arena                            = allocate({});
            thread_state.scratch_arenas[scratch_idx] = scratch_arena;
        }
        if (scratch_arena == 0) { continue; }

        B32 has_conflict = 0;
        for (U64 conflict_idx = 0; conflict_idx < count; ++conflict_idx) {
            if (scratch_arena == conflicts[conflict_idx]) {
                has_conflict = 1;
                break;
            }
        }

        if (!has_conflict) {
            result = begin(scratch_arena);
            break;
        }
    }

    return result;
}

internal void ScratchEnd(Scratch scratch) {
    if (scratch.arena != 0) { end(scratch); }
}

}  // namespace ETide::Arena
