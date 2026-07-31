# Containers

The `ETide::Containers` namespace provides containers built exclusively on
`Memory::Allocator`. Both containers reserve their maximum virtual address range up
front and commit storage one segment at a time.

Container elements must be trivially destructible. Existing element addresses remain
stable as storage grows.

## DynamicArray

`DynamicArray<T>` is a segmented, grow-only array with STL-like element access:

```cpp
Containers::DynamicArray<Item> items;

Item& first = items.emplace_back(Item{});
items.push_back(Item{});

Item& item = items[0];
U32 count  = items.size();
```

The first segment contains 64 elements. Each later segment doubles in size. Sequential
insertion caches the active segment position and does not perform a logarithm for every
new element.

### Construction and allocators

```cpp
Containers::DynamicArray<Item> defaults;
Containers::DynamicArray<Item> custom(&allocator);
```

Passing `0` as the allocator is invalid. Construction reserves the full virtual range;
physical pages are committed only when a segment is added.

### Access and capacity

```cpp
items.front();
items.back();
items[index];
items.at(index);

items.empty();
items.size();
items.capacity();
items.max_size();
items.get_allocator();
```

`operator[]` requires a valid index. `at` checks the index and throws
`std::out_of_range` when invalid.

### Clearing and moving

`clear` decommits committed element storage while retaining the virtual reservation.
The array may be reused afterward.

Dynamic arrays are movable but not copyable. Moving transfers the reservation and all
stable element addresses to the destination.

## Handle

`Handle<T>` is the opaque identifier used by `Pool<T>`:

```cpp
Containers::Handle<Entity> handle;

if (handle) {
    U64 raw_value = handle.value();
}
```

The upper 32 bits store a generation and the lower 32 bits store a slot index. A zero
generation is invalid. Reusing a slot assigns a new generation so stale handles do not
resolve to a later object.

## Pool

`Pool<T>` combines segmented stable-address storage with an intrusive free list:

```cpp
Containers::Pool<Entity> entities;

Containers::Handle<Entity> player =
    entities.emplace(Entity{});

if (entities.contains(player)) {
    Entity& entity = entities[player];
}

entities.erase(player);
```

Pool storage has the same 64-entry initial segment and doubling growth pattern as
`DynamicArray`.

### Thread safety

The pool owns a `Thread::RWLock`:

- Mutation and segment growth use the write lock.
- Lookup, validation, and metadata queries use the read lock.
- `contains`, `empty`, `size`, and `capacity` are safe concurrent observations.

The reference returned by `operator[]` or `at` outlives the internal read lock. The
caller must ensure another thread cannot erase that object while the reference is in
use.

### Pool operations

```cpp
Containers::Handle<T> handle = pool.emplace(arguments...);
pool.erase(handle);

T& value = pool[handle];
T& checked_value = pool.at(handle);

B32 exists = pool.contains(handle);
U32 count = pool.size();
U32 committed_capacity = pool.capacity();
```

Invalid or stale handles cause element access and erase to throw `std::out_of_range`.
`clear` invalidates every handle and decommits all segments while keeping the reserved
range.

Pool instances are neither copyable nor movable because their synchronization handle
and active references have stable ownership.

## Allocation behavior

- No container uses an owning standard container for storage.
- Maximum address space is reserved once during construction.
- Segments commit pages on demand.
- `clear` decommits pages without releasing the reservation.
- Destruction releases the full reservation.
- Allocation or commitment failure throws `std::bad_alloc`.
- Reaching the maximum segment count throws `std::length_error`.
