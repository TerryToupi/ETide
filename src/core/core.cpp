
#if SDL_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/mman.h>
#endif

#if defined(_MSC_VER)
#    include <intrin.h>
#endif

namespace ETide {

internal U64 ctz32(U32 val) {
    if (val == 0) { return 32; }

#if defined(_MSC_VER)
    unsigned long idx = 0;
    _BitScanForward(&idx, val);
    return idx;
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_ctz(val));
#else
    U64 result = 0;
    while ((val & 1) == 0) {
        val >>= 1;
        ++result;
    }
    return result;
#endif
}

internal U64 ctz64(U64 val) {
    if (val == 0) { return 64; }

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64) || defined(_M_ARM64EC))
    unsigned long idx = 0;
    _BitScanForward64(&idx, val);
    return idx;
#elif defined(_MSC_VER)
    U32 low = static_cast<U32>(val);
    return low != 0 ? ctz32(low) : 32 + ctz32(static_cast<U32>(val >> 32));
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_ctzll(val));
#else
    U64 result = 0;
    while ((val & 1) == 0) {
        val >>= 1;
        ++result;
    }
    return result;
#endif
}

internal U64 clz32(U32 val) {
    if (val == 0) { return 32; }

#if defined(_MSC_VER)
    unsigned long idx = 0;
    _BitScanReverse(&idx, val);
    return 31 - idx;
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_clz(val));
#else
    U64 result = 0;
    U32 bit    = U32{1} << 31;
    while ((val & bit) == 0) {
        bit >>= 1;
        ++result;
    }
    return result;
#endif
}

internal U64 clz64(U64 val) {
    if (val == 0) { return 64; }

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64) || defined(_M_ARM64EC))
    unsigned long idx = 0;
    _BitScanReverse64(&idx, val);
    return 63 - idx;
#elif defined(_MSC_VER)
    U32 high = static_cast<U32>(val >> 32);
    return high != 0 ? clz32(high) : 32 + clz32(static_cast<U32>(val));
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_clzll(val));
#else
    U64 result = 0;
    U64 bit    = U64{1} << 63;
    while ((val & bit) == 0) {
        bit >>= 1;
        ++result;
    }
    return result;
#endif
}

internal Mutex* mutex_create() {
    return reinterpret_cast<Mutex*>(SDL_CreateMutex());
}

internal void mutex_destroy(Mutex* mutex) {
    SDL_DestroyMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal void mutex_lock(Mutex* mutex) {
    SDL_LockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal B32 mutex_try_lock(Mutex* mutex) {
    return SDL_TryLockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal void mutex_unlock(Mutex* mutex) {
    SDL_UnlockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal RWLock* rwlock_create() {
    return reinterpret_cast<RWLock*>(SDL_CreateRWLock());
}

internal void rwlock_destroy(RWLock* lock) {
    SDL_DestroyRWLock(reinterpret_cast<SDL_RWLock*>(lock));
}

internal void rwlock_read_lock(RWLock* lock) {
    SDL_LockRWLockForReading(reinterpret_cast<SDL_RWLock*>(lock));
}

internal void rwlock_write_lock(RWLock* lock) {
    SDL_LockRWLockForWriting(reinterpret_cast<SDL_RWLock*>(lock));
}

internal B32 rwlock_try_read_lock(RWLock* lock) {
    return SDL_TryLockRWLockForReading(reinterpret_cast<SDL_RWLock*>(lock));
}

internal B32 rwlock_try_write_lock(RWLock* lock) {
    return SDL_TryLockRWLockForWriting(reinterpret_cast<SDL_RWLock*>(lock));
}

internal void rwlock_unlock(RWLock* lock) {
    SDL_UnlockRWLock(reinterpret_cast<SDL_RWLock*>(lock));
}

internal U64 page_size() {
    U64 result = static_cast<U64>(SDL_GetSystemPageSize());
    if (result == 0) { result = KB(4); }
    return result;
}

}  // namespace ETide

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
