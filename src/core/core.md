# Core

`core.hpp` and `core.cpp` provide ETide's shared types, platform abstractions, virtual
memory allocator, arenas, strings, intrusive-list helpers, and stable-address
containers.

The module follows the project's incremental-build model: `program.cpp` includes the
header and implementation into one translation unit.

## Quick start

```cpp
using namespace ETide;

Arena::Arena* arena = Arena::allocate({});

String::String8 message = String::str8_copy(arena, String::str8_mut(String::str8_literal("Hello ETide")));
SDL_Log("%.*s", static_cast<int>(message.size), message.str);

Arena::release(arena);
```

## Fundamental types

The flat `ETide` namespace defines fixed-width aliases:

| Type | Meaning |
| --- | --- |
| `U8`, `U16`, `U32`, `U64` | Unsigned integers. |
| `I8`, `I16`, `I32`, `I64` | Signed integers. |
| `B8`, `B16`, `B32`, `B64` | Integer boolean/storage types. |
| `F32`, `F64` | 32-bit and 64-bit floating point. |

`B32` functions return zero for false or failure and nonzero for true or success.

The linkage helpers are:

```cpp
internal      // Translation-unit-local function or variable.
global        // Translation-unit-local global.
local_persist // Function-local persistent storage.
```

## Size and numeric macros

### Byte sizes

```cpp
U64 cache_size = KB(64);
U64 arena_size = MB(256);
U64 file_limit = GB(4);
U64 huge_limit = TB(1);
```

`KB`, `MB`, `GB`, and `TB` use powers of 1024.

`Thousand`, `Million`, and `Billion` use decimal multipliers.

### Bit composition and extraction

```cpp
U64 combined = Compose64Bit(high_32, low_32);
U32 packed   = Compose32Bit(high_16, low_16);

U32 bit      = ExtractBit(value, 3);
U32 byte     = Extract8(value, 1);
U32 half     = Extract16(value, 1);
U64 word     = Extract32(value, 1);
```

Positions are counted from the least-significant side.

### Power-of-two alignment

```cpp
U64 aligned_up   = AlignPow2(value, alignment);
U64 aligned_down = AlignDownPow2(value, alignment);
U64 padding      = AlignPadPow2(value, alignment);

B32 valid = IsPow2(alignment);
```

Alignment values must be powers of two. `IsPow2OrZero` accepts either a power of two or
zero.

## Intrusive-list macros

The list macros operate on links stored directly in user nodes. They allocate no
memory.

```cpp
struct Job {
    Job* next;
    Job* prev;
    U32  id;
};

Job* first = 0;
Job* last  = 0;
Job  job   = {};

DLLPushBack(first, last, &job);
DLLRemove(first, last, &job);
```

### Doubly linked lists

| Macro | Operation |
| --- | --- |
| `DLLInsert` | Insert after a node, or at the front when the position is nil. |
| `DLLPushBack` | Append a node. |
| `DLLPushFront` | Prepend a node. |
| `DLLRemove` | Unlink a node. |

The `_NP` variants accept explicit next/previous field names. The `_NPZ` variants also
accept a custom nil sentinel.

### Singly linked queues and stacks

| Macro | Operation |
| --- | --- |
| `SLLQueuePush` | Append to a queue. |
| `SLLQueuePushFront` | Prepend to a queue. |
| `SLLQueuePop` | Remove the first queue node. |
| `SLLStackPush` | Push onto a stack. |
| `SLLStackPop` | Remove the top stack node. |

The `_N` variants accept an explicit link field. The `_NZ` variants also accept a custom
nil sentinel.

Pass simple variables and pointers to these macros. Do not pass expressions with side
effects because macro arguments may be evaluated more than once.

## Date and time

### `DateTime`

```cpp
struct DateTime {
    U16     micro_sec;
    U16     msec;
    U16     sec;
    U16     min;
    U16     hour;
    U16     day;
    WeekDay week_day;
    Month   month;
    U32     year;
};
```

Months and days use zero-based ETide representation:

- `month` ranges from `Month_Jan` through `Month_Dec`.
- `day` ranges from zero through 30.
- `week_day` ranges from `WeekDay_Sun` through `WeekDay_Sat`.
- `micro_sec` and `msec` each range from zero through 999.

The `wday` and `mon` union fields expose the weekday and month as `U32`.

### Dense time

```cpp
DenseTime dense_time_from_date_time(DateTime date_time);
DateTime  date_time_from_dense_time(DenseTime time);
```

`DenseTime` packs the calendar fields using fixed mixed-radix units: 12 months, 31 days,
24 hours, 60 minutes, 61 seconds, and 1000 milliseconds.

It is useful as a sortable internal representation. It is not Unix time and does not
model real month lengths.

### Other conversions

```cpp
DateTime date_time_from_micro_seconds(U64 time);
DateTime date_time_from_unix_time(U64 unix_seconds);
```

`date_time_from_micro_seconds` decomposes an elapsed value using the same fixed
31-day/12-month representation.

`date_time_from_unix_time` uses SDL to produce a real UTC calendar date.

### Current time and local conversion

```cpp
U64      now_time_us(void);
DateTime now_time_universal(void);
DateTime universal_from_local_time(DateTime* local);
DateTime local_from_universal_time(DateTime* universal);
void     sleep_ms(U32 milliseconds);
```

```cpp
DateTime utc   = now_time_universal();
DateTime local = local_from_universal_time(&utc);

sleep_ms(16);
```

Conversion failures are reported through SDL's error state and return a zero-initialized
`DateTime`.

## Bit scanning

```cpp
U64 ctz32(U32 value);
U64 ctz64(U64 value);
U64 clz32(U32 value);
U64 clz64(U64 value);
```

- `ctz` counts trailing zero bits.
- `clz` counts leading zero bits.
- Passing zero returns the bit width: 32 or 64.

The implementation uses compiler/platform intrinsics when available and a portable
fallback otherwise.

```cpp
U64 first_set_bit = ctz64(mask);
U64 highest_bit   = 63 - clz64(mask);
```

Check for zero before calculating `highest_bit` with subtraction.

## Mutexes and read/write locks

Synchronization handles and functions live under `ETide::Thread`. See
[`thread/thread.md`](../thread/thread.md) for mutexes, read/write locks, semaphores,
condition variables, and barriers.

## System page size

```cpp
U64 page_size();
```

Returns SDL's system page size. Virtual-memory callers use it when commit/decommit
ranges require page alignment.

## Virtual-memory allocators

### `Memory::Allocator`

```cpp
class Allocator {
    virtual void* reserve(U64 size) = 0;
    virtual B32   commit(void* ptr, U64 size) = 0;
    virtual void  decommit(void* ptr, U64 size) = 0;
    virtual void  release(void* ptr, U64 size) = 0;
};
```

The allocator interface separates address-space reservation from physical-page
commitment:

- `reserve` reserves a virtual address range.
- `commit` makes a reserved subrange readable and writable.
- `decommit` discards and protects committed pages while preserving the reservation.
- `release` returns the full reservation.

Commit and decommit callers are responsible for page-aligned addresses and sizes.

### `Memory::DefaultAllocator`

`default_allocator` implements the interface with `VirtualAlloc` on Windows and
`mmap`/`mprotect` on supported Unix-like platforms.

```cpp
void* memory = Memory::default_allocator.reserve(MB(16));
if (memory != 0) {
    if (Memory::default_allocator.commit(memory, page_size())) {
        // Use the first committed page.
    }
    Memory::default_allocator.release(memory, MB(16));
}
```

Normally, application code should use an arena or container instead of calling the
virtual-memory allocator directly.

Custom allocators must preserve the same reserve/commit/decommit/release contract.

## Related modules

The APIs extracted from Core have their own focused guides:

- [Arena](../arena/arena.md) covers persistent, temporary, and thread-local scratch
  allocation.
- [Thread](../thread/thread.md) covers SDL-backed synchronization handles.
- [String](../string/string.md) covers `String::String8`, arena allocation, lists, and
  serialization.
- [Containers](../containers/containers.md) covers `Containers::DynamicArray`,
  `Containers::Handle`, and `Containers::Pool`.

## Failure behavior

- Arena and string allocation functions return `0` or an empty result on allocation
  failure.
- Allocator `commit` returns zero on failure.
- `DynamicArray` and `Pool` constructors throw `std::invalid_argument` for a null custom
  allocator.
- Container reservation or commitment failures throw `std::bad_alloc`.
- Container maximum-size failures throw `std::length_error`.
- Checked access with an invalid index or handle throws `std::out_of_range`.
- SDL-backed platform functions report additional detail through `SDL_GetError()`.
