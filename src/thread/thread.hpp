#ifndef THREAD_HPP_
#define THREAD_HPP_

namespace ETide::Thread {

typedef struct Mutex             Mutex;
typedef struct RWLock            RWLock;
typedef struct Semaphore         Semaphore;
typedef struct ConditionVariable ConditionVariable;
typedef struct Barrier           Barrier;

internal Mutex* mutex_create();
internal void   mutex_destroy(Mutex* mutex);
internal void   mutex_lock(Mutex* mutex);
internal B32    mutex_try_lock(Mutex* mutex);
internal void   mutex_unlock(Mutex* mutex);

internal RWLock* rwlock_create();
internal void    rwlock_destroy(RWLock* lock);
internal void    rwlock_read_lock(RWLock* lock);
internal void    rwlock_write_lock(RWLock* lock);
internal B32     rwlock_try_read_lock(RWLock* lock);
internal B32     rwlock_try_write_lock(RWLock* lock);
internal void    rwlock_unlock(RWLock* lock);

internal Semaphore* semaphore_create(U32 initial_value);
internal void       semaphore_destroy(Semaphore* semaphore);
internal void       semaphore_wait(Semaphore* semaphore);
internal B32        semaphore_try_wait(Semaphore* semaphore);
internal B32        semaphore_wait_timeout(Semaphore* semaphore, I32 timeout_ms);
internal void       semaphore_signal(Semaphore* semaphore);
internal U32        semaphore_value(Semaphore* semaphore);

internal ConditionVariable* condition_variable_create();
internal void               condition_variable_destroy(ConditionVariable* condition);
internal void               condition_variable_wait(ConditionVariable* condition, Mutex* mutex);
internal B32                condition_variable_wait_timeout(ConditionVariable* condition,
                                                            Mutex*             mutex,
                                                            I32                timeout_ms);
internal void               condition_variable_signal(ConditionVariable* condition);
internal void               condition_variable_broadcast(ConditionVariable* condition);

internal Barrier* barrier_create(U32 thread_count);
internal void     barrier_destroy(Barrier* barrier);
internal B32      barrier_wait(Barrier* barrier);

}  // namespace ETide::Thread

#endif
