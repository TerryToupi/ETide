#pragma once

namespace ETide {

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

typedef uint8_t  U8;
typedef int8_t   I8;
typedef int8_t   B8;
typedef uint16_t U16;
typedef int16_t  I16;
typedef int16_t  B16;
typedef uint32_t U32;
typedef int32_t  I32;
typedef int32_t  B32;
typedef uint64_t U64;
typedef int64_t  I64;
typedef int64_t  B64;
typedef float    F32;
typedef double   F64;

}  // namespace ETide

namespace ETide::Memory {

class Allocator {
   public:
    virtual ~Allocator() = default;

    Allocator(const Allocator&)            = delete;
    Allocator& operator=(const Allocator&) = delete;

    virtual void* reserve(U64 size)            = 0;
    virtual B32   commit(void* ptr, U64 size)  = 0;
    virtual void  decomit(void* ptr, U64 size) = 0;
    virtual void  release(void* ptr, U64 size) = 0;

   protected:
    Allocator() = default;
};

}  // namespace ETide::Memory
