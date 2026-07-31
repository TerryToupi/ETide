#ifndef CONTAINERS_HPP_
#define CONTAINERS_HPP_

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
        m_lock = Thread::rwlock_create();
        if (m_lock == 0) {
            m_allocator->release(m_base, m_aligned_reservation_size);
            m_base                     = 0;
            m_aligned_reservation_size = 0;
            throw std::bad_alloc();
        }
    }
    ~Pool() {
        release_storage();
        Thread::rwlock_destroy(m_lock);
        m_lock = 0;
    }

    Pool(const Pool&)            = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&)                 = delete;
    Pool& operator=(Pool&&)      = delete;

    void clear() {
        Thread::rwlock_write_lock(m_lock);

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
        Thread::rwlock_unlock(m_lock);
    }

    template <typename... Args>
    handle_type emplace(Args&&... args) {
        Thread::rwlock_write_lock(m_lock);
        if (m_head == kEndOfList) {
            if (m_used_segments == kMaxSegments) {
                Thread::rwlock_unlock(m_lock);
                throw std::length_error("Pool reached maximum size");
            }
            if (!add_segment()) {
                Thread::rwlock_unlock(m_lock);
                throw std::bad_alloc();
            }
        }

        U32    idx   = m_head;
        Entry* entry = get(idx);
        if (entry->next == kNotInFreelist) {
            Thread::rwlock_unlock(m_lock);
            throw std::runtime_error("Pool freelist is corrupted");
        }

        try {
            std::construct_at(entry->storage_ptr(), std::forward<Args>(args)...);
        } catch (...) {
            Thread::rwlock_unlock(m_lock);
            throw;
        }

        m_head              = entry->next;
        entry->next         = kNotInFreelist;
        m_latest_generation = next_generation(m_latest_generation);
        entry->generation   = m_latest_generation;
        ++m_count;
        handle_type handle = create_handle(idx, entry->generation);
        Thread::rwlock_unlock(m_lock);
        return handle;
    }

    void erase(handle_type handle) {
        Thread::rwlock_write_lock(m_lock);
        DecomposedHandle decomposed = decompose_handle(handle);
        Entry*           entry      = get_if_valid(decomposed);
        if (entry == 0) {
            Thread::rwlock_unlock(m_lock);
            throw std::out_of_range("Pool handle is invalid or stale");
        }

        entry->next = m_head;
        m_head      = decomposed.idx;
        --m_count;
        Thread::rwlock_unlock(m_lock);
    }

    reference operator[](handle_type handle) {
        Thread::rwlock_read_lock(m_lock);
        Entry* entry = get_if_valid(decompose_handle(handle));
        if (entry == 0) {
            Thread::rwlock_unlock(m_lock);
            throw std::out_of_range("Pool handle is invalid or stale");
        }
        T* value = entry->value();
        Thread::rwlock_unlock(m_lock);
        return *value;
    }
    const_reference operator[](handle_type handle) const {
        Thread::rwlock_read_lock(m_lock);
        const Entry* entry = get_if_valid(decompose_handle(handle));
        if (entry == 0) {
            Thread::rwlock_unlock(m_lock);
            throw std::out_of_range("Pool handle is invalid or stale");
        }
        const T* value = entry->value();
        Thread::rwlock_unlock(m_lock);
        return *value;
    }

    reference       at(handle_type handle) { return (*this)[handle]; }
    const_reference at(handle_type handle) const { return (*this)[handle]; }

    B32 contains(handle_type handle) const {
        Thread::rwlock_read_lock(m_lock);
        B32 result = get_if_valid(decompose_handle(handle)) != 0;
        Thread::rwlock_unlock(m_lock);
        return result;
    }

    B32 empty() const {
        Thread::rwlock_read_lock(m_lock);
        B32 result = m_count == 0;
        Thread::rwlock_unlock(m_lock);
        return result;
    }
    U32 size() const {
        Thread::rwlock_read_lock(m_lock);
        U32 result = m_count;
        Thread::rwlock_unlock(m_lock);
        return result;
    }
    U32 capacity() const {
        Thread::rwlock_read_lock(m_lock);
        U32 result = m_capacity;
        Thread::rwlock_unlock(m_lock);
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

    mutable Thread::RWLock* m_lock                     = 0;
    Memory::Allocator*      m_allocator                = &Memory::default_allocator;
    void*                   m_base                     = 0;
    U64                     m_aligned_reservation_size = 0;
    U32                     m_capacity                 = 0;
    U32                     m_used_segments            = 0;
    U32                     m_count                    = 0;
    U32                     m_head                     = kEndOfList;
    U32                     m_latest_generation        = 0;
    Entry*                  m_segments[kMaxSegments]   = {0};
};

}  // namespace ETide::Containers

#endif
