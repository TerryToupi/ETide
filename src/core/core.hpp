#ifndef CORE_HPP_
#define CORE_HPP_

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define internal      static
#define global        static
#define local_persist static

#define KB(n)       (((U64)(n)) << 10)
#define MB(n)       (((U64)(n)) << 20)
#define GB(n)       (((U64)(n)) << 30)
#define TB(n)       (((U64)(n)) << 40)
#define Thousand(n) ((n) * 1000)
#define Million(n)  ((n) * 1000000)
#define Billion(n)  ((n) * 1000000000)

#define Compose64Bit(a, b)  ((((U64)a) << 32) | ((U64)b))
#define Compose32Bit(a, b)  ((((U32)a) << 16) | ((U32)b))
#define AlignPow2(x, b)     (((x) + (b) - 1) & (~((b) - 1)))
#define AlignDownPow2(x, b) ((x) & (~((b) - 1)))
#define AlignPadPow2(x, b)  ((0 - (x)) & ((b) - 1))
#define IsPow2(x)           ((x) != 0 && ((x) & ((x) - 1)) == 0)
#define IsPow2OrZero(x)     ((((x) - 1) & (x)) == 0)

#define ExtractBit(word, idx) (((word) >> (idx)) & 1)
#define Extract8(word, pos)   (((word) >> ((pos) * 8)) & max_U8)
#define Extract16(word, pos)  (((word) >> ((pos) * 16)) & max_U16)
#define Extract32(word, pos)  (((word) >> ((pos) * 32)) & max_U32)

#define CheckNil(nil, p) ((p) == 0 || (p) == nil)
#define SetNil(nil, p)   ((p) = nil)
#define DLLInsert_NPZ(nil, f, l, p, n, next, prev)                                                 \
    (CheckNil(nil, f)   ? ((f) = (l) = (n), SetNil(nil, (n)->next), SetNil(nil, (n)->prev))        \
     : CheckNil(nil, p) ? ((n)->next = (f), (f)->prev = (n), (f) = (n), SetNil(nil, (n)->prev))    \
     : ((p) == (l))                                                                                \
         ? ((l)->next = (n), (n)->prev = (l), (l) = (n), SetNil(nil, (n)->next))                   \
         : (((!CheckNil(nil, p) && CheckNil(nil, (p)->next)) ? (0) : ((p)->next->prev = (n))),     \
            ((n)->next = (p)->next),                                                               \
            ((p)->next = (n)),                                                                     \
            ((n)->prev = (p))))
#define DLLPushBack_NPZ(nil, f, l, n, next, prev)  DLLInsert_NPZ(nil, f, l, l, n, next, prev)
#define DLLPushFront_NPZ(nil, f, l, n, next, prev) DLLInsert_NPZ(nil, l, f, f, n, prev, next)
#define DLLRemove_NPZ(nil, f, l, n, next, prev)                                                    \
    (((n) == (f) ? (f) = (n)->next : (0)),                                                         \
     ((n) == (l) ? (l) = (l)->prev : (0)),                                                         \
     (CheckNil(nil, (n)->prev) ? (0) : ((n)->prev->next = (n)->next)),                             \
     (CheckNil(nil, (n)->next) ? (0) : ((n)->next->prev = (n)->prev)))
#define SLLQueuePush_NZ(nil, f, l, n, next)                                                        \
    (CheckNil(nil, f) ? ((f) = (l) = (n), SetNil(nil, (n)->next))                                  \
                      : ((l)->next = (n), (l) = (n), SetNil(nil, (n)->next)))
#define SLLQueuePushFront_NZ(nil, f, l, n, next)                                                   \
    (CheckNil(nil, f) ? ((f) = (l) = (n), SetNil(nil, (n)->next)) : ((n)->next = (f), (f) = (n)))
#define SLLQueuePop_NZ(nil, f, l, next)                                                            \
    ((f) == (l) ? (SetNil(nil, f), SetNil(nil, l)) : ((f) = (f)->next))
#define SLLStackPush_N(f, n, next)           ((n)->next = (f), (f) = (n))
#define SLLStackPop_N(f, next)               ((f) = (f)->next)
#define DLLInsert_NP(f, l, p, n, next, prev) DLLInsert_NPZ(0, f, l, p, n, next, prev)
#define DLLPushBack_NP(f, l, n, next, prev)  DLLPushBack_NPZ(0, f, l, n, next, prev)
#define DLLPushFront_NP(f, l, n, next, prev) DLLPushFront_NPZ(0, f, l, n, next, prev)
#define DLLRemove_NP(f, l, n, next, prev)    DLLRemove_NPZ(0, f, l, n, next, prev)
#define DLLInsert(f, l, p, n)                DLLInsert_NPZ(0, f, l, p, n, next, prev)
#define DLLPushBack(f, l, n)                 DLLPushBack_NPZ(0, f, l, n, next, prev)
#define DLLPushFront(f, l, n)                DLLPushFront_NPZ(0, f, l, n, next, prev)
#define DLLRemove(f, l, n)                   DLLRemove_NPZ(0, f, l, n, next, prev)
#define SLLQueuePush_N(f, l, n, next)        SLLQueuePush_NZ(0, f, l, n, next)
#define SLLQueuePushFront_N(f, l, n, next)   SLLQueuePushFront_NZ(0, f, l, n, next)
#define SLLQueuePop_N(f, l, next)            SLLQueuePop_NZ(0, f, l, next)
#define SLLQueuePush(f, l, n)                SLLQueuePush_NZ(0, f, l, n, next)
#define SLLQueuePushFront(f, l, n)           SLLQueuePushFront_NZ(0, f, l, n, next)
#define SLLQueuePop(f, l)                    SLLQueuePop_NZ(0, f, l, next)
#define SLLStackPush(f, n)                   SLLStackPush_N(f, n, next)
#define SLLStackPop(f)                       SLLStackPop_N(f, next)

namespace ETide {

typedef uint8_t  U8;
typedef int8_t   I8;
typedef int8_t   B8;
typedef uint16_t U16;
typedef int16_t  I16;
typedef int16_t  B16;
typedef uint32_t U32;
typedef int32_t  I32;
typedef int32_t  B32;
typedef uint64_t U64;
typedef int64_t  I64;
typedef int64_t  B64;
typedef float    F32;
typedef double   F64;

}  // namespace ETide

namespace ETide::Memory {

class Allocator {
   public:
    virtual ~Allocator() = default;

    Allocator(const Allocator&)            = delete;
    Allocator& operator=(const Allocator&) = delete;

    virtual void* reserve(U64 size)             = 0;
    virtual B32   commit(void* ptr, U64 size)   = 0;
    virtual void  decommit(void* ptr, U64 size) = 0;
    virtual void  release(void* ptr, U64 size)  = 0;

   protected:
    Allocator() = default;
};

class DefaultAllocator final : public Allocator {
   public:
    DefaultAllocator()           = default;
    ~DefaultAllocator() override = default;

    virtual void* reserve(U64 size) override;
    virtual B32   commit(void* ptr, U64 size) override;
    virtual void  decommit(void* ptr, U64 size) override;
    virtual void  release(void* ptr, U64 size) override;
};

global DefaultAllocator default_allocator = {};

}  // namespace ETide::Memory

namespace ETide::Arena {

typedef U64 Flags;
enum {
    ArenaFlags_None    = 0,
    ArenaFlags_NoChain = (1 << 0),
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

global U64 arena_header_size = 128;
global U64 default_page_size = KB(4);

internal Arena* allocate(Params params);
internal void   release(Arena* arena);

internal void* push(Arena* arena, U64 size, U64 align, B32 zero);
internal U64   pos(Arena* arena);
internal void  pop_to(Arena* arena, U64 pos);
internal void  pop(Arena* arena, U64 amt);
internal void  clear(Arena* arena);

internal Temp begin(Arena* arena);
internal void end(Temp temp);

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
