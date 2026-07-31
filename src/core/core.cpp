#if SDL_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/mman.h>
#endif

#if defined(_MSC_VER)
#    include <intrin.h>
#endif

namespace ETide {

internal DateTime date_time_from_sdl_date_time(SDL_DateTime date_time) {
    DateTime result = {};

    result.micro_sec = static_cast<U16>((date_time.nanosecond / SDL_NS_PER_US) % 1000);
    result.msec      = static_cast<U16>(date_time.nanosecond / SDL_NS_PER_MS);
    result.sec       = static_cast<U16>(date_time.second);
    result.min       = static_cast<U16>(date_time.minute);
    result.hour      = static_cast<U16>(date_time.hour);
    result.day       = static_cast<U16>(date_time.day - 1);
    result.week_day  = static_cast<WeekDay>(date_time.day_of_week);
    result.month     = static_cast<Month>(date_time.month - 1);
    result.year      = static_cast<U32>(date_time.year);

    return result;
}

internal B32 sdl_date_time_from_date_time(DateTime date_time, SDL_DateTime* result) {
    if (result == 0) {
        SDL_SetError("SDL_DateTime result must not be NULL");
        return 0;
    }
    if (date_time.year > SDL_MAX_SINT32 || date_time.mon >= Month_COUNT || date_time.day >= 31 ||
        date_time.hour >= 24 || date_time.min >= 60 || date_time.sec > 60 ||
        date_time.msec >= 1000 || date_time.micro_sec >= 1000) {
        SDL_SetError("Invalid ETide DateTime");
        return 0;
    }

    *result = {
        .year        = static_cast<I32>(date_time.year),
        .month       = static_cast<I32>(date_time.mon + 1),
        .day         = static_cast<I32>(date_time.day + 1),
        .hour        = static_cast<I32>(date_time.hour),
        .minute      = static_cast<I32>(date_time.min),
        .second      = static_cast<I32>(date_time.sec),
        .nanosecond  = static_cast<I32>(date_time.msec * SDL_NS_PER_MS +
                                       date_time.micro_sec * SDL_NS_PER_US),
        .day_of_week = static_cast<I32>(date_time.wday),
        .utc_offset  = 0,
    };
    return 1;
}

internal DenseTime dense_time_from_date_time(DateTime date_time) {
    DenseTime result = 0;
    result += date_time.year;
    result *= 12;
    result += date_time.mon;
    result *= 31;
    result += date_time.day;
    result *= 24;
    result += date_time.hour;
    result *= 60;
    result += date_time.min;
    result *= 61;
    result += date_time.sec;
    result *= 1000;
    result += date_time.msec;
    return result;
}

internal DateTime date_time_from_dense_time(DenseTime time) {
    DateTime result = {};
    result.msec     = static_cast<U16>(time % 1000);
    time /= 1000;
    result.sec = static_cast<U16>(time % 61);
    time /= 61;
    result.min = static_cast<U16>(time % 60);
    time /= 60;
    result.hour = static_cast<U16>(time % 24);
    time /= 24;
    result.day = static_cast<U16>(time % 31);
    time /= 31;
    result.mon = static_cast<U32>(time % 12);
    time /= 12;

    SDL_assert(time <= SDL_MAX_UINT32);
    if (time > SDL_MAX_UINT32) {
        SDL_SetError("DenseTime year exceeds DateTime range");
        return {};
    }

    result.year = static_cast<U32>(time);
    return result;
}

internal DateTime date_time_from_micro_seconds(U64 time) {
    DateTime result   = {};
    result.micro_sec = static_cast<U16>(time % 1000);
    time /= 1000;
    result.msec = static_cast<U16>(time % 1000);
    time /= 1000;
    result.sec = static_cast<U16>(time % 60);
    time /= 60;
    result.min = static_cast<U16>(time % 60);
    time /= 60;
    result.hour = static_cast<U16>(time % 24);
    time /= 24;
    result.day = static_cast<U16>(time % 31);
    time /= 31;
    result.mon = static_cast<U32>(time % 12);
    time /= 12;

    SDL_assert(time <= SDL_MAX_UINT32);
    if (time > SDL_MAX_UINT32) {
        SDL_SetError("Microsecond time year exceeds DateTime range");
        return {};
    }

    result.year = static_cast<U32>(time);
    return result;
}

internal DateTime date_time_from_unix_time(U64 unix_time) {
    DateTime result = {};
    if (unix_time > static_cast<U64>(SDL_MAX_TIME / SDL_NS_PER_SECOND)) {
        SDL_SetError("Unix time exceeds SDL_Time range");
        return result;
    }

    SDL_Time     time = static_cast<SDL_Time>(unix_time * SDL_NS_PER_SECOND);
    SDL_DateTime date_time;
    B32          succeeded = SDL_TimeToDateTime(time, &date_time, false);
    SDL_assert(succeeded);
    if (succeeded) { result = date_time_from_sdl_date_time(date_time); }
    return result;
}

internal U64 now_time_us(void) {
    SDL_Time time      = 0;
    B32      succeeded = SDL_GetCurrentTime(&time);
    SDL_assert(succeeded);
    if (!succeeded || time < 0) { return 0; }
    return static_cast<U64>(time / SDL_NS_PER_US);
}

internal DateTime now_time_universal(void) {
    DateTime result = {};
    SDL_Time time   = 0;

    B32 succeeded = SDL_GetCurrentTime(&time);
    SDL_assert(succeeded);
    if (!succeeded) { return result; }

    SDL_DateTime date_time;
    succeeded = SDL_TimeToDateTime(time, &date_time, false);
    SDL_assert(succeeded);
    if (succeeded) { result = date_time_from_sdl_date_time(date_time); }
    return result;
}

internal DateTime universal_from_local_time(DateTime* dt) {
    DateTime result = {};
    if (dt == 0) {
        SDL_SetError("DateTime must not be NULL");
        return result;
    }

    SDL_DateTime local_time;
    if (!sdl_date_time_from_date_time(*dt, &local_time)) { return result; }

    SDL_Time     time       = 0;
    SDL_DateTime time_probe = {};
    I32          utc_offset = 0;

    // SDL needs an offset to encode local calendar time, so converge on the offset it reports.
    for (U32 iteration = 0; iteration < 3; ++iteration) {
        local_time.utc_offset = utc_offset;
        if (!SDL_DateTimeToTime(&local_time, &time) ||
            !SDL_TimeToDateTime(time, &time_probe, true)) {
            return result;
        }
        if (time_probe.utc_offset == utc_offset) { break; }
        utc_offset = time_probe.utc_offset;
    }

    SDL_DateTime universal_time;
    B32          succeeded = SDL_TimeToDateTime(time, &universal_time, false);
    SDL_assert(succeeded);
    if (succeeded) { result = date_time_from_sdl_date_time(universal_time); }
    return result;
}

internal DateTime local_from_universal_time(DateTime* dt) {
    DateTime result = {};
    if (dt == 0) {
        SDL_SetError("DateTime must not be NULL");
        return result;
    }

    SDL_DateTime universal_time;
    if (!sdl_date_time_from_date_time(*dt, &universal_time)) { return result; }

    SDL_Time time = 0;
    if (!SDL_DateTimeToTime(&universal_time, &time)) { return result; }

    SDL_DateTime local_time;
    B32          succeeded = SDL_TimeToDateTime(time, &local_time, true);
    SDL_assert(succeeded);
    if (succeeded) { result = date_time_from_sdl_date_time(local_time); }
    return result;
}

internal void sleep_ms(U32 ms) {
    SDL_Delay(ms);
}

internal U64 ctz32(U32 val) {
    if (val == 0) { return 32; }

#if defined(_MSC_VER)
    unsigned long idx = 0;
    _BitScanForward(&idx, val);
    return idx;
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_ctz(val));
#else
    U64 result = 0;
    while ((val & 1) == 0) {
        val >>= 1;
        ++result;
    }
    return result;
#endif
}

internal U64 ctz64(U64 val) {
    if (val == 0) { return 64; }

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64) || defined(_M_ARM64EC))
    unsigned long idx = 0;
    _BitScanForward64(&idx, val);
    return idx;
#elif defined(_MSC_VER)
    U32 low = static_cast<U32>(val);
    return low != 0 ? ctz32(low) : 32 + ctz32(static_cast<U32>(val >> 32));
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_ctzll(val));
#else
    U64 result = 0;
    while ((val & 1) == 0) {
        val >>= 1;
        ++result;
    }
    return result;
#endif
}

internal U64 clz32(U32 val) {
    if (val == 0) { return 32; }

#if defined(_MSC_VER)
    unsigned long idx = 0;
    _BitScanReverse(&idx, val);
    return 31 - idx;
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_clz(val));
#else
    U64 result = 0;
    U32 bit    = U32{1} << 31;
    while ((val & bit) == 0) {
        bit >>= 1;
        ++result;
    }
    return result;
#endif
}

internal U64 clz64(U64 val) {
    if (val == 0) { return 64; }

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64) || defined(_M_ARM64EC))
    unsigned long idx = 0;
    _BitScanReverse64(&idx, val);
    return 63 - idx;
#elif defined(_MSC_VER)
    U32 high = static_cast<U32>(val >> 32);
    return high != 0 ? clz32(high) : 32 + clz32(static_cast<U32>(val));
#elif defined(__clang__) || defined(__GNUC__)
    return static_cast<U64>(__builtin_clzll(val));
#else
    U64 result = 0;
    U64 bit    = U64{1} << 63;
    while ((val & bit) == 0) {
        bit >>= 1;
        ++result;
    }
    return result;
#endif
}

internal U64 page_size() {
    U64 result = static_cast<U64>(SDL_GetSystemPageSize());
    if (result == 0) { result = KB(4); }
    return result;
}

}  // namespace ETide

namespace ETide::Memory {

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
    result = (mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0);
#endif
    return result;
}

void DefaultAllocator::decommit(void* ptr, U64 size) {
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

}  // namespace ETide::Memory
