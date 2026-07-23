#include <SDL3/SDL.h>
#include <core/core.hpp>

#if SDL_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/mman.h>
#endif

namespace ETide::Memory {

class DefaultAllocator final : public Allocator {
   public:
    DefaultAllocator()           = default;
    ~DefaultAllocator() override = default;

    virtual void* reserve(U64 size) override;
    virtual B32   commit(void* ptr, U64 size) override;
    virtual void  decomit(void* ptr, U64 size) override;
    virtual void  release(void* ptr, U64 size) override;
};

void* DefaultAllocator::reserve(U64 size) {
    void* result = 0;
#if SDL_PLATFORM_WINDOWS
    result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
#else
    result = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED) { result = 0; }
#endif
    return result;
}

B32 DefaultAllocator::commit(void* ptr, U64 size) {
    B32 result = 0;
#if SDL_PLATFORM_WINDOWS
    result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);
#else
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
    result = 1;
#endif
    return result;
}

void DefaultAllocator::decomit(void* ptr, U64 size) {
#if SDL_PLATFORM_WINDOWS
    VirtualFree(ptr, size, MEM_DECOMMIT);
#else
    madvise(ptr, size, MADV_DONTNEED);
    mprotect(ptr, size, PROT_NONE);
#endif
}

void DefaultAllocator::release(void* ptr, U64 size) {
#if SDL_PLATFORM_WINDOWS
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

static DefaultAllocator default_allocator = {};

}  // namespace ETide::Memory