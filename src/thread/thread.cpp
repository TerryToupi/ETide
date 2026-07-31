namespace ETide::Thread {

struct Barrier {
    SDL_Mutex*     mutex;
    SDL_Condition* condition;
    U32            thread_count;
    U32            waiting_count;
    U32            generation;
};

internal Mutex* mutex_create() {
    return reinterpret_cast<Mutex*>(SDL_CreateMutex());
}

internal void mutex_destroy(Mutex* mutex) {
    SDL_DestroyMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal void mutex_lock(Mutex* mutex) {
    SDL_LockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal B32 mutex_try_lock(Mutex* mutex) {
    return SDL_TryLockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal void mutex_unlock(Mutex* mutex) {
    SDL_UnlockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

internal RWLock* rwlock_create() {
    return reinterpret_cast<RWLock*>(SDL_CreateRWLock());
}

internal void rwlock_destroy(RWLock* lock) {
    SDL_DestroyRWLock(reinterpret_cast<SDL_RWLock*>(lock));
}

internal void rwlock_read_lock(RWLock* lock) {
    SDL_LockRWLockForReading(reinterpret_cast<SDL_RWLock*>(lock));
}

internal void rwlock_write_lock(RWLock* lock) {
    SDL_LockRWLockForWriting(reinterpret_cast<SDL_RWLock*>(lock));
}

internal B32 rwlock_try_read_lock(RWLock* lock) {
    return SDL_TryLockRWLockForReading(reinterpret_cast<SDL_RWLock*>(lock));
}

internal B32 rwlock_try_write_lock(RWLock* lock) {
    return SDL_TryLockRWLockForWriting(reinterpret_cast<SDL_RWLock*>(lock));
}

internal void rwlock_unlock(RWLock* lock) {
    SDL_UnlockRWLock(reinterpret_cast<SDL_RWLock*>(lock));
}

internal Semaphore* semaphore_create(U32 initial_value) {
    return reinterpret_cast<Semaphore*>(SDL_CreateSemaphore(initial_value));
}

internal void semaphore_destroy(Semaphore* semaphore) {
    SDL_DestroySemaphore(reinterpret_cast<SDL_Semaphore*>(semaphore));
}

internal void semaphore_wait(Semaphore* semaphore) {
    SDL_WaitSemaphore(reinterpret_cast<SDL_Semaphore*>(semaphore));
}

internal B32 semaphore_try_wait(Semaphore* semaphore) {
    return SDL_TryWaitSemaphore(reinterpret_cast<SDL_Semaphore*>(semaphore));
}

internal B32 semaphore_wait_timeout(Semaphore* semaphore, I32 timeout_ms) {
    return SDL_WaitSemaphoreTimeout(reinterpret_cast<SDL_Semaphore*>(semaphore), timeout_ms);
}

internal void semaphore_signal(Semaphore* semaphore) {
    SDL_SignalSemaphore(reinterpret_cast<SDL_Semaphore*>(semaphore));
}

internal U32 semaphore_value(Semaphore* semaphore) {
    return SDL_GetSemaphoreValue(reinterpret_cast<SDL_Semaphore*>(semaphore));
}

internal ConditionVariable* condition_variable_create() {
    return reinterpret_cast<ConditionVariable*>(SDL_CreateCondition());
}

internal void condition_variable_destroy(ConditionVariable* condition) {
    SDL_DestroyCondition(reinterpret_cast<SDL_Condition*>(condition));
}

internal void condition_variable_wait(ConditionVariable* condition, Mutex* mutex) {
    SDL_WaitCondition(reinterpret_cast<SDL_Condition*>(condition),
                      reinterpret_cast<SDL_Mutex*>(mutex));
}

internal B32 condition_variable_wait_timeout(ConditionVariable* condition,
                                             Mutex*             mutex,
                                             I32                timeout_ms) {
    return SDL_WaitConditionTimeout(reinterpret_cast<SDL_Condition*>(condition),
                                    reinterpret_cast<SDL_Mutex*>(mutex),
                                    timeout_ms);
}

internal void condition_variable_signal(ConditionVariable* condition) {
    SDL_SignalCondition(reinterpret_cast<SDL_Condition*>(condition));
}

internal void condition_variable_broadcast(ConditionVariable* condition) {
    SDL_BroadcastCondition(reinterpret_cast<SDL_Condition*>(condition));
}

internal Barrier* barrier_create(U32 thread_count) {
    if (thread_count == 0) {
        SDL_SetError("Barrier thread count must be greater than zero");
        return 0;
    }

    Barrier* result = static_cast<Barrier*>(SDL_calloc(1, sizeof(Barrier)));
    if (result == 0) { return 0; }

    result->mutex = SDL_CreateMutex();
    if (result->mutex == 0) {
        SDL_free(result);
        return 0;
    }

    result->condition = SDL_CreateCondition();
    if (result->condition == 0) {
        SDL_DestroyMutex(result->mutex);
        SDL_free(result);
        return 0;
    }

    result->thread_count = thread_count;
    return result;
}

internal void barrier_destroy(Barrier* barrier) {
    if (barrier == 0) { return; }
    SDL_DestroyCondition(barrier->condition);
    SDL_DestroyMutex(barrier->mutex);
    SDL_free(barrier);
}

internal B32 barrier_wait(Barrier* barrier) {
    SDL_LockMutex(barrier->mutex);

    U32 generation = barrier->generation;
    ++barrier->waiting_count;

    B32 last_thread = barrier->waiting_count == barrier->thread_count;
    if (last_thread) {
        barrier->waiting_count = 0;
        ++barrier->generation;
        SDL_BroadcastCondition(barrier->condition);
    } else {
        while (generation == barrier->generation) {
            SDL_WaitCondition(barrier->condition, barrier->mutex);
        }
    }

    SDL_UnlockMutex(barrier->mutex);
    return last_thread;
}

}  // namespace ETide::Thread
