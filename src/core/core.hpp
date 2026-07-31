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

#endif
