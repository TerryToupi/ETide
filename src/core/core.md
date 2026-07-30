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

String8 message = str8_copy(arena, str8_mut(str8_literal("Hello ETide")));
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

`Mutex` and `RWLock` are opaque C-style handles backed by SDL.

### Mutex

```cpp
Mutex* mutex_create();
void   mutex_destroy(Mutex* mutex);
void   mutex_lock(Mutex* mutex);
B32    mutex_try_lock(Mutex* mutex);
void   mutex_unlock(Mutex* mutex);
```

```cpp
Mutex* mutex = mutex_create();

mutex_lock(mutex);
update_shared_state();
mutex_unlock(mutex);

mutex_destroy(mutex);
```

`mutex_try_lock` returns nonzero only when the lock was acquired. Unlock it only after a
successful acquisition.

### Read/write lock

```cpp
RWLock* rwlock_create();
void    rwlock_destroy(RWLock* lock);
void    rwlock_read_lock(RWLock* lock);
void    rwlock_write_lock(RWLock* lock);
B32     rwlock_try_read_lock(RWLock* lock);
B32     rwlock_try_write_lock(RWLock* lock);
void    rwlock_unlock(RWLock* lock);
```

Use a read lock for shared reads and a write lock for mutations:

```cpp
rwlock_read_lock(lock);
read_shared_state();
rwlock_unlock(lock);
```

The same `rwlock_unlock` function releases either lock mode.

No guard objects are provided. Every successful lock operation must have an explicit
unlock.

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

## Arenas

An arena reserves a large virtual range, commits pages as needed, and serves aligned
linear allocations. Individual allocations are not freed; memory is reclaimed by
popping, clearing, or releasing the arena.

### Creating an arena

```cpp
Arena::Arena* arena = Arena::allocate({});
```

`Arena::Params` controls creation:

```cpp
Arena::Params params = {
    .flags        = Arena::ArenaFlags_None,
    .reserve_size = MB(256),
    .commit_size  = KB(64),
    .allocator    = &Memory::default_allocator,
};

Arena::Arena* arena = Arena::allocate(params);
```

Defaults reserve 64 MiB and commit in 64 KiB increments.

`ArenaFlags_NoChain` prevents the arena from allocating another block after its
reservation is exhausted. Use it when storage must remain contiguous.

### Raw allocation

```cpp
void* push(Arena* arena, U64 size, U64 alignment, B32 zero);
```

```cpp
Widget* widget = static_cast<Widget*>(
    Arena::push(arena, sizeof(Widget), alignof(Widget), 1));
```

The alignment must be a power of two. A nonzero `zero` argument requests
zero-initialized storage.

Pointers remain stable until their allocation is popped or the arena is released.

If the current block cannot fit an allocation, a chainable arena allocates a new block.
Allocations larger than the normal block reservation receive a sufficiently large
block.

### Typed arrays

```cpp
T* push_array(Arena* arena, U64 count);
T* push_array_no_zero(Arena* arena, U64 count);
T* push_array_aligned(Arena* arena, U64 count, U64 alignment);
T* push_array_no_zero_aligned(Arena* arena, U64 count, U64 alignment);
```

```cpp
U32* counters = Arena::push_array<U32>(arena, 128);
Vertex* vertices =
    Arena::push_array_no_zero_aligned<Vertex>(arena, 4096, alignof(Vertex));
```

Typed arena allocations require trivially destructible types. `push_array` variants
zero memory; `push_array_no_zero` variants leave existing committed memory
uninitialized.

### Positions and popping

```cpp
U64  pos(Arena* arena);
void pop_to(Arena* arena, U64 position);
void pop(Arena* arena, U64 amount);
void clear(Arena* arena);
```

```cpp
U64 checkpoint = Arena::pos(arena);
temporary_work(arena);
Arena::pop_to(arena, checkpoint);
```

`pop_to` releases chained blocks above the requested position. `clear` returns to the
initial arena position while keeping the first reservation available.

### Temporary scopes

```cpp
Arena::Temp temp = Arena::begin(arena);
temporary_work(arena);
Arena::end(temp);
```

`end` restores the exact position captured by `begin`.

### Thread-local scratch arenas

Each thread has two lazily created scratch arenas.

```cpp
Arena::Scratch scratch = Arena::ScratchBegin(0, 0);
temporary_work(scratch.arena);
Arena::ScratchEnd(scratch);
```

Conflicting arenas can be excluded:

```cpp
Arena::Arena* conflicts[] = {persistent_arena};
Arena::Scratch scratch =
    Arena::ScratchBegin(conflicts, 1);
```

`ScratchBegin` returns the first thread-local arena not present in the conflict list.
`ScratchEnd` restores its previous position so it can be reused.

The thread state releases its scratch arenas when the thread exits.

### Releasing an arena

```cpp
Arena::release(arena);
```

Releases the first reservation and all chained blocks. All pointers into the arena
become invalid.

## String8

`String8` is a non-owning pointer-and-length byte span:

```cpp
struct String8 {
    char* str;
    U64   size;
};
```

Its lifetime is controlled by the memory containing `str`. Operations use `size`, so
embedded null bytes do not truncate the string.

`String8View` is the corresponding read-only span.

### Constructing views

```cpp
char mutable_text[] = "hello";

String8 a = str8(mutable_text, 5);
String8 b = str8_cstr(mutable_text);
String8View literal = str8_literal("hello");
String8 c = str8_mut(literal);
```

Important array behavior:

- `str8(array)` uses the full array size, including a trailing null when present.
- `str8_literal("text")` excludes the trailing null.
- `str8_cstr` scans until the first null.
- `str8_empty` is a zero pointer and zero length.

### Arena allocation and copying

```cpp
String8 raw  = str8_alloc(arena, 64);
String8 cstr = str8_cstr_alloc(arena, 64);
String8 copy = str8_copy(arena, source);
```

- `str8_alloc` allocates exactly `size` bytes.
- `str8_cstr_alloc` allocates `size + 1` bytes and writes a trailing null. The returned
  logical size remains `size`.
- `str8_copy` copies the explicit byte range and adds a trailing null.

### Exact comparison

```cpp
if (str8_match_exact(a, b)) {
    // Equal size and equal bytes.
}
```

### Intrusive string lists

`String8List` stores spans in `String8Node` objects without copying their bytes.

```cpp
String8List list = {};
str8_list_push(arena, &list, first);
str8_list_push(arena, &list, second);

String8 joined = str8_list_join(arena, &list);
```

`str8_list_push_node` links a caller-provided node.

`str8_list_push_node_set_string` assigns its string and links it.

`str8_list_push` allocates the node from an arena.

`str8_list_join` allocates one null-terminated result and copies every span in order.

### Incremental serialization

```cpp
String8List output = {};
str8_serial_begin(arena, &output);
str8_serial_push_str8(arena, &output, prefix);
str8_serial_push_char(arena, &output, ':');
str8_serial_push_str8(arena, &output, value);

String8 result = str8_serial_end(arena, output);
```

Serialization collects spans first and performs one final join. The returned string is
null-terminated and owned by the supplied arena.

## `Containers::DynamicArray`

`DynamicArray<T>` is a segmented array that preserves element pointers as it grows.

It reserves enough virtual address space for `max_size()` during construction and
commits geometrically growing segments as needed. The first segment holds 64 elements.

`T` must be trivially destructible.

```cpp
struct Item {
    U64 id;
    F32 weight;
};

Containers::DynamicArray<Item> items;
Item& item = items.emplace_back(Item{.id = 7, .weight = 1.0f});
Item* stable_pointer = &item;

for (U32 idx = 0; idx < 10000; ++idx) {
    items.push_back({.id = idx});
}

// Still valid after growth.
SDL_Log("%llu", static_cast<unsigned long long>(stable_pointer->id));
```

### Construction and ownership

```cpp
DynamicArray();
explicit DynamicArray(Memory::Allocator* allocator);
```

The container owns its reservation. It is moveable but not copyable.

Every container allocation goes through the supplied ETide allocator.

### Element operations

```cpp
T& emplace_back(arguments...);
T& push_back(const T& value);
T& push_back(T&& value);
```

The container has no removal operation. `clear` decommits all active segments and
resets size and capacity to zero while keeping the reservation.

Because only trivially destructible types are accepted, `clear` does not run element
destructors.

### Access and metadata

```cpp
array[index];       // Unchecked.
array.at(index);    // Throws std::out_of_range.
array.front();
array.back();

array.empty();
array.size();
array.capacity();
array.max_size();
array.get_allocator();
```

`front` and `back` require a non-empty array.

## `Containers::Handle`

`Handle<T>` is the generation-checked identifier returned by `Pool<T>`.

```cpp
Containers::Handle<Entity> handle;

if (handle) {
    U64 encoded = handle.value();
}
```

A zero value is invalid. Equality compares the complete encoded value. The upper
32 bits contain the generation and the lower 32 bits contain the slot index.

There is no separate valid bit; a nonzero generation distinguishes live handles from
default and stale values.

## `Containers::Pool`

`Pool<T>` combines segmented stable-address storage with an intrusive free list and
generation-checked handles.

It is useful for entities, resources, jobs, or other objects that are frequently
created, looked up, and erased.

`T` must be trivially destructible.

```cpp
struct Entity {
    U64 id;
    F32 x;
    F32 y;
};

Containers::Pool<Entity> entities;
Containers::Handle<Entity> player =
    entities.emplace(Entity{.id = 1});

entities[player].x = 42.0f;

if (entities.contains(player)) {
    entities.erase(player);
}
```

### Construction and allocation

```cpp
Pool();
explicit Pool(Memory::Allocator* allocator);
```

The pool reserves its maximum virtual range at construction and commits segments when
the free list is exhausted. The first segment contains 64 slots.

The pool is neither copyable nor moveable.

### Creating and erasing objects

```cpp
Handle<T> emplace(arguments...);
void      erase(Handle<T> handle);
void      clear();
```

`emplace` returns a new generation-checked handle.

`erase` invalidates that handle and returns its slot to the free list. Passing an
invalid or stale handle throws `std::out_of_range`.

`clear` decommits every segment and invalidates all handles.

Trivially destructible objects do not receive destructor calls during erase or clear.

### Lookup

```cpp
pool[handle];     // Throws when invalid or stale.
pool.at(handle);  // Same checked behavior.
pool.contains(handle);
```

Both indexing functions validate the index, generation, and free-list state.

### Metadata

```cpp
pool.empty();
pool.size();
pool.capacity();
pool.max_size();
pool.get_allocator();
```

### Concurrency

The pool uses an `RWLock`:

- `emplace`, `erase`, and `clear` take the write lock.
- Lookup, `contains`, and metadata queries take the read lock.

The lock protects lookup itself, but `operator[]` and `at` return a reference after
releasing the read lock. The caller must ensure another thread cannot erase that object
while the reference is in use.

Growth does not move existing objects, so pointers remain stable until their slot is
erased, the pool is cleared, or the pool is destroyed.

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
