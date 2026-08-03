# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ETide is a from-scratch C++20 text editor built on SDL3 (SDL_GPU backend) + Dear ImGui, with a
custom base layer: virtual-memory allocators, arenas, strings, containers, and a persistent
piece-tree text buffer. It deliberately avoids the STL for storage and ownership.

## Build and run

```bash
cmake --preset default                      # configure once (Ninja Multi-Config -> build/)
cmake --build build --config Debug          # or RelWithDebInfo / Release
./build/bin/Debug/ETide
```

There is no test suite, no lint target, and no install step. `build/` is gitignored but present
locally; `build/compile_commands.json` is generated for tooling.

Formatting: `clang-format` with the root `.clang-format` (Google base, 4-space indent, left
pointers, heavy consecutive-alignment). Never reformat anything under `libs/`.

**`.clang-format` sets `SortIncludes: CaseSensitive`, which will reorder `program.cpp`'s module
includes alphabetically and break the build** (see below — that order is load-bearing). The include
region is fenced with `// clang-format off` / `// clang-format on`; keep any new module includes
inside that fence.

## Unity build — the most important structural fact

`src/program.cpp` is the **only** translation unit. It `#include`s every module's `.hpp` and then
every module's `.cpp` (see `src/program.cpp:27-42`). `CMakeLists.txt` lists only `src/program.cpp`.

Consequences:

- **Adding a module**: create `src/<name>/<name>.hpp`, `<name>.cpp`, `<name>.md`, then add both
  includes to `program.cpp` in dependency order. Do **not** touch `CMakeLists.txt`.
- **Headers are not self-contained.** They contain no `#include` directives and rely on
  `program.cpp` having already pulled in SDL, ImGui, and the STL headers, plus every preceding
  module header. The order is fixed: `core` → `thread` → `arena` → `string` → `containers` →
  `piece_tree` → `ui`. A header that compiles standalone in your editor is not the contract.
- `internal` (= `static`) applies to the whole program, not to a file. Everything is one TU, so
  there are no link-order or ODR concerns between modules — only include-order ones.

Vendored dependencies are split across two directories, both checked in directly (not submodules),
each aggregating into one INTERFACE target consumed by the root `CMakeLists.txt`:

- **`libs/` → `ETide::Libraries`** — third-party projects with their own build systems: SDL, Dear
  ImGui 1.92.9 (docking enabled), and the tree-sitter *runtime*. Each gets its own
  `add_subdirectory`.
- **`languages/` → `ETide::Languages`** — vendored tree-sitter *grammars*: C, C++, JSON, Odin,
  Rust. These are not projects, just generated output (`src/parser.c` parse tables, optional
  `src/scanner.c` external scanner, `src/tree_sitter/` headers), so a single
  `languages/CMakeLists.txt` builds them all through one `etide_add_language()` function rather
  than five identical child files.

**Adding a grammar**: drop its generated `src/` tree in `languages/<name>/`, add one
`etide_add_language(<name>)` line, declare its `tree_sitter_<name>()` entry point in
`program.cpp`, and add a row to the `languages[]` table. Check `LANGUAGE_VERSION` in its
`parser.c` is within the runtime's supported ABI range (currently 13–15, see
`libs/tree-sitter/lib/include/tree_sitter/api.h`). The grammar's `src/` include directory is
PRIVATE so its bundled `tree_sitter/` headers don't collide with the runtime's.

Grammar `parser.c` files are large (C++ is 25.9 MB, Odin 14.8 MB); they are compiled with warnings
suppressed since their contents aren't ours to fix.

## Base-layer conventions (`src/core/core.hpp`)

- Fixed-width aliases `U8..U64`, `I8..I64`, `B8..B64`, `F32/F64` inside `namespace ETide`. `bool`
  and `int` are avoided in module APIs.
- `B32` return means **0 = false/failure, nonzero = true/success**. Pointer-returning allocation
  functions return `0` on failure. Only the `Containers` classes throw (`std::bad_alloc`,
  `std::out_of_range`, `std::length_error`); the C-style modules never do.
- `internal` / `global` / `local_persist` are the `static` spellings; use them, not bare `static`.
- Size macros `KB/MB/GB/TB`, alignment macros `AlignPow2` etc., and intrusive list macros
  (`DLLPushBack`, `SLLQueuePush`, `SLLStackPush`, and their `_N`/`_NP`/`_NPZ` variants) are the
  house idiom for linked structures — nodes carry their own `next`/`prev`. Macro arguments may be
  evaluated more than once, so pass plain variables.

## Memory model

Three layers, each built on the one below:

1. `Memory::Allocator` (`core.hpp`) — virtual-memory interface: `reserve` / `commit` / `decommit` /
   `release`. `Memory::default_allocator` wraps `VirtualAlloc` or `mmap`/`mprotect`.
2. `ETide::Arena` — chained bump allocators. `Arena::allocate({...})` with `Params{flags,
   reserve_size, commit_size, allocator}`, then `Arena::push` / `push_array<T>` / `pop_to` /
   `release`. `ArenaFlags_NoChain` is required when callers depend on one contiguous reservation
   (e.g. the ImGui text buffer in `program.cpp` that grows in place).
   `Arena::ScratchBegin(conflicts, count)` / `ScratchEnd` give thread-local temporary arenas; pass
   the caller's arena as a conflict when the temporary work may alias it.
   Arenas never run constructors or destructors — pushed types must be trivially destructible.
3. `Containers::DynamicArray<T>` / `Pool<T>` + `Handle<T>` — segmented, address-stable storage
   (64-element first segment, doubling). They reserve their whole address range up front and commit
   per segment; `clear` decommits but keeps the reservation. `Pool` is internally RWLock-guarded.

Ownership is explicit and manual: whoever allocates an arena releases it, and objects document
which arenas they take over (e.g. `tree_builder_finish` hands the storage arena to the `Tree`; call
`PieceTree::release_tree`, never release that arena yourself).

## Module map

Each module owns a `.md` next to its source that documents the full API with examples — read the
`.md` before working in a module, and update it when the API changes.

| Module | Contents |
| --- | --- |
| `src/core` | Base types, macros, date/time, bit scanning, virtual-memory allocators. |
| `src/thread` | SDL-backed mutex, rwlock, semaphore, condition variable, barrier (opaque handles). |
| `src/arena` | Arena, `Temp`/`Scratch`, thread-local scratch state. |
| `src/string` | `String8` / `String8View`, arena-allocated strings, lists, serialization. |
| `src/containers` | `DynamicArray`, `Handle`, `Pool`. |
| `src/piece_tree` | The document buffer. |
| `src/ui` | SDL window + SDL_GPU device + ImGui context, frame lifecycle. |

## PieceTree

The editor's document representation: a **persistent** (immutable, structurally shared) red-black
tree of pieces pointing into immutable original buffers plus one append-only modification buffer.
Edits never move character data; they rewrite pieces and produce a new root, which is what makes
undo/redo and snapshots cheap. RB nodes are refcounted (`take_node_ref` / `dec_node_ref`) and
recycled through an `RBTreeBlock` free list.

- Construct with `tree_builder_start` → `tree_builder_accept` (once per chunk) → `tree_builder_finish`;
  release with `release_tree`.
- Strong enum-class types (`CharOffset`, `Length`, `Line`, `Column`, `LFCount`, `BufferIndex`) make
  offset/length/line mixups a compile error. Unwrap with `rep(value)`; combine with `distance`,
  `extend`, `retract`, and the provided operators.
- `OwningSnapshot` (owns its buffers, outlives further edits) vs `ReferenceSnapshot` (cheap, valid
  only while the tree's buffers live). `TreeWalker` / `ReverseTreeWalker` iterate characters and
  need a scratch arena for their stack.
- Read APIs that produce text (`get_line_content`, `assemble_range`) take the destination arena as
  their first argument — usually a scratch arena.

## Application shell

`program.cpp` uses SDL3's callback entry points (`SDL_MAIN_USE_CALLBACKS`): `SDL_AppInit` /
`SDL_AppEvent` / `SDL_AppIterate` / `SDL_AppQuit`, with app state carried in `appstate` and
allocated from an arena. `UI::init` / `process_event` / `begin_frame` / `end_frame` / `shutdown`
wrap the whole SDL+ImGui lifecycle; `UI::end_frame` returns `B32` and a failure should propagate as
`SDL_APP_FAILURE`.

`program.cpp` currently holds a single-code-buffer reference example wiring the whole stack
together, in four stages:

1. `find_source_file` — `std::filesystem` walks up from the working directory to the repo root and
   opens one of ETide's own sources (or `argv[1]`, if given). `language_from_path` then maps the
   file extension to a grammar via the `languages[]` table; an unmapped extension still opens and
   edits, just uncolored (`parser == 0` skips the parse).
2. `load_document` — `SDL_LoadFile` reads the bytes; `tree_builder_accept` copies them into the
   PieceTree's immutable buffer. The PieceTree is the only source of truth for text.
3. `render_buffer_rebuild` — flattens the tree through a `TreeWalker` into one arena-allocated
   `String8`, reparses it with tree-sitter, and derives `RenderSpan`s (colored tokens) and
   `RenderLine`s. This is the "dummy" render buffer; it is rebuilt wholesale on every edit and is
   the *only* thing the renderer reads.
4. `draw_code_buffer` — draws visible lines with `ImDrawList::AddText` per span, positioning glyphs
   by fixed advance (the default font is monospace). Input goes straight back to the PieceTree via
   `editor_insert` / `editor_remove`; Ctrl+Z / Ctrl+Y use the tree's persistent roots.

Highlighting uses one classifier for every grammar (`atomic_rules` / `leaf_rules` in
`program.cpp`). tree-sitter node type names are per-grammar with no cross-language standard, so
those tables list the names each vendored grammar actually emits; an unknown name falls back to
`TokenKind_Text` rather than failing. Anonymous leaf nodes carry their literal text as their type
name, which is how keywords and punctuation are classified without per-language keyword lists.

Note that ERROR nodes are normal, not a bug: tree-sitter does not expand macros, so macro-heavy
C/C++ (including ETide's own headers, where `internal` and `global` sit where declaration
specifiers belong) parses with errors under both the C and C++ grammars. Error recovery keeps the
rest of the tree intact, so coloring still works.

Deliberate simplifications, if you extend it: full reparse per keystroke rather than `ts_tree_edit`
+ incremental parse; printable ASCII only (offsets are bytes throughout, so UTF-8 needs multi-byte
handling); no selection, no save.
