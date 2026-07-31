# Thread Synchronization

The `ETide::Thread` namespace exposes SDL synchronization primitives through opaque
C-style handles. Callers never access SDL handle types directly and must pair every
create or successful lock/wait operation with its matching cleanup operation.

## Mutex

```cpp
Thread::Mutex* mutex = Thread::mutex_create();

Thread::mutex_lock(mutex);
update_shared_state();
Thread::mutex_unlock(mutex);

Thread::mutex_destroy(mutex);
```

`mutex_try_lock` returns nonzero only when it acquired the mutex:

```cpp
if (Thread::mutex_try_lock(mutex)) {
    update_shared_state();
    Thread::mutex_unlock(mutex);
}
```

## Read/write lock

Use read locking for concurrent readers and write locking for exclusive mutation:

```cpp
Thread::RWLock* lock = Thread::rwlock_create();

Thread::rwlock_read_lock(lock);
read_shared_state();
Thread::rwlock_unlock(lock);

Thread::rwlock_write_lock(lock);
update_shared_state();
Thread::rwlock_unlock(lock);

Thread::rwlock_destroy(lock);
```

`rwlock_try_read_lock` and `rwlock_try_write_lock` do not block. Both lock modes use
`rwlock_unlock`.

## Semaphore

A semaphore represents an unsigned count:

```cpp
Thread::Semaphore* work = Thread::semaphore_create(0);

Thread::semaphore_signal(work);
Thread::semaphore_wait(work);

Thread::semaphore_destroy(work);
```

Available operations are:

```cpp
B32 Thread::semaphore_try_wait(Thread::Semaphore* semaphore);
B32 Thread::semaphore_wait_timeout(Thread::Semaphore* semaphore, I32 timeout_ms);
U32 Thread::semaphore_value(Thread::Semaphore* semaphore);
```

The two conditional waits return nonzero only when they consume a signal.
`semaphore_value` is an observation; another thread may change the count immediately.

## Condition variable

A condition variable is always used with a `Mutex` and a caller-owned predicate:

```cpp
Thread::Mutex* mutex = Thread::mutex_create();
Thread::ConditionVariable* condition = Thread::condition_variable_create();

Thread::mutex_lock(mutex);
while (!work_ready) {
    Thread::condition_variable_wait(condition, mutex);
}
consume_work();
Thread::mutex_unlock(mutex);
```

Waiting atomically releases the mutex and reacquires it before returning. The predicate
must be checked in a loop.

Wake one or all waiting threads with:

```cpp
Thread::condition_variable_signal(condition);
Thread::condition_variable_broadcast(condition);
```

`condition_variable_wait_timeout` uses milliseconds and returns nonzero when signaled.

Destroy the condition only after all waiters have stopped:

```cpp
Thread::condition_variable_destroy(condition);
Thread::mutex_destroy(mutex);
```

## Barrier

A barrier blocks a fixed number of threads until every participant reaches the same
phase:

```cpp
Thread::Barrier* barrier = Thread::barrier_create(worker_count);

B32 last_thread = Thread::barrier_wait(barrier);
if (last_thread) {
    // Exactly one participant receives a nonzero result for this phase.
}

Thread::barrier_destroy(barrier);
```

The barrier is reusable. Each completed group advances its generation and releases all
participants. A count of zero is invalid and makes `barrier_create` return `0`.

The barrier is implemented with SDL mutex and condition-variable primitives because
SDL does not expose a native barrier handle.

## Lifetime and locking rules

- Check every create result for `0`.
- Destroy a primitive only after all threads have stopped using it.
- Unlock only after acquiring the corresponding lock.
- Keep condition predicates protected by the same mutex used for waiting.
- Every barrier generation must receive exactly the configured number of participants.
