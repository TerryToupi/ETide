#ifndef CORE_HPP_
#define CORE_HPP_

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

internal U64 ctz32(U32 val);
internal U64 ctz64(U64 val);
internal U64 clz32(U32 val);
internal U64 clz64(U64 val);

internal U64 page_size();

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

namespace ETide::Containers {

// A segmented array that keeps element pointers stable as the container grows.
template <typename T>
class DynamicArray {
   public:
    using value_type      = T;
    using size_type       = U32;
    using difference_type = I64;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using allocator_type  = Memory::Allocator;

    DynamicArray() : DynamicArray(&Memory::default_allocator) {}
    explicit DynamicArray(allocator_type* allocator) : m_allocator(allocator) {
        if (m_allocator == 0) {
            throw std::invalid_argument("DynamicArray allocator cannot be null");
        }
        reserve_storage();
    }
    ~DynamicArray() { release_storage(); }

    DynamicArray(const DynamicArray&)            = delete;
    DynamicArray& operator=(const DynamicArray&) = delete;

    DynamicArray(DynamicArray&& other) noexcept :
        m_allocator(other.m_allocator),
        m_base(other.m_base),
        m_aligned_reservation_size(other.m_aligned_reservation_size),
        m_current_segment_entry(other.m_current_segment_entry),
        m_capacity(other.m_capacity),
        m_used_segments(other.m_used_segments),
        m_count(other.m_count) {
        for (U32 idx = 0; idx < kMaxSegments; ++idx) {
            m_segments[idx]       = other.m_segments[idx];
            other.m_segments[idx] = 0;
        }

        other.m_allocator                = &Memory::default_allocator;
        other.m_base                     = 0;
        other.m_aligned_reservation_size = 0;
        other.m_current_segment_entry    = 0;
        other.m_capacity                 = 0;
        other.m_used_segments            = 0;
        other.m_count                    = 0;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        swap(*this, other);
        return *this;
    }

    friend void swap(DynamicArray& a, DynamicArray& b) noexcept {
        using std::swap;
        swap(a.m_allocator, b.m_allocator);
        swap(a.m_base, b.m_base);
        swap(a.m_aligned_reservation_size, b.m_aligned_reservation_size);
        swap(a.m_current_segment_entry, b.m_current_segment_entry);
        swap(a.m_capacity, b.m_capacity);
        swap(a.m_count, b.m_count);
        swap(a.m_used_segments, b.m_used_segments);
        swap(a.m_segments, b.m_segments);
    }

    void clear() noexcept {
        U32 remaining_count = m_count;
        for (U32 segment_idx = 0; segment_idx < m_used_segments; ++segment_idx) {
            U32 segment_size = slots_in_segment(segment_idx);
            T*  segment      = m_segments[segment_idx];

            for (U32 idx = 0; idx < segment_size && remaining_count > 0; ++idx, --remaining_count) {
                std::destroy_at(&segment[idx]);
            }

            m_segments[segment_idx] = 0;
        }

        if (m_used_segments > 0) {
            void* ptr  = m_base;
            U64   size = static_cast<U64>(capacity()) * sizeof(T);
            align_to_page(&ptr, &size);
            m_allocator->decommit(ptr, size);
        }

        m_current_segment_entry = 0;
        m_capacity              = 0;
        m_used_segments         = 0;
        m_count                 = 0;
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (m_count == m_capacity) { add_segment(); }

        T* entry = m_current_segment_entry;
        std::construct_at(entry, std::forward<Args>(args)...);
        ++m_current_segment_entry;
        ++m_count;
        return *entry;
    }

    reference push_back(const T& value) { return emplace_back(value); }
    reference push_back(T&& value) { return emplace_back(std::move(value)); }

    const_reference operator[](U32 idx) const { return *get(idx); }
    reference       operator[](U32 idx) { return *get(idx); }

    const_reference at(U32 idx) const {
        if (idx >= m_count) { throw std::out_of_range("DynamicArray index out of range"); }
        return *get(idx);
    }
    reference at(U32 idx) {
        if (idx >= m_count) { throw std::out_of_range("DynamicArray index out of range"); }
        return *get(idx);
    }

    const_reference front() const { return *get(0); }
    reference       front() { return *get(0); }
    const_reference back() const { return *get(m_count - 1); }
    reference       back() { return *get(m_count - 1); }

    constexpr B32        empty() const { return m_count == 0; }
    constexpr U32        size() const { return m_count; }
    constexpr U32        capacity() const { return m_capacity; }
    static constexpr U32 max_size() { return capacity_for_segment_count(kMaxSegments); }

    allocator_type* get_allocator() const { return m_allocator; }

   private:
    static constexpr U32 kSmallSegmentsToSkip = 6;
    static constexpr U32 kMaxSegments         = 26;

    static constexpr U32 slots_in_segment(U32 segment_index) {
        return static_cast<U32>((U64{1} << kSmallSegmentsToSkip) << segment_index);
    }

    static constexpr U32 capacity_for_segment_count(U32 segment_count) {
        return static_cast<U32>(((U64{1} << kSmallSegmentsToSkip) << segment_count) -
                                (U64{1} << kSmallSegmentsToSkip));
    }

    static constexpr U32 segment_for_index(U32 idx) {
        U32 value = (idx >> kSmallSegmentsToSkip) + 1;
        return 31 - static_cast<U32>(clz32(value));
    }

    static void align_to_page(void** ptr, U64* size) {
        U64 page            = ETide::page_size();
        U64 address         = reinterpret_cast<U64>(*ptr);
        U64 aligned_address = AlignDownPow2(address, page);
        U64 aligned_size    = AlignPow2(*size + address - aligned_address, page);

        *ptr  = reinterpret_cast<void*>(aligned_address);
        *size = aligned_size;
    }

    void reserve_storage() {
        m_aligned_reservation_size =
            AlignPow2(static_cast<U64>(max_size()) * sizeof(T), ETide::page_size());
        m_base = m_allocator->reserve(m_aligned_reservation_size);
        if (m_base == 0) { throw std::bad_alloc(); }
    }

    void release_storage() noexcept {
        clear();
        if (m_base != 0) {
            m_allocator->release(m_base, m_aligned_reservation_size);
            m_base                     = 0;
            m_aligned_reservation_size = 0;
        }
    }

    void add_segment() {
        if (m_used_segments == kMaxSegments) {
            throw std::length_error("DynamicArray reached maximum size");
        }
        if (m_base == 0) { reserve_storage(); }

        U32 segment_size = slots_in_segment(m_used_segments);
        U64 byte_size    = static_cast<U64>(segment_size) * sizeof(T);
        U64 byte_offset = static_cast<U64>(capacity_for_segment_count(m_used_segments)) * sizeof(T);
        T*  segment     = reinterpret_cast<T*>(static_cast<U8*>(m_base) + byte_offset);

        void* commit_ptr  = segment;
        U64   commit_size = byte_size;
        align_to_page(&commit_ptr, &commit_size);

        B32 commit_succeeded = m_allocator->commit(commit_ptr, commit_size);
        if (!commit_succeeded) { throw std::bad_alloc(); }

        m_segments[m_used_segments++] = segment;
        m_current_segment_entry       = segment;
        m_capacity += segment_size;
    }

    T* get(U32 idx) {
        U32 segment = segment_for_index(idx);
        U32 slot    = idx - capacity_for_segment_count(segment);
        return &m_segments[segment][slot];
    }

    const T* get(U32 idx) const {
        U32 segment = segment_for_index(idx);
        U32 slot    = idx - capacity_for_segment_count(segment);
        return &m_segments[segment][slot];
    }

    Memory::Allocator* m_allocator                = &Memory::default_allocator;
    void*              m_base                     = 0;
    U64                m_aligned_reservation_size = 0;
    T*                 m_current_segment_entry    = 0;
    U32                m_capacity                 = 0;
    U32                m_used_segments            = 0;
    U32                m_count                    = 0;
    T*                 m_segments[kMaxSegments]   = {0};
};

}  // namespace ETide::Containers

#endif
