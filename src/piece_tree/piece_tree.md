# PieceTree

`ETide::PieceTree` is an arena-backed text buffer for editors and other workloads that
perform frequent insertions, removals, snapshots, and line-based queries.

The document is stored as a persistent red-black tree of pieces. A piece points into
either an immutable original buffer or the append-only modification buffer. Editing
changes pieces and tree nodes; it does not move existing character data.

## Quick start

```cpp
using namespace ETide;

Arena::Arena* storage = Arena::allocate({});
PieceTree::TreeBuilder builder = PieceTree::tree_builder_start(storage);

char first[]  = "Hello\n";
char second[] = "world";
PieceTree::tree_builder_accept(0, &builder, String::str8_cstr(first));
PieceTree::tree_builder_accept(0, &builder, String::str8_cstr(second));

PieceTree::Tree* tree = PieceTree::tree_builder_finish(&builder);

char inserted[] = "persistent ";
tree->insert(PieceTree::CharOffset{6}, String::str8_cstr(inserted));

Arena::Scratch scratch = Arena::ScratchBegin(0, 0);
String::String8 line = tree->get_line_content(scratch.arena, PieceTree::Line{1});
SDL_Log("%.*s", static_cast<int>(line.size), line.str); // persistent world
Arena::ScratchEnd(scratch);

PieceTree::release_tree(tree);
```

After `tree_builder_finish`, the tree owns the supplied storage arena. Release the tree
with `release_tree`; do not release that arena separately.

## Strong numeric types

PieceTree uses distinct numeric types to prevent accidental mixing of offsets, lengths,
lines, and columns.

| Type | Meaning |
| --- | --- |
| `CharOffset` | Zero-based character offset in the complete document. |
| `Length` | Character count or line count. |
| `Line` | Zero-based document or buffer line. |
| `Column` | Zero-based column within a line. |
| `LFCount` | Number of `\n` characters. |
| `BufferIndex` | Index of an immutable buffer; `ModBuf` identifies inserted text. |
| `LineStart` | Byte offset at which a buffer-relative line begins. |
| `Offset` | Alias for `CharOffset`. |

Use `rep` when the underlying `U64` value is needed:

```cpp
PieceTree::Length length = tree->length();
U64 raw_length = PieceTree::rep(length);

PieceTree::CharOffset end{raw_length};
PieceTree::CharOffset next = end + PieceTree::Length{4};
```

The helper operations are:

- `rep(value)` returns the underlying integer.
- `distance(first, last)` returns a `Length`.
- `extend(value, amount)` adds an integer amount while preserving the type.
- `retract(value, amount)` subtracts an integer amount while preserving the type.
- `CharOffset + Length`, `Length + Length`, and `Length - Length` preserve their
  appropriate strong type.

`CharOffset::Sentinel` is used internally when no meaningful insertion offset exists.

## Building and releasing trees

### `tree_builder_start`

```cpp
TreeBuilder tree_builder_start(Arena::Arena* buffer_arena);
```

Starts a builder and creates the long-lived arenas used by the document. The supplied
arena becomes the immutable-buffer and tree-node arena.

### `tree_builder_accept`

```cpp
void tree_builder_accept(
    Arena::Arena* scratch_arena,
    TreeBuilder* builder,
    String::String8 text);
```

Copies one initial buffer into immutable storage. Buffers appear in the document in the
order accepted. Empty buffers remain valid storage inputs but do not create tree pieces.

The current implementation does not require the optional `scratch_arena`, so `0` is
valid.

```cpp
char header[] = "one\n";
char body[]   = "two\nthree";

PieceTree::tree_builder_accept(0, &builder, String::str8_cstr(header));
PieceTree::tree_builder_accept(0, &builder, String::str8_cstr(body));
```

### `tree_builder_finish`

```cpp
Tree* tree_builder_finish(TreeBuilder* builder);
```

Finishes construction and returns the owning tree. The builder is consumed and cleared.

### `tree_builder_empty`

```cpp
Tree* tree_builder_empty(Arena::Arena* buffer_arena);
```

Creates an empty document without manually using a builder:

```cpp
Arena::Arena* storage = Arena::allocate({});
PieceTree::Tree* tree = PieceTree::tree_builder_empty(storage);
```

An empty document has length zero and one logical line.

### `release_tree`

```cpp
void release_tree(Tree* tree);
```

Releases history, tree nodes, text buffers, and all arenas owned by the tree. Snapshots
retain the storage they require.

## Editing

### Insertion

```cpp
void Tree::insert(
    CharOffset offset,
    String::String8 text,
    SuppressHistory history = SuppressHistory::No);
```

`offset` may be anywhere from zero through `length()`. Inserted bytes are appended to
the modification buffer, while the piece tree is changed to reference them.

```cpp
char prefix[] = "Start: ";
tree->insert(PieceTree::CharOffset{0}, String::str8_cstr(prefix));

char suffix[] = "\nDone";
tree->insert(
    PieceTree::CharOffset{PieceTree::rep(tree->length())},
    String::str8_cstr(suffix));
```

Inserting an empty string is a no-op. Sequential insertions at the previous insertion
endpoint can share an undo boundary and combine compatible modification pieces.

### Removal

```cpp
void Tree::remove(
    CharOffset offset,
    Length count,
    SuppressHistory history = SuppressHistory::No);
```

Removes `count` characters beginning at `offset`. The underlying character buffers
remain append-only; removal only changes piece references.

```cpp
tree->remove(PieceTree::CharOffset{5}, PieceTree::Length{3});
```

Removing zero characters, or removing from an empty document, is a no-op. The requested
range must be inside the document.

### Suppressing automatic history

Use `SuppressHistory::Yes` when a caller manages edit grouping explicitly:

```cpp
tree->commit_head(PieceTree::CharOffset{10});

tree->insert(
    PieceTree::CharOffset{10},
    String::str8_cstr(inserted),
    PieceTree::SuppressHistory::Yes);
```

## Document metadata

```cpp
Length  Tree::length() const;
B32     Tree::is_empty() const;
LFCount Tree::line_feed_count() const;
Length  Tree::line_count() const;
```

`line_count()` is always `line_feed_count() + 1`, including for an empty document.

```cpp
SDL_Log("characters: %llu",
        static_cast<unsigned long long>(PieceTree::rep(tree->length())));
SDL_Log("lines: %llu",
        static_cast<unsigned long long>(PieceTree::rep(tree->line_count())));
```

## Character and line queries

### `at`

```cpp
char Tree::at(CharOffset offset) const;
```

Returns the character at an offset. The offset must be less than `length()`.

```cpp
char first = tree->at(PieceTree::CharOffset{0});
```

### `line_at`

```cpp
Line Tree::line_at(CharOffset offset) const;
```

Returns the zero-based line containing the offset. The document-end offset is valid.

```cpp
PieceTree::Line line = tree->line_at(PieceTree::CharOffset{24});
```

### Line ranges

All line ranges use document offsets and are half-open: `[first, last)`.

```cpp
LineRange Tree::get_line_range(Line line) const;
LineRange Tree::get_line_range_crlf(Line line) const;
LineRange Tree::get_line_range_with_newline(Line line) const;
```

- `get_line_range` excludes `\n` but retains a preceding `\r`.
- `get_line_range_crlf` excludes both bytes of a complete `\r\n`.
- `get_line_range_with_newline` includes the line ending when one exists.

```cpp
PieceTree::LineRange range =
    tree->get_line_range_crlf(PieceTree::Line{2});

PieceTree::Length length = PieceTree::distance(range.first, range.last);
```

### Assembling line content

```cpp
String::String8 Tree::get_line_content(Arena::Arena* arena, Line line) const;

IncompleteCRLF Tree::get_line_content_crlf(
    Arena::Arena* arena,
    String::String8* result,
    Line line) const;
```

Line content may span several pieces, so the result is assembled into the supplied
arena. It remains valid until that arena is popped or released.

```cpp
Arena::Scratch scratch = Arena::ScratchBegin(0, 0);

String::String8 content =
    tree->get_line_content(scratch.arena, PieceTree::Line{0});

SDL_Log("%.*s", static_cast<int>(content.size), content.str);
Arena::ScratchEnd(scratch);
```

The CRLF-aware function removes a valid `\r\n`. It returns
`IncompleteCRLF::Yes` when the line ends with `\n` without a preceding `\r`.

```cpp
String::String8 content = {};
PieceTree::IncompleteCRLF incomplete =
    tree->get_line_content_crlf(
        scratch.arena,
        &content,
        PieceTree::Line{0});
```

## Undo and redo

Edits record persistent roots, so saving or switching document states does not copy the
complete document.

```cpp
UndoRedoResult Tree::try_undo(CharOffset current_offset);
UndoRedoResult Tree::try_redo(CharOffset current_offset);
```

The supplied offset is the caller's current cursor or operation position. It is stored
for the inverse operation. A successful result contains the offset associated with the
restored state.

```cpp
PieceTree::UndoRedoResult undo =
    tree->try_undo(PieceTree::CharOffset{18});

if (undo.success) {
    move_cursor_to(PieceTree::rep(undo.op_offset));
}

PieceTree::UndoRedoResult redo =
    tree->try_redo(undo.op_offset);
```

An empty undo or redo stack returns `success == 0`.

Starting a new recorded edit clears the redo stack.

### Explicit history boundaries

```cpp
void Tree::commit_head(CharOffset offset);
```

Saves the current root and associated operation offset in undo history. This is useful
for grouping several edits performed with `SuppressHistory::Yes`.

## Persistent roots

### `head`

```cpp
RedBlackTree Tree::head() const;
```

Returns a retained root representing the current document state. Root duplication is
constant time.

### `snap_to`

```cpp
void Tree::snap_to(const RedBlackTree& root);
```

Switches the active document to a compatible retained root.

```cpp
PieceTree::RedBlackTree saved = tree->head();

tree->insert(PieceTree::CharOffset{0}, String::str8_cstr(inserted));

// Restore the saved branch.
tree->snap_to(saved);
```

A root passed to `snap_to` must originate from the same buffer collection.

`RedBlackTree` is move-only. Use `dup()` when another retained reference is required.
Its `left`, `right`, `root`, `root_color`, `tree_length`, and `tree_lf_count` operations
are primarily intended for tree implementation, diagnostics, and invariant checking.

## Snapshots

Both snapshot types preserve the root present when they are created. Later edits do not
change snapshot content.

They provide the same read-only APIs as `Tree`:

- `at`
- `line_at`
- `get_line_content`
- `get_line_content_crlf`
- `get_line_range`
- `get_line_range_crlf`
- `get_line_range_with_newline`
- `length`
- `line_count`
- `is_empty`

### Reference snapshots

```cpp
ReferenceSnapshot Tree::ref_snap() const;
```

A reference snapshot shares the persistent root and all underlying character storage.
It is cheap to create and copy. Its destructor releases its retained references.

```cpp
PieceTree::ReferenceSnapshot snapshot = tree->ref_snap();

tree->insert(PieceTree::CharOffset{0}, String::str8_cstr(inserted));

// Reads the state from before the insertion.
char original_first = snapshot.at(PieceTree::CharOffset{0});
```

### Owning snapshots

```cpp
OwningSnapshot* Tree::owning_snap(Arena::Arena* arena) const;
void release_owning_snap(OwningSnapshot* snapshot);
```

An owning snapshot retains immutable tree storage and copies the current modification
buffer. It remains usable after the source tree is released.

The snapshot object itself is placed in the caller-provided arena. Always call
`release_owning_snap` before popping that arena.

```cpp
Arena::Scratch scratch = Arena::ScratchBegin(0, 0);

PieceTree::OwningSnapshot* snapshot = tree->owning_snap(scratch.arena);
PieceTree::release_tree(tree);

String::String8 line =
    snapshot->get_line_content(scratch.arena, PieceTree::Line{0});

PieceTree::release_owning_snap(snapshot);
Arena::ScratchEnd(scratch);
```

## Forward walking

`TreeWalker` traverses characters without flattening the document. Its traversal stack
is allocated from the supplied arena, which must outlive the walker.

Walkers can read a `Tree`, `ReferenceSnapshot`, or `OwningSnapshot`.

```cpp
Arena::Scratch scratch = Arena::ScratchBegin(0, 0);

PieceTree::TreeWalker walker{
    scratch.arena,
    tree,
    PieceTree::CharOffset{0},
};

while (!walker.exhausted()) {
    char character = walker.next();
    consume(character);
}

Arena::ScratchEnd(scratch);
```

The walker API is:

```cpp
char       current();             // Read without advancing.
char       next();                // Read and advance.
void       seek(CharOffset offset);
B32        exhausted() const;
Length     remaining() const;
CharOffset offset() const;
```

Forward offsets may range from zero through the document length.

## Reverse walking

`ReverseTreeWalker` mirrors `TreeWalker`, moving toward offset zero.

For a non-empty document, begin at `length() - 1`:

```cpp
Arena::Scratch scratch = Arena::ScratchBegin(0, 0);

PieceTree::CharOffset last{
    PieceTree::rep(tree->length()) - 1,
};
PieceTree::ReverseTreeWalker walker{scratch.arena, tree, last};

while (!walker.exhausted()) {
    char character = walker.next();
    consume(character);
}

Arena::ScratchEnd(scratch);
```

Do not construct a reverse walker at `length()`; its starting offset must identify a
character.

## Selections

Selections are lightweight document ranges stored in an intrusive list.

```cpp
struct Selection {
    Offset         first;
    Offset         last;
    EmptySelection empty;
};
```

`EmptySelection::Yes` allows a cursor-like selection where no character range is
selected.

```cpp
PieceTree::SelectionList selections = {};
PieceTree::Selection selection = {
    .first = PieceTree::CharOffset{4},
    .last  = PieceTree::CharOffset{9},
    .empty = PieceTree::EmptySelection::No,
};

PieceTree::push_selection(scratch.arena, &selections, selection);
PieceTree::pop_selection(&selections);
```

`push_selection` allocates a node in the supplied arena. `pop_selection` unlinks the
last selection; arena memory is recovered when the arena is popped.

`SelectionMeta` associates an owning snapshot with a selection list:

```cpp
PieceTree::SelectionMeta meta = {
    .snap       = snapshot,
    .selections = selections,
};
```

## Storage model

`BufferCollection` coordinates four long-lived storage areas:

- Immutable buffer and persistent tree-node arena.
- Append-only modification-byte arena.
- Append-only modification line-start arena.
- Undo and redo entry arena.

`CharBuffer` combines a `String::String8` byte span with its `LineStarts` index.

`BufferIndex::ModBuf` selects the modification buffer. Any other `BufferIndex` selects
an entry in `orig_buffers`.

```cpp
PieceTree::BufferCollection buffers = tree->buffer_collection_no_ref();
PieceTree::CharBuffer* modified =
    buffers.buffer_at(PieceTree::BufferIndex::ModBuf);
```

`buffer_collection_no_ref` does not increment storage references. It is intended for
short-lived inspection and internal integration. Do not retain the returned collection
past the lifetime of its owner unless the required references are acquired explicitly.

The lower-level storage and node types are exposed for integration and diagnostics:

- `BufferCursor` identifies a buffer-relative line and column.
- `Piece` identifies a half-open range in one character buffer.
- `NodeData` stores a piece and cached left-subtree metadata.
- `RBNodeCounted` stores persistent tree links, color, metadata, and reference count.
- `RBTreeBlock` owns node allocation and the ABA-tagged atomic free list.
- `BufferMeta` stores active document length and LF count.
- `NodePosition` describes a piece lookup result.
- `UndoRedoEntry` and `UndoRedoList` form intrusive history stacks.

Normal editor code should use `Tree`, builders, snapshots, and walkers instead of
manipulating these structures directly.

### Low-level reference management

These functions support internal ownership transfers:

```cpp
RBNodeCounted* take_node_ref(RBNodeCounted* node);
void dec_node_ref(RBNodeCounted* node);

BufferCollection take_buffer_ref(const BufferCollection* buffers);
BufferCollection take_immutable_buffer_ref(const BufferCollection* buffers);
void dec_buffer_ref(BufferCollection* buffers);
```

- `take_node_ref` and `dec_node_ref` retain and release a persistent node.
- `take_buffer_ref` retains immutable and source modification storage.
- `take_immutable_buffer_ref` retains only immutable storage, as required by an owning
  snapshot that has copied its modification buffer.
- `dec_buffer_ref` releases the references held by a collection.

`BufferCollection::buffer_offset` converts a buffer-relative `BufferCursor` into an
offset in that character buffer.

The direct `RedBlackTree::insert` and `RedBlackTree::remove` operations work at piece
boundaries and require the matching `RBTreeBlock`. They are implementation-level
operations; document edits should use `Tree::insert` and `Tree::remove`.

The direct snapshot constructors allow a caller to pair a tree with a specific retained
root. The raw `TreeWalker` constructor similarly accepts a buffer collection, metadata,
and root. These entry points are useful for internal tree tooling but require the caller
to preserve all ownership relationships.

`Direction`, `StackEntry`, and `StackList` describe the arena-backed traversal stack
used by walkers.

## Ownership checklist

- Initial text is copied when accepted by a builder.
- Inserted text is copied into append-only modification storage.
- Returned `String::String8` line content belongs to the arena supplied to the query.
- `release_tree` is the only release needed for a successfully finished tree.
- Reference snapshots release themselves through normal C++ lifetime.
- Owning snapshots require `release_owning_snap`.
- A walker must not outlive its source tree or snapshot.
- A walker's arena must remain valid for the full traversal.
- Retained roots must only be used with their originating buffer collection.

## Complexity

Let `p` be the number of pieces and `n` the number of traversed or returned characters.

| Operation | Expected complexity |
| --- | --- |
| Character or offset lookup | `O(log p)` |
| Line lookup | `O(log p)` plus returned content |
| Insert | `O(log p)` structural work |
| Remove | `O(k log p)` when the range intersects `k` pieces |
| Root duplication | `O(1)` |
| Undo or redo root switch | `O(1)` excluding released-reference cleanup |
| Sequential forward or reverse walk | `O(n)` |
| Reference snapshot capture | `O(1)` |
| Owning snapshot capture | `O(m)` for `m` modification-buffer bytes |
