#ifndef TREE_HPP_
#define TREE_HPP_

namespace ETide::PieceTree {

enum class Column : U64 { Beginning = 0 };
enum class Length : U64 {};
enum class CharOffset : U64 { Sentinel = UINT64_MAX };
enum class Line : U64 { IndexBeginning = 0, Beginning = 0 };
enum class LFCount : U64 {};
enum class BufferIndex : U64 { ModBuf = UINT64_MAX };
enum class LineStart : U64 {};

internal constexpr U64 rep(Column value) {
    return static_cast<U64>(value);
}
internal constexpr U64 rep(Length value) {
    return static_cast<U64>(value);
}
internal constexpr U64 rep(CharOffset value) {
    return static_cast<U64>(value);
}
internal constexpr U64 rep(Line value) {
    return static_cast<U64>(value);
}
internal constexpr U64 rep(LFCount value) {
    return static_cast<U64>(value);
}
internal constexpr U64 rep(BufferIndex value) {
    return static_cast<U64>(value);
}
internal constexpr U64 rep(LineStart value) {
    return static_cast<U64>(value);
}

internal constexpr CharOffset operator+(CharOffset offset, Length length) {
    return CharOffset{rep(offset) + rep(length)};
}
internal constexpr Length operator+(Length a, Length b) {
    return Length{rep(a) + rep(b)};
}
internal constexpr Length operator-(Length a, Length b) {
    return Length{rep(a) - rep(b)};
}
internal constexpr Length distance(CharOffset first, CharOffset last) {
    return Length{rep(last) - rep(first)};
}
template <typename T>
internal constexpr T retract(T value, U64 amount = 1) {
    return T{rep(value) - amount};
}
template <typename T>
internal constexpr T extend(T value, U64 amount = 1) {
    return T{rep(value) + amount};
}

using Offset = CharOffset;

struct BufferCursor {
    Line   line;
    Column column;

    bool operator==(const BufferCursor&) const = default;
};

struct Piece {
    BufferIndex  index;
    BufferCursor first;
    BufferCursor last;
    Length       length;
    LFCount      newline_count;
};

struct NodeData {
    Piece   piece;
    Length  left_subtree_length;
    LFCount left_subtree_lf_count;
};

enum class Color : U8 { Red, Black };

typedef struct RBNodeCounted RBNodeCounted;
typedef struct RBTreeBlock   RBTreeBlock;

struct RBNodeCounted {
    RBNodeCounted* left;
    RBNodeCounted* right;
    RBNodeCounted* free_next;
    RBTreeBlock*   block;
    NodeData       data;
    Length         subtree_length;
    LFCount        subtree_lf_count;
    U64            ref_count;
    Color          color;
};

struct alignas(16) RBNodeFreeList {
    RBNodeCounted* head;
    U64            tag;
};

struct RBTreeBlock {
    RBNodeFreeList free_list;
    Arena::Arena*  alloc_arena;
    Thread::Mutex* allocation_mutex;
};

internal void           dec_node_ref(RBNodeCounted* node);
internal RBNodeCounted* take_node_ref(RBNodeCounted* node);

class RedBlackTree {
   public:
    RedBlackTree() = default;
    RedBlackTree(RedBlackTree&& other) noexcept;
    RedBlackTree& operator=(RedBlackTree&& other) noexcept;
    RedBlackTree(const RedBlackTree&)            = delete;
    RedBlackTree& operator=(const RedBlackTree&) = delete;
    ~RedBlackTree();

    RBNodeCounted* root_ptr() const { return m_root; }
    B32            is_empty() const { return m_root == 0; }
    NodeData&      root() const;
    RedBlackTree   left() const;
    RedBlackTree   right() const;
    Color          root_color() const;
    RedBlackTree   insert(RBTreeBlock* block, NodeData data, Offset at) const;
    RedBlackTree   remove(RBTreeBlock* block, Offset at) const;
    RedBlackTree   dup() const;
    B32            operator==(const RedBlackTree& other) const { return m_root == other.m_root; }

   private:
    explicit RedBlackTree(RBNodeCounted* root, B32 retain);
    friend class Tree;
    friend class OwningSnapshot;
    friend class ReferenceSnapshot;
    friend class TreeWalker;
    friend class ReverseTreeWalker;
    RBNodeCounted* m_root = 0;
};

internal Length  tree_length(const RedBlackTree& root);
internal LFCount tree_lf_count(const RedBlackTree& root);

struct LineStarts {
    LineStart* starts;
    U64        count;
};

struct CharBuffer {
    String::String8 buffer;
    LineStarts      line_starts;
};

struct ImmutableBufferArray {
    CharBuffer* buffers;
    U64         count;
    U64*        ref_count;
};

typedef struct SharedBuffers SharedBuffers;
struct SharedBuffers {
    Arena::Arena* immutable_buf_arena;
    Arena::Arena* undo_redo_stack_arena;
    Arena::Arena* mut_buf_starts_arena;
    Arena::Arena* mut_buf_arena;
    RBTreeBlock*  rb_tree_block;
    U64           immutable_ref_count;
    U64           source_ref_count;
};

struct BufferCollection {
    CharBuffer*       buffer_at(BufferIndex index);
    const CharBuffer* buffer_at(BufferIndex index) const;
    CharOffset        buffer_offset(BufferIndex index, BufferCursor cursor) const;

    Arena::Arena*        immutable_buf_arena;
    Arena::Arena*        undo_redo_stack_arena;
    Arena::Arena*        mut_buf_starts_arena;
    Arena::Arena*        mut_buf_arena;
    ImmutableBufferArray orig_buffers;
    CharBuffer           mod_buffer;
    RBTreeBlock*         rb_tree_blk;
    SharedBuffers*       shared;
    B32                  retains_source_arenas;
};

struct BufferMeta {
    LFCount lf_count;
    Length  total_content_length;
};

struct NodePosition {
    NodeData*  node;
    Length     remainder;
    CharOffset start_offset;
    Line       line;
};

struct LineRange {
    CharOffset first;
    CharOffset last;

    bool operator==(const LineRange&) const = default;
};

struct UndoRedoResult {
    B32        success;
    CharOffset op_offset;
};

enum class SuppressHistory : B8 { No, Yes };
enum class IncompleteCRLF : B8 { No, Yes };

typedef struct UndoRedoEntry UndoRedoEntry;
struct UndoRedoEntry {
    UndoRedoEntry* next;
    RedBlackTree   root;
    CharOffset     op_offset;
};

struct UndoRedoList {
    UndoRedoEntry* first;
    UndoRedoEntry* last;
    U64            count;
};

using UndoStack = UndoRedoList;
using RedoStack = UndoRedoList;

internal void             dec_buffer_ref(BufferCollection* collection);
internal BufferCollection take_buffer_ref(const BufferCollection* collection);
internal BufferCollection take_immutable_buffer_ref(const BufferCollection* collection);

class OwningSnapshot;
class ReferenceSnapshot;
class TreeWalker;
class ReverseTreeWalker;

class Tree {
   public:
    explicit Tree(BufferCollection buffers);
    ~Tree();

    void           build_tree();
    void           insert(CharOffset      offset,
                          String::String8 text,
                          SuppressHistory suppress_history = SuppressHistory::No);
    void           remove(CharOffset      offset,
                          Length          count,
                          SuppressHistory suppress_history = SuppressHistory::No);
    UndoRedoResult try_undo(CharOffset op_offset);
    UndoRedoResult try_redo(CharOffset op_offset);
    void           commit_head(CharOffset offset);
    RedBlackTree   head() const;
    void           snap_to(const RedBlackTree& new_root);

    String::String8 get_line_content(Arena::Arena* arena, Line line) const;
    IncompleteCRLF  get_line_content_crlf(Arena::Arena*    arena,
                                          String::String8* buffer,
                                          Line             line) const;
    char            at(CharOffset offset) const;
    Line            line_at(CharOffset offset) const;
    LineRange       get_line_range(Line line) const;
    LineRange       get_line_range_crlf(Line line) const;
    LineRange       get_line_range_with_newline(Line line) const;
    Length          length() const { return m_meta.total_content_length; }
    B32             is_empty() const { return rep(length()) == 0; }
    LFCount         line_feed_count() const { return m_meta.lf_count; }
    Length          line_count() const { return Length{rep(m_meta.lf_count) + 1}; }

    OwningSnapshot*   owning_snap(Arena::Arena* arena) const;
    ReferenceSnapshot ref_snap() const;
    BufferCollection  buffer_collection_no_ref() const { return m_buffers; }

   private:
    friend class OwningSnapshot;
    friend class ReferenceSnapshot;
    friend class TreeWalker;
    friend class ReverseTreeWalker;

    void            internal_insert(CharOffset offset, String::String8 text);
    void            internal_remove(CharOffset offset, Length count);
    Piece           build_piece(String::String8 text);
    NodePosition    node_at(CharOffset offset) const;
    BufferCursor    buffer_position(Piece piece, Length remainder) const;
    Piece           piece_range(Piece piece, U64 first, U64 last) const;
    void            append_undo(const RedBlackTree& old_root, CharOffset op_offset);
    void            clear_history(UndoRedoList* list);
    void            set_root(RedBlackTree root);
    void            compute_buffer_meta();
    String::String8 assemble_range(Arena::Arena* arena, CharOffset first, CharOffset last) const;

    BufferCollection m_buffers;
    RedBlackTree     m_root;
    BufferCursor     m_last_insert;
    CharOffset       m_end_last_insert = CharOffset::Sentinel;
    BufferMeta       m_meta;
    UndoStack        m_undo_stack;
    RedoStack        m_redo_stack;
    UndoRedoEntry*   m_free_undo_list = 0;
};

struct ImmutableBufferNode {
    ImmutableBufferNode* next;
    CharBuffer           buffer;
};

struct ImmutableBufferList {
    ImmutableBufferNode* first;
    ImmutableBufferNode* last;
    U64                  count;
};

struct TreeBuilder {
    Arena::Arena*       immutable_buf_arena;
    Arena::Arena*       undo_redo_stack_arena;
    Arena::Arena*       mut_buf_starts_arena;
    Arena::Arena*       mut_buf_arena;
    ImmutableBufferList buffers;
};

internal TreeBuilder tree_builder_start(Arena::Arena* buffer_arena);
internal void  tree_builder_accept(Arena::Arena* arena, TreeBuilder* builder, String::String8 text);
internal Tree* tree_builder_finish(TreeBuilder* builder);
internal Tree* tree_builder_empty(Arena::Arena* buffer_arena);
internal void  release_tree(Tree* tree);

class OwningSnapshot {
   public:
    OwningSnapshot(Arena::Arena* arena, const Tree* tree);
    OwningSnapshot(Arena::Arena* arena, const Tree* tree, const RedBlackTree& root);
    ~OwningSnapshot();

    String::String8  get_line_content(Arena::Arena* arena, Line line) const;
    IncompleteCRLF   get_line_content_crlf(Arena::Arena*    arena,
                                           String::String8* buffer,
                                           Line             line) const;
    char             at(CharOffset offset) const;
    Line             line_at(CharOffset offset) const;
    LineRange        get_line_range(Line line) const;
    LineRange        get_line_range_crlf(Line line) const;
    LineRange        get_line_range_with_newline(Line line) const;
    B32              is_empty() const { return rep(m_meta.total_content_length) == 0; }
    Length           line_count() const { return Length{rep(m_meta.lf_count) + 1}; }
    Length           length() const { return m_meta.total_content_length; }
    BufferCollection buffer_collection_no_ref() const { return m_buffers; }

   private:
    friend class TreeWalker;
    friend class ReverseTreeWalker;
    RedBlackTree     m_root;
    BufferMeta       m_meta;
    BufferCollection m_buffers;
    Arena::Arena*    m_owned_mut_buf_arena;
    Arena::Arena*    m_owned_mut_starts_arena;
};

internal void release_owning_snap(OwningSnapshot* snapshot);

class ReferenceSnapshot {
   public:
    explicit ReferenceSnapshot(const Tree* tree);
    ReferenceSnapshot(const Tree* tree, const RedBlackTree& root);
    ReferenceSnapshot(const ReferenceSnapshot& other);
    ReferenceSnapshot& operator=(const ReferenceSnapshot& other);
    ~ReferenceSnapshot();

    String::String8 get_line_content(Arena::Arena* arena, Line line) const;
    IncompleteCRLF  get_line_content_crlf(Arena::Arena*    arena,
                                          String::String8* buffer,
                                          Line             line) const;
    char            at(CharOffset offset) const;
    Line            line_at(CharOffset offset) const;
    LineRange       get_line_range(Line line) const;
    LineRange       get_line_range_crlf(Line line) const;
    LineRange       get_line_range_with_newline(Line line) const;
    B32             is_empty() const { return rep(m_meta.total_content_length) == 0; }
    Length          line_count() const { return Length{rep(m_meta.lf_count) + 1}; }
    Length          length() const { return m_meta.total_content_length; }

   private:
    friend class TreeWalker;
    friend class ReverseTreeWalker;
    RedBlackTree     m_root;
    BufferMeta       m_meta;
    BufferCollection m_buffers;
};

enum class Direction : U8 { Left, Center, Right };

struct StackEntry {
    StackEntry*    next;
    RBNodeCounted* node;
    Direction      direction;
};

struct StackList {
    StackEntry* stack;
    StackEntry* free_list;
};

class TreeWalker {
   public:
    TreeWalker(Arena::Arena* arena, const Tree* tree, CharOffset offset = {});
    TreeWalker(Arena::Arena* arena, const OwningSnapshot* snapshot, CharOffset offset = {});
    TreeWalker(Arena::Arena* arena, const ReferenceSnapshot* snapshot, CharOffset offset = {});
    TreeWalker(Arena::Arena*       arena,
               BufferCollection*   buffers,
               BufferMeta          meta,
               const RedBlackTree& root,
               CharOffset          offset = {});
    TreeWalker(const TreeWalker&) = delete;

    char       current();
    char       next();
    void       seek(CharOffset offset);
    B32        exhausted() const;
    Length     remaining() const;
    CharOffset offset() const { return m_offset; }

   private:
    BufferCollection* m_buffers;
    RedBlackTree      m_root;
    BufferMeta        m_meta;
    Arena::Arena*     m_arena;
    StackList         m_stack        = {};
    RBNodeCounted*    m_node         = 0;
    U64               m_piece_offset = 0;
    CharOffset        m_offset       = {};
};

class ReverseTreeWalker {
   public:
    ReverseTreeWalker(Arena::Arena* arena, const Tree* tree, CharOffset offset = {});
    ReverseTreeWalker(Arena::Arena* arena, const OwningSnapshot* snapshot, CharOffset offset = {});
    ReverseTreeWalker(Arena::Arena*            arena,
                      const ReferenceSnapshot* snapshot,
                      CharOffset               offset = {});
    ReverseTreeWalker(const ReverseTreeWalker&) = delete;

    char       current();
    char       next();
    void       seek(CharOffset offset);
    B32        exhausted() const;
    Length     remaining() const;
    CharOffset offset() const { return m_offset; }

   private:
    BufferCollection* m_buffers;
    RedBlackTree      m_root;
    BufferMeta        m_meta;
    Arena::Arena*     m_arena;
    StackList         m_stack        = {};
    RBNodeCounted*    m_node         = 0;
    U64               m_piece_offset = 0;
    CharOffset        m_offset       = {};
};

enum class EmptySelection : B8 { No, Yes };

struct Selection {
    Offset         first;
    Offset         last;
    EmptySelection empty;
};

struct SelectionNode {
    SelectionNode* next;
    Selection      selection;
};

struct SelectionList {
    SelectionNode* first;
    SelectionNode* last;
    U64            count;
};

internal SelectionNode* push_selection(Arena::Arena*  arena,
                                       SelectionList* list,
                                       Selection      selection);
internal void           pop_selection(SelectionList* list);

struct SelectionMeta {
    OwningSnapshot* snap;
    SelectionList   selections;
};

}  // namespace ETide::PieceTree

#endif
