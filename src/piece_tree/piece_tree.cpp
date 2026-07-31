namespace ETide::PieceTree {

internal U64 node_length(RBNodeCounted* node) {
    return node != 0 ? rep(node->subtree_length) : 0;
}

internal U64 node_lf_count(RBNodeCounted* node) {
    return node != 0 ? rep(node->subtree_lf_count) : 0;
}

internal B32 node_is_red(RBNodeCounted* node) {
    return node != 0 && node->color == Color::Red;
}

internal void update_node_metadata(RBNodeCounted* node) {
    U64 left_length                  = node_length(node->left);
    U64 left_lf                      = node_lf_count(node->left);
    node->data.left_subtree_length   = Length{left_length};
    node->data.left_subtree_lf_count = LFCount{left_lf};
    node->subtree_length =
        Length{left_length + rep(node->data.piece.length) + node_length(node->right)};
    node->subtree_lf_count =
        LFCount{left_lf + rep(node->data.piece.newline_count) + node_lf_count(node->right)};
}

internal RBNodeCounted* take_node_ref(RBNodeCounted* node) {
    if (node != 0) {
        std::atomic_ref<U64> refs(node->ref_count);
        refs.fetch_add(1, std::memory_order_relaxed);
    }
    return node;
}

internal void dec_node_ref(RBNodeCounted* node) {
    if (node == 0) { return; }

    std::atomic_ref<U64> refs(node->ref_count);
    if (refs.fetch_sub(1, std::memory_order_acq_rel) != 1) { return; }

    RBNodeCounted* left    = node->left;
    RBNodeCounted* right   = node->right;
    RBTreeBlock*   block   = node->block;
    node->left             = 0;
    node->right            = 0;
    node->data             = {};
    node->subtree_length   = {};
    node->subtree_lf_count = {};

    std::atomic_ref<RBNodeFreeList> free_list(block->free_list);
    RBNodeFreeList                  expected = free_list.load(std::memory_order_acquire);
    RBNodeFreeList                  desired;
    do {
        std::atomic_ref<RBNodeCounted*> next(node->free_next);
        next.store(expected.head, std::memory_order_release);
        desired = {.head = node, .tag = expected.tag + 1};
    } while (!free_list.compare_exchange_weak(expected,
                                              desired,
                                              std::memory_order_release,
                                              std::memory_order_acquire));

    dec_node_ref(left);
    dec_node_ref(right);
}

internal RBNodeCounted* allocate_node(RBTreeBlock* block) {
    RBNodeCounted*                  result = 0;
    std::atomic_ref<RBNodeFreeList> free_list(block->free_list);
    RBNodeFreeList                  expected = free_list.load(std::memory_order_acquire);
    while (expected.head != 0) {
        std::atomic_ref<RBNodeCounted*> next(expected.head->free_next);
        RBNodeFreeList                  desired = {
            .head = next.load(std::memory_order_acquire),
            .tag  = expected.tag + 1,
        };
        if (free_list.compare_exchange_weak(expected,
                                            desired,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
            result = expected.head;
            break;
        }
    }

    if (result == 0) {
        Thread::mutex_lock(block->allocation_mutex);

        expected = free_list.load(std::memory_order_acquire);
        while (expected.head != 0) {
            std::atomic_ref<RBNodeCounted*> next(expected.head->free_next);
            RBNodeFreeList                  desired = {
                .head = next.load(std::memory_order_acquire),
                .tag  = expected.tag + 1,
            };
            if (free_list.compare_exchange_weak(expected,
                                                desired,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
                result = expected.head;
                break;
            }
        }
        if (result == 0) { result = Arena::push_array<RBNodeCounted>(block->alloc_arena, 1); }
        Thread::mutex_unlock(block->allocation_mutex);
        if (result == 0) { throw std::bad_alloc(); }
    }
    result->left             = 0;
    result->right            = 0;
    result->block            = block;
    result->data             = {};
    result->subtree_length   = {};
    result->subtree_lf_count = {};
    result->ref_count        = 1;
    result->color            = Color::Black;
    return result;
}

internal RBNodeCounted* make_node(RBTreeBlock*   block,
                                  Color          color,
                                  RBNodeCounted* left,
                                  NodeData       data,
                                  RBNodeCounted* right) {
    RBNodeCounted* result = allocate_node(block);
    result->left          = take_node_ref(left);
    result->right         = take_node_ref(right);
    result->data          = data;
    result->color         = color;
    update_node_metadata(result);
    return result;
}

internal RBNodeCounted* repaint_owned(RBTreeBlock* block, RBNodeCounted* node, Color color) {
    if (node == 0) { return 0; }
    RBNodeCounted* result = make_node(block, color, node->left, node->data, node->right);
    dec_node_ref(node);
    return result;
}

internal RBNodeCounted* rotate_left_owned(RBTreeBlock* block, RBNodeCounted* node) {
    RBNodeCounted* pivot  = node->right;
    RBNodeCounted* left   = make_node(block, Color::Red, node->left, node->data, pivot->left);
    RBNodeCounted* result = make_node(block, node->color, left, pivot->data, pivot->right);
    dec_node_ref(left);
    dec_node_ref(node);
    return result;
}

internal RBNodeCounted* rotate_right_owned(RBTreeBlock* block, RBNodeCounted* node) {
    RBNodeCounted* pivot  = node->left;
    RBNodeCounted* right  = make_node(block, Color::Red, pivot->right, node->data, node->right);
    RBNodeCounted* result = make_node(block, node->color, pivot->left, pivot->data, right);
    dec_node_ref(right);
    dec_node_ref(node);
    return result;
}

internal RBNodeCounted* flip_colors_owned(RBTreeBlock* block, RBNodeCounted* node) {
    Color          parent_color = node->color == Color::Red ? Color::Black : Color::Red;
    Color          left_color   = node->left->color == Color::Red ? Color::Black : Color::Red;
    Color          right_color  = node->right->color == Color::Red ? Color::Black : Color::Red;
    RBNodeCounted* left =
        make_node(block, left_color, node->left->left, node->left->data, node->left->right);
    RBNodeCounted* right =
        make_node(block, right_color, node->right->left, node->right->data, node->right->right);
    RBNodeCounted* result = make_node(block, parent_color, left, node->data, right);
    dec_node_ref(left);
    dec_node_ref(right);
    dec_node_ref(node);
    return result;
}

internal RBNodeCounted* fix_up_owned(RBTreeBlock* block, RBNodeCounted* node) {
    if (node_is_red(node->right) && !node_is_red(node->left)) {
        node = rotate_left_owned(block, node);
    }
    if (node_is_red(node->left) && node_is_red(node->left->left)) {
        node = rotate_right_owned(block, node);
    }
    if (node_is_red(node->left) && node_is_red(node->right)) {
        node = flip_colors_owned(block, node);
    }
    return node;
}

internal RBNodeCounted* move_red_left_owned(RBTreeBlock* block, RBNodeCounted* node) {
    node = flip_colors_owned(block, node);
    if (node->right != 0 && node_is_red(node->right->left)) {
        RBNodeCounted* right  = make_node(block,
                                          node->right->color,
                                          node->right->left,
                                          node->right->data,
                                          node->right->right);
        right                 = rotate_right_owned(block, right);
        RBNodeCounted* parent = make_node(block, node->color, node->left, node->data, right);
        dec_node_ref(right);
        dec_node_ref(node);
        node = rotate_left_owned(block, parent);
        node = flip_colors_owned(block, node);
    }
    return node;
}

internal RBNodeCounted* move_red_right_owned(RBTreeBlock* block, RBNodeCounted* node) {
    node = flip_colors_owned(block, node);
    if (node->left != 0 && node_is_red(node->left->left)) {
        node = rotate_right_owned(block, node);
        node = flip_colors_owned(block, node);
    }
    return node;
}

internal RBNodeCounted* insert_node(RBTreeBlock*   block,
                                    RBNodeCounted* node,
                                    NodeData       data,
                                    U64            offset) {
    if (node == 0) { return make_node(block, Color::Red, 0, data, 0); }

    U64            left_length = node_length(node->left);
    RBNodeCounted* result;
    if (offset <= left_length) {
        RBNodeCounted* left = insert_node(block, node->left, data, offset);
        result              = make_node(block, node->color, left, node->data, node->right);
        dec_node_ref(left);
    } else {
        U64            right_offset = offset - left_length - rep(node->data.piece.length);
        RBNodeCounted* right        = insert_node(block, node->right, data, right_offset);
        result                      = make_node(block, node->color, node->left, node->data, right);
        dec_node_ref(right);
    }
    return fix_up_owned(block, result);
}

internal RBNodeCounted* minimum_node(RBNodeCounted* node) {
    while (node->left != 0) { node = node->left; }
    return node;
}

internal RBNodeCounted* delete_min_node(RBTreeBlock* block, RBNodeCounted* node) {
    if (node->left == 0) { return 0; }

    RBNodeCounted* owned = make_node(block, node->color, node->left, node->data, node->right);
    if (!node_is_red(owned->left) && (owned->left->left == 0 || !node_is_red(owned->left->left))) {
        owned = move_red_left_owned(block, owned);
    }
    RBNodeCounted* left   = delete_min_node(block, owned->left);
    RBNodeCounted* result = make_node(block, owned->color, left, owned->data, owned->right);
    dec_node_ref(left);
    dec_node_ref(owned);
    return fix_up_owned(block, result);
}

internal RBNodeCounted* delete_node(RBTreeBlock* block, RBNodeCounted* node, U64 offset) {
    RBNodeCounted* owned = make_node(block, node->color, node->left, node->data, node->right);

    if (offset < node_length(owned->left)) {
        if (owned->left != 0 && !node_is_red(owned->left) &&
            (owned->left->left == 0 || !node_is_red(owned->left->left))) {
            owned = move_red_left_owned(block, owned);
        }
        RBNodeCounted* left   = delete_node(block, owned->left, offset);
        RBNodeCounted* result = make_node(block, owned->color, left, owned->data, owned->right);
        dec_node_ref(left);
        dec_node_ref(owned);
        return fix_up_owned(block, result);
    }

    if (node_is_red(owned->left)) { owned = rotate_right_owned(block, owned); }

    U64 left_length  = node_length(owned->left);
    U64 piece_length = rep(owned->data.piece.length);
    if (offset == left_length && owned->right == 0) {
        dec_node_ref(owned);
        return 0;
    }

    if (owned->right != 0 && !node_is_red(owned->right) &&
        (owned->right->left == 0 || !node_is_red(owned->right->left))) {
        owned = move_red_right_owned(block, owned);
    }

    left_length  = node_length(owned->left);
    piece_length = rep(owned->data.piece.length);
    if (offset == left_length) {
        RBNodeCounted* successor = minimum_node(owned->right);
        RBNodeCounted* right     = delete_min_node(block, owned->right);
        RBNodeCounted* result = make_node(block, owned->color, owned->left, successor->data, right);
        dec_node_ref(right);
        dec_node_ref(owned);
        return fix_up_owned(block, result);
    }

    U64            right_offset = offset - left_length - piece_length;
    RBNodeCounted* right        = delete_node(block, owned->right, right_offset);
    RBNodeCounted* result       = make_node(block, owned->color, owned->left, owned->data, right);
    dec_node_ref(right);
    dec_node_ref(owned);
    return fix_up_owned(block, result);
}

RedBlackTree::RedBlackTree(RBNodeCounted* root, B32 retain) :
    m_root(retain ? take_node_ref(root) : root) {}

RedBlackTree::RedBlackTree(RedBlackTree&& other) noexcept : m_root(other.m_root) {
    other.m_root = 0;
}

RedBlackTree& RedBlackTree::operator=(RedBlackTree&& other) noexcept {
    if (this != &other) {
        dec_node_ref(m_root);
        m_root       = other.m_root;
        other.m_root = 0;
    }
    return *this;
}

RedBlackTree::~RedBlackTree() {
    dec_node_ref(m_root);
}

NodeData& RedBlackTree::root() const {
    SDL_assert(m_root != 0);
    return m_root->data;
}

RedBlackTree RedBlackTree::left() const {
    return RedBlackTree(m_root != 0 ? m_root->left : 0, 1);
}

RedBlackTree RedBlackTree::right() const {
    return RedBlackTree(m_root != 0 ? m_root->right : 0, 1);
}

Color RedBlackTree::root_color() const {
    return m_root != 0 ? m_root->color : Color::Black;
}

RedBlackTree RedBlackTree::insert(RBTreeBlock* block, NodeData data, Offset at) const {
    SDL_assert(rep(at) <= node_length(m_root));
    RBNodeCounted* root = insert_node(block, m_root, data, rep(at));
    if (root->color != Color::Black) { root = repaint_owned(block, root, Color::Black); }
    return RedBlackTree(root, 0);
}

RedBlackTree RedBlackTree::remove(RBTreeBlock* block, Offset at) const {
    SDL_assert(m_root != 0 && rep(at) < node_length(m_root));
    RBNodeCounted* root = delete_node(block, m_root, rep(at));
    if (root != 0 && root->color != Color::Black) {
        root = repaint_owned(block, root, Color::Black);
    }
    return RedBlackTree(root, 0);
}

RedBlackTree RedBlackTree::dup() const {
    return RedBlackTree(m_root, 1);
}

internal Length tree_length(const RedBlackTree& root) {
    return Length{node_length(root.root_ptr())};
}

internal LFCount tree_lf_count(const RedBlackTree& root) {
    return LFCount{node_lf_count(root.root_ptr())};
}

internal U64 line_start_index(CharBuffer* buffer, U64 byte_offset) {
    U64 first = 0;
    U64 count = buffer->line_starts.count;
    while (count > 0) {
        U64 step = count / 2;
        U64 idx  = first + step;
        if (rep(buffer->line_starts.starts[idx]) <= byte_offset) {
            first = idx + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first == 0 ? 0 : first - 1;
}

internal BufferCursor cursor_from_offset(CharBuffer* buffer, U64 byte_offset) {
    SDL_assert(byte_offset <= buffer->buffer.size);
    U64 line = line_start_index(buffer, byte_offset);
    return {
        .line   = Line{line},
        .column = Column{byte_offset - rep(buffer->line_starts.starts[line])},
    };
}

internal U64 cursor_offset(CharBuffer* buffer, BufferCursor cursor) {
    SDL_assert(rep(cursor.line) < buffer->line_starts.count);
    return rep(buffer->line_starts.starts[rep(cursor.line)]) + rep(cursor.column);
}

internal U64 lf_count_in_range(CharBuffer* buffer, U64 first_offset, U64 last_offset) {
    U64 first_line = line_start_index(buffer, first_offset);
    U64 last_line  = line_start_index(buffer, last_offset);
    U64 result     = last_line - first_line;
    if (last_offset < buffer->buffer.size && last_offset > 0 &&
        buffer->buffer.str[last_offset - 1] != '\n') {
        result = last_line - first_line;
    }
    return result;
}

CharBuffer* BufferCollection::buffer_at(BufferIndex index) {
    if (index == BufferIndex::ModBuf) { return &mod_buffer; }
    SDL_assert(rep(index) < orig_buffers.count);
    return &orig_buffers.buffers[rep(index)];
}

const CharBuffer* BufferCollection::buffer_at(BufferIndex index) const {
    if (index == BufferIndex::ModBuf) { return &mod_buffer; }
    SDL_assert(rep(index) < orig_buffers.count);
    return &orig_buffers.buffers[rep(index)];
}

CharOffset BufferCollection::buffer_offset(BufferIndex index, BufferCursor cursor) const {
    return CharOffset{cursor_offset(const_cast<CharBuffer*>(buffer_at(index)), cursor)};
}

internal BufferCollection take_buffer_ref(const BufferCollection* collection) {
    BufferCollection result = *collection;
    if (result.shared != 0) {
        std::atomic_ref<U64> immutable_refs(result.shared->immutable_ref_count);
        immutable_refs.fetch_add(1, std::memory_order_relaxed);
        if (result.retains_source_arenas) {
            std::atomic_ref<U64> source_refs(result.shared->source_ref_count);
            source_refs.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return result;
}

internal BufferCollection take_immutable_buffer_ref(const BufferCollection* collection) {
    BufferCollection result      = *collection;
    result.retains_source_arenas = 0;
    if (result.shared != 0) {
        std::atomic_ref<U64> refs(result.shared->immutable_ref_count);
        refs.fetch_add(1, std::memory_order_relaxed);
    }
    return result;
}

internal void dec_buffer_ref(BufferCollection* collection) {
    if (collection == 0 || collection->shared == 0) { return; }
    SharedBuffers* shared = collection->shared;
    collection->shared    = 0;
    if (collection->retains_source_arenas) {
        std::atomic_ref<U64> source_refs(shared->source_ref_count);
        if (source_refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            Arena::release(shared->undo_redo_stack_arena);
            Arena::release(shared->mut_buf_starts_arena);
            Arena::release(shared->mut_buf_arena);
        }
    }

    std::atomic_ref<U64> immutable_refs(shared->immutable_ref_count);
    if (immutable_refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (shared->rb_tree_block->allocation_mutex != 0) {
            Thread::mutex_destroy(shared->rb_tree_block->allocation_mutex);
            shared->rb_tree_block->allocation_mutex = 0;
        }
        Arena::release(shared->immutable_buf_arena);
    }
}

internal LineStarts calculate_line_starts(Arena::Arena* arena, String::String8 text) {
    U64 count = 1;
    for (U64 idx = 0; idx < text.size; ++idx) {
        if (text.str[idx] == '\n') { ++count; }
    }
    LineStart* starts = Arena::push_array_no_zero<LineStart>(arena, count);
    if (starts == 0) { return {}; }
    starts[0] = LineStart{0};
    U64 line  = 1;
    for (U64 idx = 0; idx < text.size; ++idx) {
        if (text.str[idx] == '\n') { starts[line++] = LineStart{idx + 1}; }
    }
    return {.starts = starts, .count = count};
}

internal NodePosition find_node(RBNodeCounted* root, CharOffset offset) {
    NodePosition   result         = {};
    U64            target         = rep(offset);
    U64            document_start = 0;
    U64            document_line  = 0;
    RBNodeCounted* node           = root;

    while (node != 0) {
        U64 left_length  = node_length(node->left);
        U64 left_lf      = node_lf_count(node->left);
        U64 piece_length = rep(node->data.piece.length);
        if (target < left_length) {
            node = node->left;
        } else if (target < left_length + piece_length) {
            result.node         = &node->data;
            result.remainder    = Length{target - left_length};
            result.start_offset = CharOffset{document_start + left_length};
            result.line         = Line{document_line + left_lf};
            return result;
        } else {
            target -= left_length + piece_length;
            document_start += left_length + piece_length;
            document_line += left_lf + rep(node->data.piece.newline_count);
            node = node->right;
        }
    }
    result.start_offset = CharOffset{document_start};
    result.line         = Line{document_line};
    return result;
}

internal U64 line_offset(BufferCollection* buffers, RBNodeCounted* root, U64 line) {
    if (line == 0) { return 0; }

    U64            target = line;
    U64            base   = 0;
    RBNodeCounted* node   = root;
    while (node != 0) {
        U64 left_lf = node_lf_count(node->left);
        if (target <= left_lf) {
            node = node->left;
            continue;
        }

        U64 left_length = node_length(node->left);
        target -= left_lf;
        U64 piece_lf = rep(node->data.piece.newline_count);
        if (target <= piece_lf) {
            Piece       piece       = node->data.piece;
            CharBuffer* buffer      = buffers->buffer_at(piece.index);
            U64         piece_first = cursor_offset(buffer, piece.first);
            U64         first_line  = line_start_index(buffer, piece_first);
            U64         start       = rep(buffer->line_starts.starts[first_line + target]);
            return base + left_length + start - piece_first;
        }

        target -= piece_lf;
        base += left_length + rep(node->data.piece.length);
        node = node->right;
    }
    return node_length(root);
}

internal U64 line_at_offset(BufferCollection* buffers, RBNodeCounted* root, U64 offset) {
    U64            result = 0;
    RBNodeCounted* node   = root;
    while (node != 0) {
        U64 left_length = node_length(node->left);
        if (offset < left_length) {
            node = node->left;
            continue;
        }

        result += node_lf_count(node->left);
        offset -= left_length;
        U64 piece_length = rep(node->data.piece.length);
        if (offset <= piece_length) {
            Piece       piece  = node->data.piece;
            CharBuffer* buffer = buffers->buffer_at(piece.index);
            U64         first  = cursor_offset(buffer, piece.first);
            result += lf_count_in_range(buffer, first, first + offset);
            return result;
        }
        result += rep(node->data.piece.newline_count);
        offset -= piece_length;
        node = node->right;
    }
    return result;
}

internal char char_at_tree(BufferCollection* buffers, RBNodeCounted* root, U64 offset) {
    NodePosition position = find_node(root, CharOffset{offset});
    SDL_assert(position.node != 0);
    Piece       piece       = position.node->piece;
    CharBuffer* buffer      = buffers->buffer_at(piece.index);
    U64         byte_offset = cursor_offset(buffer, piece.first) + rep(position.remainder);
    return buffer->buffer.str[byte_offset];
}

internal String::String8 assemble_tree_range(Arena::Arena*     arena,
                                             BufferCollection* buffers,
                                             RBNodeCounted*    root,
                                             U64               first,
                                             U64               last) {
    SDL_assert(first <= last && last <= node_length(root));
    String::String8 result = String::str8_cstr_alloc(arena, last - first);
    if (result.str == 0) { return {}; }
    U64 output = 0;
    U64 offset = first;
    while (offset < last) {
        NodePosition position  = find_node(root, CharOffset{offset});
        Piece        piece     = position.node->piece;
        CharBuffer*  buffer    = buffers->buffer_at(piece.index);
        U64          available = rep(piece.length) - rep(position.remainder);
        U64          count     = std::min(available, last - offset);
        U64          source    = cursor_offset(buffer, piece.first) + rep(position.remainder);
        SDL_memcpy(result.str + output, buffer->buffer.str + source, count);
        output += count;
        offset += count;
    }
    return result;
}

internal LineRange tree_line_range(BufferCollection* buffers,
                                   BufferMeta        meta,
                                   RBNodeCounted*    root,
                                   Line              line,
                                   B32               strip_cr,
                                   B32               include_newline) {
    U64 line_idx   = rep(line);
    U64 line_count = rep(meta.lf_count) + 1;
    SDL_assert(line_idx < line_count);
    U64 first = line_offset(buffers, root, line_idx);
    U64 last  = node_length(root);
    if (line_idx + 1 < line_count) {
        U64 next = line_offset(buffers, root, line_idx + 1);
        last     = include_newline ? next : next - 1;
        if (!include_newline && strip_cr && last > first &&
            char_at_tree(buffers, root, last - 1) == '\r') {
            --last;
        }
    }
    return {.first = CharOffset{first}, .last = CharOffset{last}};
}

TreeBuilder tree_builder_start(Arena::Arena* buffer_arena) {
    TreeBuilder result = {};
    if (buffer_arena == 0) { return result; }

    result.immutable_buf_arena   = buffer_arena;
    result.undo_redo_stack_arena = Arena::allocate({});
    result.mut_buf_starts_arena =
        Arena::allocate({.flags = Arena::ArenaFlags_NoChain, .reserve_size = GB(64)});
    result.mut_buf_arena =
        Arena::allocate({.flags = Arena::ArenaFlags_NoChain, .reserve_size = GB(64)});
    if (result.undo_redo_stack_arena == 0 || result.mut_buf_starts_arena == 0 ||
        result.mut_buf_arena == 0) {
        if (result.undo_redo_stack_arena != 0) { Arena::release(result.undo_redo_stack_arena); }
        if (result.mut_buf_starts_arena != 0) { Arena::release(result.mut_buf_starts_arena); }
        if (result.mut_buf_arena != 0) { Arena::release(result.mut_buf_arena); }
        result = {};
    }
    return result;
}

void tree_builder_accept(Arena::Arena* arena, TreeBuilder* builder, String::String8 text) {
    (void)arena;
    if (builder == 0 || builder->immutable_buf_arena == 0) { return; }

    ImmutableBufferNode* node =
        Arena::push_array<ImmutableBufferNode>(builder->immutable_buf_arena, 1);
    if (node == 0) { throw std::bad_alloc(); }
    node->buffer.buffer = String::str8_copy(builder->immutable_buf_arena, text);
    node->buffer.line_starts =
        calculate_line_starts(builder->immutable_buf_arena, node->buffer.buffer);
    if ((text.size != 0 && node->buffer.buffer.str == 0) || node->buffer.line_starts.starts == 0) {
        throw std::bad_alloc();
    }
    SLLQueuePush(builder->buffers.first, builder->buffers.last, node);
    ++builder->buffers.count;
}

Tree* tree_builder_finish(TreeBuilder* builder) {
    if (builder == 0 || builder->immutable_buf_arena == 0 || builder->undo_redo_stack_arena == 0 ||
        builder->mut_buf_starts_arena == 0 || builder->mut_buf_arena == 0) {
        return 0;
    }

    CharBuffer* immutable_buffers =
        Arena::push_array<CharBuffer>(builder->immutable_buf_arena, builder->buffers.count);
    if (immutable_buffers == 0 && builder->buffers.count != 0) { return 0; }
    U64 buffer_idx = 0;
    for (ImmutableBufferNode* node = builder->buffers.first; node != 0; node = node->next) {
        immutable_buffers[buffer_idx++] = node->buffer;
    }

    RBTreeBlock*   block  = Arena::push_array<RBTreeBlock>(builder->immutable_buf_arena, 1);
    SharedBuffers* shared = Arena::push_array<SharedBuffers>(builder->immutable_buf_arena, 1);
    if (block == 0 || shared == 0) { return 0; }

    block->alloc_arena      = builder->immutable_buf_arena;
    block->allocation_mutex = Thread::mutex_create();
    if (block->allocation_mutex == 0) { return 0; }
    char*      mod_buffer = Arena::push_array_no_zero_aligned<char>(builder->mut_buf_arena, 1, 1);
    LineStart* mod_starts = Arena::push_array_no_zero<LineStart>(builder->mut_buf_starts_arena, 1);
    if (mod_buffer == 0 || mod_starts == 0) {
        Thread::mutex_destroy(block->allocation_mutex);
        block->allocation_mutex = 0;
        return 0;
    }
    mod_buffer[0] = 0;
    mod_starts[0] = LineStart{0};

    *shared = {
        .immutable_buf_arena   = builder->immutable_buf_arena,
        .undo_redo_stack_arena = builder->undo_redo_stack_arena,
        .mut_buf_starts_arena  = builder->mut_buf_starts_arena,
        .mut_buf_arena         = builder->mut_buf_arena,
        .rb_tree_block         = block,
        .immutable_ref_count   = 1,
        .source_ref_count      = 1,
    };

    BufferCollection collection = {
        .immutable_buf_arena   = builder->immutable_buf_arena,
        .undo_redo_stack_arena = builder->undo_redo_stack_arena,
        .mut_buf_starts_arena  = builder->mut_buf_starts_arena,
        .mut_buf_arena         = builder->mut_buf_arena,
        .orig_buffers =
            {
                .buffers   = immutable_buffers,
                .count     = builder->buffers.count,
                .ref_count = &shared->immutable_ref_count,
            },
        .mod_buffer =
            {
                .buffer      = {.str = mod_buffer, .size = 0},
                .line_starts = {.starts = mod_starts, .count = 1},
            },
        .rb_tree_blk           = block,
        .shared                = shared,
        .retains_source_arenas = 1,
    };

    Tree* tree_memory = static_cast<Tree*>(
        Arena::push(builder->immutable_buf_arena, sizeof(Tree), alignof(Tree), 0));
    if (tree_memory == 0) { return 0; }
    Tree* tree = ::new (tree_memory) Tree(collection);
    tree->build_tree();
    *builder = {};
    return tree;
}

Tree* tree_builder_empty(Arena::Arena* buffer_arena) {
    TreeBuilder builder = tree_builder_start(buffer_arena);
    return tree_builder_finish(&builder);
}

void release_tree(Tree* tree) {
    if (tree == 0) { return; }
    BufferCollection buffers = tree->buffer_collection_no_ref();
    tree->~Tree();
    dec_buffer_ref(&buffers);
}

Tree::Tree(BufferCollection buffers) : m_buffers(buffers) {}

Tree::~Tree() {
    clear_history(&m_undo_stack);
    clear_history(&m_redo_stack);
    m_free_undo_list = 0;
    m_root           = {};
}

void Tree::build_tree() {
    U64 offset = 0;
    for (U64 idx = 0; idx < m_buffers.orig_buffers.count; ++idx) {
        CharBuffer* buffer = &m_buffers.orig_buffers.buffers[idx];
        if (buffer->buffer.size == 0) { continue; }
        Piece piece = {
            .index         = BufferIndex{idx},
            .first         = {},
            .last          = cursor_from_offset(buffer, buffer->buffer.size),
            .length        = Length{buffer->buffer.size},
            .newline_count = LFCount{buffer->line_starts.count - 1},
        };
        NodeData     data = {.piece = piece};
        RedBlackTree next = m_root.insert(m_buffers.rb_tree_blk, data, CharOffset{offset});
        m_root            = std::move(next);
        offset += buffer->buffer.size;
    }
    compute_buffer_meta();
}

void Tree::compute_buffer_meta() {
    m_meta = {
        .lf_count             = tree_lf_count(m_root),
        .total_content_length = tree_length(m_root),
    };
}

void Tree::set_root(RedBlackTree root) {
    m_root = std::move(root);
    compute_buffer_meta();
}

NodePosition Tree::node_at(CharOffset offset) const {
    return find_node(m_root.root_ptr(), offset);
}

BufferCursor Tree::buffer_position(Piece piece, Length remainder) const {
    CharBuffer* buffer = const_cast<BufferCollection*>(&m_buffers)->buffer_at(piece.index);
    U64         first  = cursor_offset(buffer, piece.first);
    return cursor_from_offset(buffer, first + rep(remainder));
}

Piece Tree::piece_range(Piece piece, U64 first, U64 last) const {
    SDL_assert(first <= last && last <= rep(piece.length));
    CharBuffer* buffer      = const_cast<BufferCollection*>(&m_buffers)->buffer_at(piece.index);
    U64         base        = cursor_offset(buffer, piece.first);
    U64         range_first = base + first;
    U64         range_last  = base + last;
    return {
        .index         = piece.index,
        .first         = cursor_from_offset(buffer, range_first),
        .last          = cursor_from_offset(buffer, range_last),
        .length        = Length{last - first},
        .newline_count = LFCount{lf_count_in_range(buffer, range_first, range_last)},
    };
}

Piece Tree::build_piece(String::String8 text) {
    CharBuffer* buffer          = &m_buffers.mod_buffer;
    U64         first           = buffer->buffer.size;
    U64         old_start_count = buffer->line_starts.count;

    U64 added_lines = 0;
    for (U64 idx = 0; idx < text.size; ++idx) {
        if (text.str[idx] == '\n') { ++added_lines; }
    }

    Arena::Temp byte_temp  = Arena::begin(m_buffers.mut_buf_arena);
    Arena::Temp start_temp = Arena::begin(m_buffers.mut_buf_starts_arena);
    char*       extension =
        Arena::push_array_no_zero_aligned<char>(m_buffers.mut_buf_arena, text.size, 1);
    LineStart* starts =
        Arena::push_array_no_zero<LineStart>(m_buffers.mut_buf_starts_arena, added_lines);
    if ((extension == 0 && text.size != 0) || (starts == 0 && added_lines != 0)) {
        Arena::end(byte_temp);
        Arena::end(start_temp);
        throw std::bad_alloc();
    }
    SDL_assert(extension == buffer->buffer.str + first + 1);
    SDL_assert(starts == buffer->line_starts.starts + old_start_count);

    if (text.size != 0) { SDL_memcpy(buffer->buffer.str + first, text.str, text.size); }
    buffer->buffer.size += text.size;
    buffer->buffer.str[buffer->buffer.size] = 0;

    U64 line = 0;
    for (U64 idx = 0; idx < text.size; ++idx) {
        if (text.str[idx] == '\n') { starts[line++] = LineStart{first + idx + 1}; }
    }
    buffer->line_starts.count += added_lines;

    return {
        .index         = BufferIndex::ModBuf,
        .first         = cursor_from_offset(buffer, first),
        .last          = cursor_from_offset(buffer, first + text.size),
        .length        = Length{text.size},
        .newline_count = LFCount{added_lines},
    };
}

void Tree::clear_history(UndoRedoList* list) {
    while (list->first != 0) {
        UndoRedoEntry* entry = list->first;
        SLLQueuePop(list->first, list->last);
        --list->count;
        entry->root.~RedBlackTree();
        SLLStackPush(m_free_undo_list, entry);
    }
}

void Tree::append_undo(const RedBlackTree& old_root, CharOffset op_offset) {
    UndoRedoEntry* entry = 0;
    if (m_free_undo_list != 0) {
        entry = m_free_undo_list;
        SLLStackPop(m_free_undo_list);
    } else {
        entry = static_cast<UndoRedoEntry*>(Arena::push(m_buffers.undo_redo_stack_arena,
                                                        sizeof(UndoRedoEntry),
                                                        alignof(UndoRedoEntry),
                                                        0));
        if (entry == 0) { throw std::bad_alloc(); }
    }
    ::new (entry) UndoRedoEntry{
        .next      = 0,
        .root      = old_root.dup(),
        .op_offset = op_offset,
    };
    SLLQueuePushFront(m_undo_stack.first, m_undo_stack.last, entry);
    ++m_undo_stack.count;
}

internal UndoRedoEntry* history_entry(Arena::Arena*       arena,
                                      UndoRedoEntry**     free_list,
                                      const RedBlackTree& root,
                                      CharOffset          offset) {
    UndoRedoEntry* entry = 0;
    if (*free_list != 0) {
        entry = *free_list;
        SLLStackPop(*free_list);
    } else {
        entry = static_cast<UndoRedoEntry*>(
            Arena::push(arena, sizeof(UndoRedoEntry), alignof(UndoRedoEntry), 0));
        if (entry == 0) { throw std::bad_alloc(); }
    }
    ::new (entry) UndoRedoEntry{
        .next      = 0,
        .root      = root.dup(),
        .op_offset = offset,
    };
    return entry;
}

void Tree::insert(CharOffset offset, String::String8 text, SuppressHistory suppress_history) {
    if (text.size == 0) { return; }
    SDL_assert(rep(offset) <= rep(length()));
    if (rep(offset) > rep(length())) { return; }

    B32 continues_insert = offset == m_end_last_insert;
    if (suppress_history == SuppressHistory::No) {
        clear_history(&m_redo_stack);
        if (!continues_insert) { append_undo(m_root, offset); }
    }
    internal_insert(offset, text);
    m_end_last_insert = CharOffset{rep(offset) + text.size};
}

void Tree::internal_insert(CharOffset offset, String::String8 text) {
    Piece new_piece = build_piece(text);
    U64   insert_at = rep(offset);

    if (insert_at > 0) {
        NodePosition previous       = node_at(CharOffset{insert_at - 1});
        Piece        previous_piece = previous.node->piece;
        U64          previous_end   = rep(previous.start_offset) + rep(previous_piece.length);
        if (previous_end == insert_at && previous_piece.index == BufferIndex::ModBuf &&
            previous_piece.last == new_piece.first) {
            Piece combined  = previous_piece;
            combined.last   = new_piece.last;
            combined.length = previous_piece.length + new_piece.length;
            combined.newline_count =
                LFCount{rep(previous_piece.newline_count) + rep(new_piece.newline_count)};
            RedBlackTree removed  = m_root.remove(m_buffers.rb_tree_blk, previous.start_offset);
            RedBlackTree inserted = removed.insert(m_buffers.rb_tree_blk,
                                                   NodeData{.piece = combined},
                                                   previous.start_offset);
            set_root(std::move(inserted));
            m_last_insert = combined.last;
            return;
        }
    }

    if (m_root.is_empty() || insert_at == rep(length())) {
        RedBlackTree inserted =
            m_root.insert(m_buffers.rb_tree_blk, NodeData{.piece = new_piece}, offset);
        set_root(std::move(inserted));
        m_last_insert = new_piece.last;
        return;
    }

    NodePosition position  = node_at(offset);
    Piece        existing  = position.node->piece;
    U64          remainder = rep(position.remainder);
    if (remainder == 0) {
        RedBlackTree inserted =
            m_root.insert(m_buffers.rb_tree_blk, NodeData{.piece = new_piece}, offset);
        set_root(std::move(inserted));
        m_last_insert = new_piece.last;
        return;
    }

    Piece        left    = piece_range(existing, 0, remainder);
    Piece        right   = piece_range(existing, remainder, rep(existing.length));
    RedBlackTree changed = m_root.remove(m_buffers.rb_tree_blk, position.start_offset);
    changed = changed.insert(m_buffers.rb_tree_blk, NodeData{.piece = left}, position.start_offset);
    changed = changed.insert(m_buffers.rb_tree_blk,
                             NodeData{.piece = new_piece},
                             CharOffset{rep(position.start_offset) + rep(left.length)});
    changed = changed.insert(
        m_buffers.rb_tree_blk,
        NodeData{.piece = right},
        CharOffset{rep(position.start_offset) + rep(left.length) + rep(new_piece.length)});
    set_root(std::move(changed));
    m_last_insert = new_piece.last;
}

void Tree::remove(CharOffset offset, Length count, SuppressHistory suppress_history) {
    if (rep(count) == 0 || is_empty()) { return; }
    SDL_assert(rep(offset) <= rep(length()) && rep(count) <= rep(length()) - rep(offset));
    if (rep(offset) > rep(length()) || rep(count) > rep(length()) - rep(offset)) { return; }

    if (suppress_history == SuppressHistory::No) {
        clear_history(&m_redo_stack);
        append_undo(m_root, offset);
    }
    internal_remove(offset, count);
    m_end_last_insert = CharOffset::Sentinel;
}

void Tree::internal_remove(CharOffset offset, Length count) {
    U64 remaining = rep(count);
    while (remaining > 0) {
        NodePosition position = node_at(offset);
        SDL_assert(position.node != 0);
        Piece piece      = position.node->piece;
        U64   first      = rep(position.remainder);
        U64   available  = rep(piece.length) - first;
        U64   remove_now = std::min(remaining, available);
        Piece left       = piece_range(piece, 0, first);
        Piece right      = piece_range(piece, first + remove_now, rep(piece.length));

        RedBlackTree changed = m_root.remove(m_buffers.rb_tree_blk, position.start_offset);
        if (rep(left.length) != 0) {
            changed = changed.insert(m_buffers.rb_tree_blk,
                                     NodeData{.piece = left},
                                     position.start_offset);
        }
        if (rep(right.length) != 0) {
            changed = changed.insert(m_buffers.rb_tree_blk,
                                     NodeData{.piece = right},
                                     CharOffset{rep(position.start_offset) + rep(left.length)});
        }
        set_root(std::move(changed));
        remaining -= remove_now;
    }
}

void Tree::commit_head(CharOffset offset) {
    clear_history(&m_redo_stack);
    append_undo(m_root, offset);
    m_end_last_insert = CharOffset::Sentinel;
}

RedBlackTree Tree::head() const {
    return m_root.dup();
}

void Tree::snap_to(const RedBlackTree& new_root) {
    set_root(new_root.dup());
    m_end_last_insert = CharOffset::Sentinel;
}

UndoRedoResult Tree::try_undo(CharOffset op_offset) {
    if (m_undo_stack.first == 0) { return {}; }
    UndoRedoEntry* undo = m_undo_stack.first;
    SLLQueuePop(m_undo_stack.first, m_undo_stack.last);
    --m_undo_stack.count;

    UndoRedoEntry* redo =
        history_entry(m_buffers.undo_redo_stack_arena, &m_free_undo_list, m_root, op_offset);
    SLLQueuePushFront(m_redo_stack.first, m_redo_stack.last, redo);
    ++m_redo_stack.count;

    CharOffset result_offset = undo->op_offset;
    set_root(std::move(undo->root));
    undo->root.~RedBlackTree();
    SLLStackPush(m_free_undo_list, undo);
    m_end_last_insert = CharOffset::Sentinel;
    return {.success = 1, .op_offset = result_offset};
}

UndoRedoResult Tree::try_redo(CharOffset op_offset) {
    if (m_redo_stack.first == 0) { return {}; }
    UndoRedoEntry* redo = m_redo_stack.first;
    SLLQueuePop(m_redo_stack.first, m_redo_stack.last);
    --m_redo_stack.count;

    UndoRedoEntry* undo =
        history_entry(m_buffers.undo_redo_stack_arena, &m_free_undo_list, m_root, op_offset);
    SLLQueuePushFront(m_undo_stack.first, m_undo_stack.last, undo);
    ++m_undo_stack.count;

    CharOffset result_offset = redo->op_offset;
    set_root(std::move(redo->root));
    redo->root.~RedBlackTree();
    SLLStackPush(m_free_undo_list, redo);
    m_end_last_insert = CharOffset::Sentinel;
    return {.success = 1, .op_offset = result_offset};
}

String::String8 Tree::assemble_range(Arena::Arena* arena, CharOffset first, CharOffset last) const {
    return assemble_tree_range(arena,
                               const_cast<BufferCollection*>(&m_buffers),
                               m_root.root_ptr(),
                               rep(first),
                               rep(last));
}

char Tree::at(CharOffset offset) const {
    SDL_assert(rep(offset) < rep(length()));
    return char_at_tree(const_cast<BufferCollection*>(&m_buffers), m_root.root_ptr(), rep(offset));
}

Line Tree::line_at(CharOffset offset) const {
    SDL_assert(rep(offset) <= rep(length()));
    return Line{
        line_at_offset(const_cast<BufferCollection*>(&m_buffers), m_root.root_ptr(), rep(offset))};
}

LineRange Tree::get_line_range(Line line) const {
    return tree_line_range(const_cast<BufferCollection*>(&m_buffers),
                           m_meta,
                           m_root.root_ptr(),
                           line,
                           0,
                           0);
}

LineRange Tree::get_line_range_crlf(Line line) const {
    return tree_line_range(const_cast<BufferCollection*>(&m_buffers),
                           m_meta,
                           m_root.root_ptr(),
                           line,
                           1,
                           0);
}

LineRange Tree::get_line_range_with_newline(Line line) const {
    return tree_line_range(const_cast<BufferCollection*>(&m_buffers),
                           m_meta,
                           m_root.root_ptr(),
                           line,
                           0,
                           1);
}

String::String8 Tree::get_line_content(Arena::Arena* arena, Line line) const {
    LineRange range = get_line_range(line);
    return assemble_range(arena, range.first, range.last);
}

IncompleteCRLF Tree::get_line_content_crlf(Arena::Arena*    arena,
                                           String::String8* buffer,
                                           Line             line) const {
    LineRange range = get_line_range_crlf(line);
    *buffer         = assemble_range(arena, range.first, range.last);
    U64 line_idx    = rep(line);
    if (line_idx >= rep(m_meta.lf_count)) { return IncompleteCRLF::No; }
    U64 next_start =
        line_offset(const_cast<BufferCollection*>(&m_buffers), m_root.root_ptr(), line_idx + 1);
    B32 has_cr = next_start >= 2 && at(CharOffset{next_start - 2}) == '\r';
    return has_cr ? IncompleteCRLF::No : IncompleteCRLF::Yes;
}

OwningSnapshot* Tree::owning_snap(Arena::Arena* arena) const {
    OwningSnapshot* memory = static_cast<OwningSnapshot*>(
        Arena::push(arena, sizeof(OwningSnapshot), alignof(OwningSnapshot), 0));
    if (memory == 0) { return 0; }
    return ::new (memory) OwningSnapshot(arena, this);
}

ReferenceSnapshot Tree::ref_snap() const {
    return ReferenceSnapshot(this);
}

OwningSnapshot::OwningSnapshot(Arena::Arena* arena, const Tree* tree) :
    OwningSnapshot(arena, tree, tree->m_root) {}

OwningSnapshot::OwningSnapshot(Arena::Arena* arena, const Tree* tree, const RedBlackTree& root) :
    m_root(root.dup()),
    m_meta(tree->m_meta),
    m_buffers(take_immutable_buffer_ref(&tree->m_buffers)) {
    (void)arena;
    U64 byte_reserve   = std::max(MB(1), AlignPow2(m_buffers.mod_buffer.buffer.size + 1, KB(64)));
    U64 starts_size    = m_buffers.mod_buffer.line_starts.count * sizeof(LineStart);
    U64 starts_reserve = std::max(MB(1), AlignPow2(starts_size + Arena::arena_header_size, KB(64)));
    m_owned_mut_buf_arena =
        Arena::allocate({.flags        = Arena::ArenaFlags_NoChain,
                         .reserve_size = byte_reserve + Arena::arena_header_size});
    m_owned_mut_starts_arena =
        Arena::allocate({.flags = Arena::ArenaFlags_NoChain, .reserve_size = starts_reserve});
    if (m_owned_mut_buf_arena == 0 || m_owned_mut_starts_arena == 0) {
        if (m_owned_mut_buf_arena != 0) { Arena::release(m_owned_mut_buf_arena); }
        if (m_owned_mut_starts_arena != 0) { Arena::release(m_owned_mut_starts_arena); }
        m_owned_mut_buf_arena = m_owned_mut_starts_arena = 0;
        m_root                                           = {};
        dec_buffer_ref(&m_buffers);
        throw std::bad_alloc();
    }

    String::String8 copied =
        String::str8_cstr_alloc(m_owned_mut_buf_arena, m_buffers.mod_buffer.buffer.size);
    if (copied.str == 0) {
        Arena::release(m_owned_mut_buf_arena);
        Arena::release(m_owned_mut_starts_arena);
        m_owned_mut_buf_arena = m_owned_mut_starts_arena = 0;
        m_root                                           = {};
        dec_buffer_ref(&m_buffers);
        throw std::bad_alloc();
    }
    if (copied.size != 0) { SDL_memcpy(copied.str, m_buffers.mod_buffer.buffer.str, copied.size); }
    LineStart* starts =
        Arena::push_array_no_zero<LineStart>(m_owned_mut_starts_arena,
                                             m_buffers.mod_buffer.line_starts.count);
    if (starts == 0) {
        Arena::release(m_owned_mut_buf_arena);
        Arena::release(m_owned_mut_starts_arena);
        m_owned_mut_buf_arena = m_owned_mut_starts_arena = 0;
        m_root                                           = {};
        dec_buffer_ref(&m_buffers);
        throw std::bad_alloc();
    }
    SDL_memcpy(starts, m_buffers.mod_buffer.line_starts.starts, starts_size);
    m_buffers.mut_buf_arena        = m_owned_mut_buf_arena;
    m_buffers.mut_buf_starts_arena = m_owned_mut_starts_arena;
    m_buffers.mod_buffer           = {
        .buffer = copied,
        .line_starts =
            {
                .starts = starts,
                .count  = m_buffers.mod_buffer.line_starts.count,
            },
    };
}

OwningSnapshot::~OwningSnapshot() {
    m_root = {};
    dec_buffer_ref(&m_buffers);
    if (m_owned_mut_buf_arena != 0) { Arena::release(m_owned_mut_buf_arena); }
    if (m_owned_mut_starts_arena != 0) { Arena::release(m_owned_mut_starts_arena); }
    m_owned_mut_buf_arena    = 0;
    m_owned_mut_starts_arena = 0;
}

void release_owning_snap(OwningSnapshot* snapshot) {
    if (snapshot != 0) { snapshot->~OwningSnapshot(); }
}

ReferenceSnapshot::ReferenceSnapshot(const Tree* tree) : ReferenceSnapshot(tree, tree->m_root) {}

ReferenceSnapshot::ReferenceSnapshot(const Tree* tree, const RedBlackTree& root) :
    m_root(root.dup()),
    m_meta(tree->m_meta),
    m_buffers(take_buffer_ref(const_cast<BufferCollection*>(&tree->m_buffers))) {}

ReferenceSnapshot::ReferenceSnapshot(const ReferenceSnapshot& other) :
    m_root(other.m_root.dup()),
    m_meta(other.m_meta),
    m_buffers(take_buffer_ref(const_cast<BufferCollection*>(&other.m_buffers))) {}

ReferenceSnapshot& ReferenceSnapshot::operator=(const ReferenceSnapshot& other) {
    if (this != &other) {
        m_root = {};
        dec_buffer_ref(&m_buffers);
        m_root    = other.m_root.dup();
        m_meta    = other.m_meta;
        m_buffers = take_buffer_ref(const_cast<BufferCollection*>(&other.m_buffers));
    }
    return *this;
}

ReferenceSnapshot::~ReferenceSnapshot() {
    m_root = {};
    dec_buffer_ref(&m_buffers);
}

internal String::String8 snapshot_line_content(Arena::Arena*     arena,
                                               BufferCollection* buffers,
                                               BufferMeta        meta,
                                               RBNodeCounted*    root,
                                               Line              line,
                                               B32               strip_cr) {
    LineRange range = tree_line_range(buffers, meta, root, line, strip_cr, 0);
    return assemble_tree_range(arena, buffers, root, rep(range.first), rep(range.last));
}

#define SNAPSHOT_QUERY_IMPL(Type)                                                                   \
    String::String8 Type::get_line_content(Arena::Arena* arena, Line line) const {                  \
        return snapshot_line_content(arena,                                                         \
                                     const_cast<BufferCollection*>(&m_buffers),                     \
                                     m_meta,                                                        \
                                     m_root.root_ptr(),                                             \
                                     line,                                                          \
                                     0);                                                            \
    }                                                                                               \
    IncompleteCRLF Type::get_line_content_crlf(Arena::Arena*    arena,                              \
                                               String::String8* buffer,                             \
                                               Line             line) const {                       \
        *buffer = snapshot_line_content(arena,                                                      \
                                        const_cast<BufferCollection*>(&m_buffers),                  \
                                        m_meta,                                                     \
                                        m_root.root_ptr(),                                          \
                                        line,                                                       \
                                        1);                                                         \
        if (rep(line) >= rep(m_meta.lf_count)) { return IncompleteCRLF::No; }                       \
        U64 next_start = line_offset(const_cast<BufferCollection*>(&m_buffers),                     \
                                     m_root.root_ptr(),                                             \
                                     rep(line) + 1);                                                \
        B32 has_cr     = next_start >= 2 && char_at_tree(const_cast<BufferCollection*>(&m_buffers), \
                                                         m_root.root_ptr(),                         \
                                                         next_start - 2) == '\r';                   \
        return has_cr ? IncompleteCRLF::No : IncompleteCRLF::Yes;                                   \
    }                                                                                               \
    char Type::at(CharOffset offset) const {                                                        \
        SDL_assert(rep(offset) < rep(m_meta.total_content_length));                                 \
        return char_at_tree(const_cast<BufferCollection*>(&m_buffers),                              \
                            m_root.root_ptr(),                                                      \
                            rep(offset));                                                           \
    }                                                                                               \
    Line Type::line_at(CharOffset offset) const {                                                   \
        SDL_assert(rep(offset) <= rep(m_meta.total_content_length));                                \
        return Line{line_at_offset(const_cast<BufferCollection*>(&m_buffers),                       \
                                   m_root.root_ptr(),                                               \
                                   rep(offset))};                                                   \
    }                                                                                               \
    LineRange Type::get_line_range(Line line) const {                                               \
        return tree_line_range(const_cast<BufferCollection*>(&m_buffers),                           \
                               m_meta,                                                              \
                               m_root.root_ptr(),                                                   \
                               line,                                                                \
                               0,                                                                   \
                               0);                                                                  \
    }                                                                                               \
    LineRange Type::get_line_range_crlf(Line line) const {                                          \
        return tree_line_range(const_cast<BufferCollection*>(&m_buffers),                           \
                               m_meta,                                                              \
                               m_root.root_ptr(),                                                   \
                               line,                                                                \
                               1,                                                                   \
                               0);                                                                  \
    }                                                                                               \
    LineRange Type::get_line_range_with_newline(Line line) const {                                  \
        return tree_line_range(const_cast<BufferCollection*>(&m_buffers),                           \
                               m_meta,                                                              \
                               m_root.root_ptr(),                                                   \
                               line,                                                                \
                               0,                                                                   \
                               1);                                                                  \
    }

SNAPSHOT_QUERY_IMPL(OwningSnapshot)
SNAPSHOT_QUERY_IMPL(ReferenceSnapshot)
#undef SNAPSHOT_QUERY_IMPL

internal RBNodeCounted* node_pointer_at(RBNodeCounted* root, U64 offset, U64* remainder) {
    RBNodeCounted* node = root;
    while (node != 0) {
        U64 left_length  = node_length(node->left);
        U64 piece_length = rep(node->data.piece.length);
        if (offset < left_length) {
            node = node->left;
        } else if (offset < left_length + piece_length) {
            *remainder = offset - left_length;
            return node;
        } else {
            offset -= left_length + piece_length;
            node = node->right;
        }
    }
    *remainder = 0;
    return 0;
}

internal void walker_stack_clear(StackList* stack) {
    while (stack->stack != 0) {
        StackEntry* entry = stack->stack;
        SLLStackPop(stack->stack);
        SLLStackPush(stack->free_list, entry);
    }
}

internal void walker_stack_push(Arena::Arena* arena, StackList* stack, RBNodeCounted* node) {
    StackEntry* entry = 0;
    if (stack->free_list != 0) {
        entry = stack->free_list;
        SLLStackPop(stack->free_list);
    } else {
        entry = Arena::push_array<StackEntry>(arena, 1);
        if (entry == 0) { throw std::bad_alloc(); }
    }
    entry->node      = node;
    entry->direction = Direction::Center;
    SLLStackPush(stack->stack, entry);
}

internal RBNodeCounted* walker_stack_pop(StackList* stack) {
    StackEntry* entry = stack->stack;
    if (entry == 0) { return 0; }
    RBNodeCounted* node = entry->node;
    SLLStackPop(stack->stack);
    SLLStackPush(stack->free_list, entry);
    return node;
}

TreeWalker::TreeWalker(Arena::Arena* arena, const Tree* tree, CharOffset offset) :
    TreeWalker(arena,
               const_cast<BufferCollection*>(&tree->m_buffers),
               tree->m_meta,
               tree->m_root,
               offset) {}

TreeWalker::TreeWalker(Arena::Arena* arena, const OwningSnapshot* snapshot, CharOffset offset) :
    TreeWalker(arena,
               const_cast<BufferCollection*>(&snapshot->m_buffers),
               snapshot->m_meta,
               snapshot->m_root,
               offset) {}

TreeWalker::TreeWalker(Arena::Arena* arena, const ReferenceSnapshot* snapshot, CharOffset offset) :
    TreeWalker(arena,
               const_cast<BufferCollection*>(&snapshot->m_buffers),
               snapshot->m_meta,
               snapshot->m_root,
               offset) {}

TreeWalker::TreeWalker(Arena::Arena*       arena,
                       BufferCollection*   buffers,
                       BufferMeta          meta,
                       const RedBlackTree& root,
                       CharOffset          offset) :
    m_buffers(buffers), m_root(root.dup()), m_meta(meta), m_arena(arena), m_offset(offset) {
    seek(offset);
}

void TreeWalker::seek(CharOffset offset) {
    SDL_assert(rep(offset) <= rep(m_meta.total_content_length));
    walker_stack_clear(&m_stack);
    m_offset   = offset;
    U64 target = rep(offset);
    m_node     = m_root.root_ptr();
    while (m_node != 0) {
        walker_stack_push(m_arena, &m_stack, m_node);
        U64 left_length  = node_length(m_node->left);
        U64 piece_length = rep(m_node->data.piece.length);
        if (target < left_length) {
            m_node = m_node->left;
        } else if (target < left_length + piece_length) {
            m_piece_offset = target - left_length;
            return;
        } else {
            target -= left_length + piece_length;
            m_node = m_node->right;
        }
    }
    m_piece_offset = 0;
    walker_stack_clear(&m_stack);
}

B32 TreeWalker::exhausted() const {
    return m_node == 0;
}

Length TreeWalker::remaining() const {
    if (exhausted()) { return {}; }
    return Length{rep(m_meta.total_content_length) - rep(m_offset)};
}

char TreeWalker::current() {
    SDL_assert(!exhausted());
    Piece       piece       = m_node->data.piece;
    CharBuffer* buffer      = m_buffers->buffer_at(piece.index);
    U64         byte_offset = cursor_offset(buffer, piece.first) + m_piece_offset;
    return buffer->buffer.str[byte_offset];
}

char TreeWalker::next() {
    char result = current();
    ++m_piece_offset;
    m_offset = CharOffset{rep(m_offset) + 1};
    if (m_piece_offset == rep(m_node->data.piece.length)) {
        RBNodeCounted* child = m_stack.stack->node;
        SDL_assert(child == m_node);
        if (child->right != 0) {
            RBNodeCounted* node = child->right;
            while (node != 0) {
                walker_stack_push(m_arena, &m_stack, node);
                if (node->left == 0) { break; }
                node = node->left;
            }
            m_node = node;
        } else {
            walker_stack_pop(&m_stack);
            m_node = 0;
            while (m_stack.stack != 0) {
                RBNodeCounted* parent = m_stack.stack->node;
                if (parent->left == child) {
                    m_node = parent;
                    break;
                }
                child = walker_stack_pop(&m_stack);
            }
        }
        m_piece_offset = 0;
    }
    return result;
}

ReverseTreeWalker::ReverseTreeWalker(Arena::Arena* arena, const Tree* tree, CharOffset offset) :
    m_buffers(const_cast<BufferCollection*>(&tree->m_buffers)),
    m_root(tree->m_root.dup()),
    m_meta(tree->m_meta),
    m_arena(arena) {
    seek(offset);
}

ReverseTreeWalker::ReverseTreeWalker(Arena::Arena*         arena,
                                     const OwningSnapshot* snapshot,
                                     CharOffset            offset) :
    m_buffers(const_cast<BufferCollection*>(&snapshot->m_buffers)),
    m_root(snapshot->m_root.dup()),
    m_meta(snapshot->m_meta),
    m_arena(arena) {
    seek(offset);
}

ReverseTreeWalker::ReverseTreeWalker(Arena::Arena*            arena,
                                     const ReferenceSnapshot* snapshot,
                                     CharOffset               offset) :
    m_buffers(const_cast<BufferCollection*>(&snapshot->m_buffers)),
    m_root(snapshot->m_root.dup()),
    m_meta(snapshot->m_meta),
    m_arena(arena) {
    seek(offset);
}

void ReverseTreeWalker::seek(CharOffset offset) {
    walker_stack_clear(&m_stack);
    if (rep(m_meta.total_content_length) == 0) {
        m_node         = 0;
        m_piece_offset = 0;
        m_offset       = {};
        return;
    }
    SDL_assert(rep(offset) < rep(m_meta.total_content_length));
    m_offset   = offset;
    U64 target = rep(offset);
    m_node     = m_root.root_ptr();
    while (m_node != 0) {
        walker_stack_push(m_arena, &m_stack, m_node);
        U64 left_length  = node_length(m_node->left);
        U64 piece_length = rep(m_node->data.piece.length);
        if (target < left_length) {
            m_node = m_node->left;
        } else if (target < left_length + piece_length) {
            m_piece_offset = target - left_length;
            return;
        } else {
            target -= left_length + piece_length;
            m_node = m_node->right;
        }
    }
    m_piece_offset = 0;
    walker_stack_clear(&m_stack);
}

B32 ReverseTreeWalker::exhausted() const {
    return m_node == 0;
}

Length ReverseTreeWalker::remaining() const {
    if (exhausted()) { return {}; }
    return Length{rep(m_offset) + 1};
}

char ReverseTreeWalker::current() {
    SDL_assert(!exhausted());
    Piece       piece       = m_node->data.piece;
    CharBuffer* buffer      = m_buffers->buffer_at(piece.index);
    U64         byte_offset = cursor_offset(buffer, piece.first) + m_piece_offset;
    return buffer->buffer.str[byte_offset];
}

char ReverseTreeWalker::next() {
    char result = current();
    if (rep(m_offset) == 0) {
        m_node         = 0;
        m_piece_offset = 0;
        m_offset       = CharOffset::Sentinel;
        return result;
    }
    m_offset = CharOffset{rep(m_offset) - 1};
    if (m_piece_offset > 0) {
        --m_piece_offset;
    } else {
        RBNodeCounted* child = m_stack.stack->node;
        SDL_assert(child == m_node);
        if (child->left != 0) {
            RBNodeCounted* node = child->left;
            while (node != 0) {
                walker_stack_push(m_arena, &m_stack, node);
                if (node->right == 0) { break; }
                node = node->right;
            }
            m_node         = node;
            m_piece_offset = rep(node->data.piece.length) - 1;
        } else {
            walker_stack_pop(&m_stack);
            m_node = 0;
            while (m_stack.stack != 0) {
                RBNodeCounted* parent = m_stack.stack->node;
                if (parent->right == child) {
                    m_node = parent;
                    break;
                }
                child = walker_stack_pop(&m_stack);
            }
            if (m_node != 0) { m_piece_offset = rep(m_node->data.piece.length) - 1; }
        }
    }
    return result;
}

SelectionNode* push_selection(Arena::Arena* arena, SelectionList* list, Selection selection) {
    SelectionNode* node = Arena::push_array<SelectionNode>(arena, 1);
    if (node == 0) { return 0; }
    node->selection = selection;
    SLLQueuePush(list->first, list->last, node);
    ++list->count;
    return node;
}

void pop_selection(SelectionList* list) {
    if (list == 0 || list->first == 0) { return; }
    if (list->first == list->last) {
        list->first = list->last = 0;
        list->count              = 0;
        return;
    }
    SelectionNode* previous = list->first;
    while (previous->next != list->last) { previous = previous->next; }
    previous->next = 0;
    list->last     = previous;
    --list->count;
}

}  // namespace ETide::PieceTree
