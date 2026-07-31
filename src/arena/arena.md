# Arena

The Arena module provides linear allocation over reserved virtual memory. It lives in
the `ETide::Arena` namespace and uses only an `ETide::Memory::Allocator` for memory
operations.

An arena commits memory as allocations reach new pages. A chainable arena adds another
reservation when its current block is full. Existing pointers remain stable until the
allocation is popped or the arena is released.

## Creating and releasing an arena

```cpp
Arena::Arena* arena = Arena::allocate({});
if (arena == 0) {
    // Allocation failed.
}

Arena::release(arena);
```

Creation parameters control reservation, commitment, chaining, and the allocator:

```cpp
Arena::Params params = {
    .flags        = Arena::ArenaFlags_NoChain,
    .reserve_size = MB(256),
    .commit_size  = KB(64),
    .allocator    = &Memory::default_allocator,
};

Arena::Arena* arena = Arena::allocate(params);
```

`ArenaFlags_NoChain` makes exhaustion return `0` instead of creating another block.

## Allocating memory

`push` allocates raw memory with explicit alignment and initialization:

```cpp
void* memory = Arena::push(arena, byte_count, alignment, zero_memory);
```

The typed helpers accept only trivially destructible types:

```cpp
Widget* widgets = Arena::push_array<Widget>(arena, 64);
Vertex* vertices =
    Arena::push_array_no_zero_aligned<Vertex>(arena, 1024, alignof(Vertex));
```

- `push_array` and `push_array_aligned` zero the allocation.
- `push_array_no_zero` and `push_array_no_zero_aligned` leave it uninitialized.
- Every allocation returns `0` when reservation or commitment fails.

## Positions and temporary scopes

An arena position is a checkpoint across all chained blocks:

```cpp
U64 checkpoint = Arena::pos(arena);
do_temporary_work(arena);
Arena::pop_to(arena, checkpoint);
```

Other position operations are:

```cpp
Arena::pop(arena, byte_count);
Arena::clear(arena);
```

`clear` returns to the initial position and releases chained blocks. The first
reservation remains available.

Temporary scopes capture and restore a position:

```cpp
Arena::Temp temp = Arena::begin(arena);
do_temporary_work(arena);
Arena::end(temp);
```

## Thread-local scratch arenas

Each thread owns two lazily allocated scratch arenas:

```cpp
Arena::Scratch scratch = Arena::ScratchBegin(0, 0);
if (scratch.arena != 0) {
    do_temporary_work(scratch.arena);
}
Arena::ScratchEnd(scratch);
```

Pass conflicting arenas when temporary work may alias persistent input:

```cpp
Arena::Arena* conflicts[] = {input_arena};
Arena::Scratch scratch = Arena::ScratchBegin(conflicts, 1);
```

`ScratchBegin` selects the first non-conflicting arena. `ScratchEnd` restores its
position. The thread-local state releases both scratch arenas when the thread exits.

## Lifetime rules

- Do not access memory after popping past its allocation.
- Do not access any arena pointer after `release`.
- Do not call `release` for an individual chained block.
- Arena allocation does not run constructors or destructors.
- A no-chain arena is required when later code depends on one contiguous reservation.
