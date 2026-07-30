#ifndef CORE_HPP_
#define CORE_HPP_

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

typedef struct Mutex  Mutex;
typedef struct RWLock RWLock;

typedef enum WeekDay {
    WeekDay_Sun,
    WeekDay_Mon,
    WeekDay_Tue,
    WeekDay_Wed,
    WeekDay_Thu,
    WeekDay_Fri,
    WeekDay_Sat,
    WeekDay_COUNT,
} WeekDay;

typedef enum Month {
    Month_Jan,
    Month_Feb,
    Month_Mar,
    Month_Apr,
    Month_May,
    Month_Jun,
    Month_Jul,
    Month_Aug,
    Month_Sep,
    Month_Oct,
    Month_Nov,
    Month_Dec,
    Month_COUNT,
} Month;

typedef struct DateTime DateTime;
struct DateTime {
    U16 micro_sec;  // [0,999]
    U16 msec;       // [0,999]
    U16 sec;        // [0,60]
    U16 min;        // [0,59]
    U16 hour;       // [0,24]
    U16 day;        // [0,30]
    union {
        WeekDay week_day;
        U32     wday;
    };
    union {
        Month month;
        U32   mon;
    };
    U32 year;  // 1 = 1 CE, 0 = 1 BC
};

typedef U64 DenseTime;

internal DenseTime dense_time_from_date_time(DateTime date_time);
internal DateTime  date_time_from_dense_time(DenseTime time);
internal DateTime  date_time_from_micro_seconds(U64 time);
internal DateTime  date_time_from_unix_time(U64 unix_time);

internal U64 now_time_us(void);
internal DateTime now_time_universal(void);
internal DateTime universal_from_local_time(DateTime *dt);
internal DateTime local_from_universal_time(DateTime *dt);
internal void sleep_ms(U32 ms);

internal U64 ctz32(U32 val);
internal U64 ctz64(U64 val);
internal U64 clz32(U32 val);
internal U64 clz64(U64 val);

internal Mutex* mutex_create();
internal void   mutex_destroy(Mutex* mutex);
internal void   mutex_lock(Mutex* mutex);
internal B32    mutex_try_lock(Mutex* mutex);
internal void   mutex_unlock(Mutex* mutex);

internal RWLock* rwlock_create();
internal void    rwlock_destroy(RWLock* lock);
internal void    rwlock_read_lock(RWLock* lock);
internal void    rwlock_write_lock(RWLock* lock);
internal B32     rwlock_try_read_lock(RWLock* lock);
internal B32     rwlock_try_write_lock(RWLock* lock);
internal void    rwlock_unlock(RWLock* lock);

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

namespace ETide {

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
internal String8Node* str8_list_push_node_set_string(String8List* list, String8Node* node,
                                                     String8 string);
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

}  // namespace ETide

namespace ETide::Containers {

// A segmented array that keeps element pointers stable as the container grows.
template <typename T>
class DynamicArray {
   public:
    static_assert(std::is_trivially_destructible_v<T>,
                  "DynamicArray only accepts trivially destructible types");

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
        for (U32 segment_idx = 0; segment_idx < m_used_segments; ++segment_idx) {
            m_segments[segment_idx] = 0;
        }

        if (m_used_segments > 0) {
            U64   page_size                = ETide::page_size();
            U64   decommit_address         = reinterpret_cast<U64>(m_base);
            U64   aligned_decommit_address = AlignDownPow2(decommit_address, page_size);
            void* aligned_decommit_ptr     = reinterpret_cast<void*>(aligned_decommit_address);
            U64   decommit_size            = static_cast<U64>(capacity()) * sizeof(T);
            U64   aligned_decommit_size =
                AlignPow2(decommit_size + decommit_address - aligned_decommit_address, page_size);
            m_allocator->decommit(aligned_decommit_ptr, aligned_decommit_size);
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

        U64   page_size              = ETide::page_size();
        U64   segment_address        = reinterpret_cast<U64>(segment);
        U64   aligned_commit_address = AlignDownPow2(segment_address, page_size);
        void* aligned_commit_ptr     = reinterpret_cast<void*>(aligned_commit_address);
        U64   aligned_commit_size =
            AlignPow2(byte_size + segment_address - aligned_commit_address, page_size);

        B32 commit_succeeded = m_allocator->commit(aligned_commit_ptr, aligned_commit_size);
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

template <typename T>
class Handle {
   public:
    using value_type = U64;

    Handle() = default;
    explicit Handle(U64 value) : m_value(value) {}

    explicit operator bool() const { return m_value != 0; }
    U64      value() const { return m_value; }

    friend B32 operator==(Handle a, Handle b) { return a.m_value == b.m_value; }
    friend B32 operator!=(Handle a, Handle b) { return a.m_value != b.m_value; }

   private:
    template <typename>
    friend class Pool;

    U64 m_value = 0;
};

// A segmented object pool with stable addresses and generation-checked handles.
template <typename T>
class Pool {
   public:
    static_assert(std::is_trivially_destructible_v<T>,
                  "Pool only accepts trivially destructible types");

    using value_type      = T;
    using size_type       = U32;
    using reference       = T&;
    using const_reference = const T&;
    using allocator_type  = Memory::Allocator;
    using handle_type     = Handle<T>;

    Pool() : Pool(&Memory::default_allocator) {}
    explicit Pool(allocator_type* allocator) : m_allocator(allocator) {
        if (m_allocator == 0) { throw std::invalid_argument("Pool allocator cannot be null"); }
        reserve_storage();
        m_lock = rwlock_create();
        if (m_lock == 0) {
            m_allocator->release(m_base, m_aligned_reservation_size);
            m_base                     = 0;
            m_aligned_reservation_size = 0;
            throw std::bad_alloc();
        }
    }
    ~Pool() {
        release_storage();
        rwlock_destroy(m_lock);
        m_lock = 0;
    }

    Pool(const Pool&)            = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&)                 = delete;
    Pool& operator=(Pool&&)      = delete;

    void clear() {
        rwlock_write_lock(m_lock);

        for (U32 segment_idx = 0; segment_idx < m_used_segments; ++segment_idx) {
            m_segments[segment_idx] = 0;
        }

        if (m_used_segments > 0) {
            U64   page_size                = ETide::page_size();
            U64   decommit_address         = reinterpret_cast<U64>(m_base);
            U64   aligned_decommit_address = AlignDownPow2(decommit_address, page_size);
            void* aligned_decommit_ptr     = reinterpret_cast<void*>(aligned_decommit_address);
            U64   decommit_size            = static_cast<U64>(m_capacity) * sizeof(Entry);
            U64   aligned_decommit_size =
                AlignPow2(decommit_size + decommit_address - aligned_decommit_address, page_size);
            m_allocator->decommit(aligned_decommit_ptr, aligned_decommit_size);
        }

        m_capacity      = 0;
        m_used_segments = 0;
        m_count         = 0;
        m_head          = kEndOfList;
        rwlock_unlock(m_lock);
    }

    template <typename... Args>
    handle_type emplace(Args&&... args) {
        rwlock_write_lock(m_lock);
        if (m_head == kEndOfList) {
            if (m_used_segments == kMaxSegments) {
                rwlock_unlock(m_lock);
                throw std::length_error("Pool reached maximum size");
            }
            if (!add_segment()) {
                rwlock_unlock(m_lock);
                throw std::bad_alloc();
            }
        }

        U32    idx   = m_head;
        Entry* entry = get(idx);
        if (entry->next == kNotInFreelist) {
            rwlock_unlock(m_lock);
            throw std::runtime_error("Pool freelist is corrupted");
        }

        try {
            std::construct_at(entry->storage_ptr(), std::forward<Args>(args)...);
        } catch (...) {
            rwlock_unlock(m_lock);
            throw;
        }

        m_head              = entry->next;
        entry->next         = kNotInFreelist;
        m_latest_generation = next_generation(m_latest_generation);
        entry->generation   = m_latest_generation;
        ++m_count;
        handle_type handle = create_handle(idx, entry->generation);
        rwlock_unlock(m_lock);
        return handle;
    }

    void erase(handle_type handle) {
        rwlock_write_lock(m_lock);
        DecomposedHandle decomposed = decompose_handle(handle);
        Entry*           entry      = get_if_valid(decomposed);
        if (entry == 0) {
            rwlock_unlock(m_lock);
            throw std::out_of_range("Pool handle is invalid or stale");
        }

        entry->next = m_head;
        m_head      = decomposed.idx;
        --m_count;
        rwlock_unlock(m_lock);
    }

    reference operator[](handle_type handle) {
        rwlock_read_lock(m_lock);
        Entry* entry = get_if_valid(decompose_handle(handle));
        if (entry == 0) {
            rwlock_unlock(m_lock);
            throw std::out_of_range("Pool handle is invalid or stale");
        }
        T* value = entry->value();
        rwlock_unlock(m_lock);
        return *value;
    }
    const_reference operator[](handle_type handle) const {
        rwlock_read_lock(m_lock);
        const Entry* entry = get_if_valid(decompose_handle(handle));
        if (entry == 0) {
            rwlock_unlock(m_lock);
            throw std::out_of_range("Pool handle is invalid or stale");
        }
        const T* value = entry->value();
        rwlock_unlock(m_lock);
        return *value;
    }

    reference       at(handle_type handle) { return (*this)[handle]; }
    const_reference at(handle_type handle) const { return (*this)[handle]; }

    B32 contains(handle_type handle) const {
        rwlock_read_lock(m_lock);
        B32 result = get_if_valid(decompose_handle(handle)) != 0;
        rwlock_unlock(m_lock);
        return result;
    }

    B32 empty() const {
        rwlock_read_lock(m_lock);
        B32 result = m_count == 0;
        rwlock_unlock(m_lock);
        return result;
    }
    U32 size() const {
        rwlock_read_lock(m_lock);
        U32 result = m_count;
        rwlock_unlock(m_lock);
        return result;
    }
    U32 capacity() const {
        rwlock_read_lock(m_lock);
        U32 result = m_capacity;
        rwlock_unlock(m_lock);
        return result;
    }
    static constexpr U32 max_size() { return capacity_for_segment_count(kMaxSegments); }

    allocator_type* get_allocator() const { return m_allocator; }

   private:
    static constexpr U32 kSmallSegmentsToSkip = 6;
    static constexpr U32 kMaxSegments         = 26;
    static constexpr U32 kNotInFreelist       = UINT32_MAX;
    static constexpr U32 kEndOfList           = kNotInFreelist - 1;
    struct Entry {
        T*       storage_ptr() { return reinterpret_cast<T*>(storage); }
        const T* storage_ptr() const { return reinterpret_cast<const T*>(storage); }
        T*       value() { return std::launder(storage_ptr()); }
        const T* value() const { return std::launder(storage_ptr()); }

        alignas(T) U8 storage[sizeof(T)];
        U32 next;
        U32 generation;
    };

    struct DecomposedHandle {
        U32 idx;
        U32 generation;
    };

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

    static U32 next_generation(U32 generation) {
        ++generation;
        if (generation == 0) { generation = 1; }
        return generation;
    }

    static handle_type create_handle(U32 idx, U32 generation) {
        U64 value = (static_cast<U64>(generation) << 32) | idx;
        return handle_type(value);
    }

    static DecomposedHandle decompose_handle(handle_type handle) {
        return {
            .idx        = static_cast<U32>(handle.m_value & 0xFFFF'FFFF),
            .generation = static_cast<U32>(handle.m_value >> 32),
        };
    }

    void reserve_storage() {
        m_aligned_reservation_size =
            AlignPow2(static_cast<U64>(max_size()) * sizeof(Entry), ETide::page_size());
        m_base = m_allocator->reserve(m_aligned_reservation_size);
        if (m_base == 0) { throw std::bad_alloc(); }
    }

    void release_storage() {
        clear();
        if (m_base != 0) {
            m_allocator->release(m_base, m_aligned_reservation_size);
            m_base                     = 0;
            m_aligned_reservation_size = 0;
        }
    }

    B32 add_segment() {
        U32 segment_size = slots_in_segment(m_used_segments);
        U64 byte_size    = static_cast<U64>(segment_size) * sizeof(Entry);
        U64 byte_offset =
            static_cast<U64>(capacity_for_segment_count(m_used_segments)) * sizeof(Entry);
        Entry* segment = reinterpret_cast<Entry*>(static_cast<U8*>(m_base) + byte_offset);

        U64   page_size              = ETide::page_size();
        U64   segment_address        = reinterpret_cast<U64>(segment);
        U64   aligned_commit_address = AlignDownPow2(segment_address, page_size);
        void* aligned_commit_ptr     = reinterpret_cast<void*>(aligned_commit_address);
        U64   aligned_commit_size =
            AlignPow2(byte_size + segment_address - aligned_commit_address, page_size);

        B32 commit_succeeded = m_allocator->commit(aligned_commit_ptr, aligned_commit_size);
        if (!commit_succeeded) { return 0; }

        U32 segment_offset            = m_capacity;
        m_segments[m_used_segments++] = segment;
        m_capacity += segment_size;

        for (U32 slot = segment_size; slot > 0; --slot) {
            U32    idx        = segment_offset + slot - 1;
            Entry* entry      = &segment[slot - 1];
            entry->generation = 0;
            entry->next       = m_head;
            m_head            = idx;
        }
        return 1;
    }

    Entry* get(U32 idx) {
        U32 segment = segment_for_index(idx);
        U32 slot    = idx - capacity_for_segment_count(segment);
        return &m_segments[segment][slot];
    }

    const Entry* get(U32 idx) const {
        U32 segment = segment_for_index(idx);
        U32 slot    = idx - capacity_for_segment_count(segment);
        return &m_segments[segment][slot];
    }

    Entry* get_if_valid(DecomposedHandle handle) {
        if ((handle.idx >= m_capacity) || (handle.generation == 0)) { return 0; }

        Entry* entry = get(handle.idx);
        if (entry->next != kNotInFreelist || entry->generation != handle.generation) { return 0; }
        return entry;
    }

    const Entry* get_if_valid(DecomposedHandle handle) const {
        if ((handle.idx >= m_capacity) || (handle.generation == 0)) { return 0; }

        const Entry* entry = get(handle.idx);
        if (entry->next != kNotInFreelist || entry->generation != handle.generation) { return 0; }
        return entry;
    }

    mutable RWLock*    m_lock                     = 0;
    Memory::Allocator* m_allocator                = &Memory::default_allocator;
    void*              m_base                     = 0;
    U64                m_aligned_reservation_size = 0;
    U32                m_capacity                 = 0;
    U32                m_used_segments            = 0;
    U32                m_count                    = 0;
    U32                m_head                     = kEndOfList;
    U32                m_latest_generation        = 0;
    Entry*             m_segments[kMaxSegments]   = {0};
};

}  // namespace ETide::Containers

#endif
