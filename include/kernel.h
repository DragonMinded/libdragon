/**
 * @file kernel.h
 * @brief Multi-threading kernel
 * @ingroup kernel
 */
#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "exception.h"
#include "n64sys.h"

/**
 * @defgroup kernel Multi-threading kernel
 * @ingroup libdragon
 * @brief A multi-threading kernel that can be used by libdragon applications.
 *
 * This module implements a hybrid cooperative/preemptive multi-threaded
 * kernel for parallel execution of code.
 *
 * The scheduler uses a very simple logic:
 * 
 *   * A thread is "ready" whenever is able to run, that is, it is not waiting
 *     for some event or otherwise sleeping. A ready thread can technically be
 *     scheduled at any time, whenever the scheduler decides so.
 *   * When a thread switch happens, the scheduler selects the ready thread which
 *     has the highest priority.
 *   * If there are multiple threads with the same priority (higher than any
 *     other ready thread), the scheduler will round-robin among them.
 * 
 * A thread switch can happen in a few specific situations:
 * 
 *   * The active thread explicitly starts waiting for some event, or sleeps
 *     for a definite amount of time. In general, whenever a kthread_* function
 *     is invoked, it might cause a context switch.
 *   * An interrupt occurs, which triggers an event, which wakes up some thread
 *     whose priority is higher than the currently running thread.
 * 
 * Given the above rules, we can say that the kernel is "hard real-time": any
 * thread with high priority that is ready (not blocked) will always have
 * priority over lower priority threads. This is required to allow threads to
 * implement operations that require low latency, for instance preparing audio
 * when the AI interrupt fires.
 *
 * Also, the kernel is not fully preemptive; in particular, there is no timer
 * interrupt that switches among ready threads at a fixed interval. This
 * is not deemed necessary as applications will usually have a low number
 * threads that are mostly blocked waiting for specific events like
 * hardware interrupts or background activities like RSP ucode.
 * 
 * @{
 */

/**
 * @brief a kernel thread for parallel execution
 *
 * This structure represents a thread that can be scheduled
 * for parallel execution. Create a thread with #kthread_new,
 * allocating the required memory for the stack.
 *
 * Normally, there's no need to explicitly manage the lifetime
 * or state of a thread. Once the thread is created, it is
 * immediately started, and the kernel will scheduled it when
 * required. If the thread exits (by simply returning by its
 * entry point function), it will be cleaned up and the memory
 * released.
 *
 * #kthread_new returns a pointer to the thread (a pointer
 * to kthread_t) that can be used to externally manage the
 * thread like changing its priority or killing it.
 */
typedef struct kthread_s kthread_t;

/**
 * @brief A mutex for synchronization
 * 
 * A mutex is a synchronization primitive that can be used to protect
 * shared resources from concurrent access. A mutex can be locked by
 * only one thread at a time; if another thread tries to lock a mutex
 * that is already locked, it will block until the mutex is unlocked.
 * 
 * The mutex can be created with the KMUTEX_RECURSIVE flag, so that
 * the same thread can lock the mutex multiple times without blocking.
 * In this case, the mutex must be unlocked the same number of times
 * it was locked.
 * 
 * @note The contents of this structure are subject to change and should be
 * considered internal. Do not access or modify any field directly. The structure
 * is exposed only to allow creation in a static context (that is, without malloc).
 * 
 * @see #kmutex_init
 * @see #kmutex_lock
 * @see #kmutex_unlock
 * @see #kmutex_destroy
 */
typedef struct kmutex_s {
    uint8_t flags : 8;          ///< Flags for the mutex
    phys_addr_t owner : 24;     ///< Owner thread
    uint8_t counter : 8;        ///< Recursive lock counter
    phys_addr_t waiting : 24;   ///< List of waiting threads
} kmutex_t;
    
#define KMUTEX_STANDARD		0			///< Standard mutex
#define KMUTEX_RECURSIVE	(1<<0)		///< Recursive mutex


/**
 * @brief A condition variable for synchronization
 * 
 * A condition variable is a synchronization primitive that allows
 * threads to wait for a specific condition to happen. A condition
 * variable is always associated with a mutex, and the mutex must be
 * locked before calling #kcond_wait or #kcond_signal.
 * 
 * @note The contents of this structure are subject to change and should be
 * considered internal. Do not access or modify any field directly. The structure
 * is exposed only to allow creation in a static context (that is, without malloc).
 * 
 * @see #kcond_init
 * @see #kcond_wait
 * @see #kcond_signal
 * @see #kcond_broadcast
 * 
 */
typedef struct kcond_s {
    kthread_t *waiting;			///< List of waiting threads
} kcond_t;

/** 
 * @brief Initialize the multi-threading kernel
 *
 * The current execution context becomes the main thread of the
 * program, with priority set to 0 (you can change priority
 * to the main thread as well using #kthread_set_pri).
 *
 * The main thread uses the original stack allocated for the
 * whole process, so it is technically unbounded (or limited by
 * heap size). The priority of the main thread is 0.
 *
 * @return a pointer to the main thread.
 *
 */
kthread_t* kernel_init(void);

/** 
 * @brief Shutdown the multi-threading kernel
 *
 * This function is mostly useful for testing purposes.
 * Since the kernel does not keep track of all created threads,
 * this function should be called only when all created threads
 * are exited or have been killed.
 */
void kernel_close(void);

/**
 * @brief Create a new thread
 *
 * Create a new thread, with a specified stack size and priority.
 * The thread is immediately made ready after creation, so if it has
 * a priority higher than or equal to the current thread, it will be
 * scheduled immediately, before thread_new returns.
 *
 * The thread will begin execution from the specified entry point
 * function. If the function ever returns, the thread is automatically
 * killed.
 *
 * @param[in] name
 *            Name of the thread (for debugging purposes)
 * @param[in] stack_size
 *            Size of the stack in bytes. Minimum suggested size
 *            is 2048.
 * @param[in] pri
 *            Priority of the thread (-128 .. 127). Higher number means higher 
 * 			  priority. Main thread is conventionally set at 0, so you can use
 * 		      positive numbers for "high priority" tasks that should interrupt
 *            the main thread (eg: audio), and negative numbers for stuff that
 * 		      needs to happen "in background" while the main thread is idle.
 * @param[in] user_entry
 *            Entry point of the thread. This is the function that will
 *            be called when the thread begins execution. If this function
 *            ever returns, the thread is automatically killed, and the return
 * 			  value will be its exit code.
 * @param[in] user_data
 *            Argument that will be passed to the entry point of the thread.
 *
 * @return    A pointer to the new thread. It is not necessary to store
 *            this reference if not required; the thread will clean up
 *            after itself when it exits.
 */
kthread_t* kthread_new(const char *name, int stack_size, int8_t pri,
    int (*user_entry)(void*), void *user_data);


/**
 * @brief Return a reference to the current running thread
 * 
 * @return kthread_t	Reference to the current thread
 */
kthread_t* kthread_current(void);


/**
 * @brief Yield execution of the current thread and run the scheduler.
 *
 * This function allows the current thread to cooperatively yield
 * its execution to allow other threads to run.
 *
 * The scheduler will switch to the highest priority thread that is
 * currently ready to run. If no ready thread has a priority higher than
 * the current thread, the scheduler will switch to a different thread 
 * of the same priority of the current one (the scheduler will guarantee
 * a correct round-robin scheduling among threads of the same priority).
 * If no ready thread has a priority higher or equal than the current
 * one, the scheduler will reschedule the current thread, that will
 * continue execution.
 *
 * @note The scheduler is semi-preemptive. Any interrupt could cause
 * a thread switch to happen if the interrupt itself makes a thread
 * become ready. If you need a block of code to be executed without
 * any context switch, make sure to disable interrupts.
 */
void kthread_yield(void);

/**
 * @brief Suspend the specified thread
 * 
 * Suspends the specified thread, so that it will not be scheduled anymore.
 * The thread will be put in a "suspended" state, and will not be scheduled
 * until it is resumed.
 * 
 * @param th        Reference to the thread to suspend
 */
void kthread_suspend(kthread_t *th);

/**
 * @brief Resume a thread that was previously suspended
 * 
 * @param th        Reference to the thread to resume
 */
void kthread_resume(kthread_t *th);

/**
 * @brief Return the backtrace of the specified thread
 * 
 * This function has the same semantic of #backtrace, but it returns
 * the backtrace of the specified thread instead of the current one.
 * 
 * When called with a NULL argument, this function returns the backtrace
 * of the current thread, and thus behaves identical to #backtrace.
 * 
 * @param[in]  th       Reference to the thread to backtrace (NULL = current thread)
 * @param[out] buffer   Buffer where to store the backtrace
 * @param[in]  size     Size of the buffer
 * @return              Number of entries in the backtrace
 */
int kthread_backtrace(kthread_t *th, void *buffer, int size);

/**
 * @brief Sleep for the specified interval, allowing execution of other threads.
 *
 * This function will put the current thread to sleep for a specified
 * time interval, allowing other threads to run.
 *
 * The sleeping interval is expressed in hardware ticks. See #TICKS_READ
 * for an explanation of hardware ticks, and #TICKS_PER_SECOND and #TICKS_FROM_MS
 * for constants and macros that simplify conversion to standard time units.
 *
 * Notice that #wait_ticks and #wait_ms are not threading aware, so they will
 * just spin-wait while keeping the current thread ready. Other higher-priority
 * threads might still be scheduled if they are awaken by an interrupt, but in
 * general thread_sleep is a better primitive if you want the current thread
 * to yield.
 *
 * @note This function requires the timer module, so #timer_init must have
 *       been called.
 *
 * @param[in] ticks
 *            The number of hardware ticks to sleep.
 */
void kthread_sleep(uint32_t ticks);

/** 
 * @brief Change priority of a thread.
 * 
 * Change priority of the specified thread. If the argument is NULL,
 * this function changes priority to the current thread.
 *
 * The change of priority is immediately effective. It may cause
 * a context switch if the changed thread is ready and its priority
 * is changed in a way to start/stop it relative to the other ready
 * threads.
 *
 * @param[in]  th
 *             Reference to the thread (NULL = current thread)
 * @param[in]  pri
 *             New priority of the thread (0-127). Higher number means
 *             higher priority.
 *
 */
void kthread_set_pri(kthread_t *th, int8_t pri);


/** 
 * @brief Kill a thread, aborting its execution.
 *
 * The specified thread is aborted, and its memory freed (including
 * its stack). The execution will be aborted and the memory released
 * to the heap for further use.
 * 
 * This function can executed for any thread, including the
 * current one, in which case it becomes equivalent to #kthread_exit.
 *
 * @param[in]  th 	Reference to the thread to kill. Since NULL refers to the
 *                  current thread, calling this function passing NULL is
 *                  equivalent to #kthread_exit.
 * @param[in]  res	Result code of the thread that will be killed
 * 
 * @see #kthread_exit
 */
void kthread_kill(kthread_t *th, int res);


/**
 * @brief Exit from a thread, providing a result value
 * 
 * This function allows to abort the execution of the current thread, optionally
 * providing a result value. If the thread is not detached, the result value
 * can be read by the joiner thread via #kthread_join. If the thread is detached,
 * the return value will be ignored.
 * 
 * Returning from a thread's entry point is equivalent to calling kthread_exit.
 * 
 * @param res 		Result value
 */
__attribute__((noreturn))
void kthread_exit(int res);

/**
 * @brief Detach a thread, so that it can terminated without any join
 * 
 * By defaults, kernel threads are "attached" to the main thread; this 
 * means that to fully terminate, there should be a thread calling kthread_join
 * on then. Joining a thread can happen before or after it finishes execution,
 * but it has to happen for the thread to be fully cleaned up.
 * 
 * If a thread is detached, it can be terminated without any join. The thread
 * will be fully cleaned up when it finishes execution.
 * 
 * NOTE: Pay attention to race conditions when detaching threads that you have
 * just created. It might be advisable to let a thread detaches itself, so that
 * you don't risk to detach a thread that is already finished.
 * 
 * @param[in]  th		Reference to the thread to detach (or NULL to
 * 						detach the current thread)
 */
void kthread_detach(kthread_t *th);


/**
 * @brief Wait for a thread to finish.
 * 
 * This function blocks the current thread until the specified
 * thread finishes its execution. The CPU is yielded so that other
 * threads will be scheduled.
 * 
 * @param[in]  th		Reference to the thread to wait for
 * @result				Result code of the thread that was joined
 */
int kthread_join(kthread_t *th);

/**
 * @brief Check if a thread is finished without blocking
 * 
 * This function is similar to #kthread_join, but it does not block if
 * the thread is not finished yet. If the thread is finished, it returns
 * true and stores the result code in the res argument. If the thread is
 * still running, it returns false.
 * 
 * Notice that the thread *is* joined if the function returns true; after
 * that, the thread is fully cleaned up and the memory released, so the thread
 * pointer becomes invalid.
 * 
 * @param[in]  th           Reference to the thread to wait for
 * @param[out] res          Pointer to the result code of the thread
 * @return true             if the thread is finished and successfully joined
 * @return false            if the thread is still running
 */
bool kthread_try_join(kthread_t *th, int *res);

/**
 * @brief Return the name of the specified thread.
 *
 * @param[in]  th
 *             Reference to thread (NULL = current thread)
 */
const char* kthread_name(kthread_t *th);

/**
 * @brief Inititalize a new mutex.
 * 
 * A mutex is a synchronization primitive that can be used to protect
 * shared resources from concurrent access. A mutex can be locked by
 * only one thread at a time; if another thread tries to lock a mutex
 * that is already locked, it will block until the mutex is unlocked.
 * 
 * The mutex can be created with the KMUTEX_RECURSIVE flag, so that
 * the same thread can lock the mutex multiple times without blocking.
 * In this case, the mutex must be unlocked the same number of times
 * it was locked.
 * 
 * @note Mutexes are not recursive by default.
 * 
 * @param mutex			Pointer to the mutex to initialize
 * @param flags			Flags for the mutex. Use #KMUTEX_STANDARD for a standard
 * 						mutex, or #KMUTEX_RECURSIVE for a recursive mutex.
 * 
 * @see #kmutex_destroy
 */
void kmutex_init(kmutex_t *mutex, uint8_t flags);

/**
 * @brief Destroy a mutex.
 * 
 * @param mtx 			Pointer to the mutex to free
 */
void kmutex_destroy(kmutex_t *mtx);

/**
 * @brief Acquire a lock to the mutex
 * 
 * This function tries to acquire a lock to the mutex. If the mutex
 * is already locked, the thread will block until the mutex is unlocked.
 * 
 * A thread can lock a mutex multiple times only if the mutex was created
 * with the KMUTEX_RECURSIVE flag. In this case, the mutex must be unlocked
 * the same number of times it was locked.
 * 
 * For a non-blocking or timed version, see #kmutex_try_lock.
 * 
 * @param mtx 			Pointer to the mutex
 * 
 * @see #kmutex_unlock
 * @see #kmutex_try_lock
 */
void kmutex_lock(kmutex_t *mtx);

/**
 * @brief Release a lock to the mutex
 * 
 * @param mtx 			Pointer to the mutex
 */
void kmutex_unlock(kmutex_t *mtx);

/**
 * @brief Try to acquire a lock to the mutex for a specified amount of time
 * 
 * This function tries to acquire a lock to the mutex. If the mutex
 * is already locked, the thread will block until the mutex is unlocked
 * but only for the specified amount of @p ticks. If the mutex is not
 * unlocked in time, the function will return false.
 * 
 * As a special case, if @p ticks is 0, the function will never block
 * and will return false immediately if the mutex is already locked.
 * 
 * @param mtx               Pointer to the mutex
 * @param ticks             Number of hardware ticks to wait for the mutex. See
 *                          #TICKS_FROM_MS or #TICKS_FROM_US for conversion macros.
 * @return true             If the mutex was successfully locked
 * @return false            If the mutex was not locked in time
 */
bool kmutex_try_lock(kmutex_t *mtx, uint32_t ticks);

/**
 * @brief Initialize a condition variable
 * 
 * A condition variable is a synchronization primitive that allows
 * threads to wait for a specific condition to happen. A condition
 * variable is always associated with a mutex, and the mutex must be
 * locked before calling #kcond_wait or #kcond_signal.
 * 
 * @param[in] cond		Pointer to the condition variable to initialize
 * 
 * @see #kcond_destroy
 */
void kcond_init(kcond_t *cond);

/**
 * @brief Destroy a condition variable
 * 
 * @param cond 			Pointer to the condition variable
 */
void kcond_destroy(kcond_t *cond);

/**
 * @brief Wait for a condition to happen
 * 
 * This function will block the current thread until the condition
 * variable is signaled. The mutex must be locked before calling
 * this function. It will be released while the thread is waiting
 * and re-acquired when the thread is woken up.
 * 
 * @param cond 			Pointer to the condition variable
 * @param mtx 			Pointer to the mutex
 * 
 * @see #kcond_signal
 * @see #kcond_broadcast
 */
void kcond_wait(kcond_t *cond, kmutex_t *mtx);


/**
 * @brief Wait for a condition to happen for a specified amount of time
 * 
 * This function will block the current thread until the condition
 * variable is signaled, or until the specified amount of time has
 * passed. The mutex must be locked before calling this function.
 * It will be released while the thread is waiting and re-acquired
 * when the thread is woken up.
 * 
 * @param cond 			Pointer to the condition variable
 * @param mtx 			Pointer to the mutex
 * @param ticks 		Number of hardware ticks to wait for the condition.
 * 
 * @return true 		If the condition was signaled
 * @return false 		If the condition was not signaled in time
 */
bool kcond_wait_timeout(kcond_t *cond, kmutex_t *mtx, uint32_t ticks);


/**
 * @brief Signal a condition variable
 * 
 * This function will wake up one thread that is waiting on the
 * condition variable. If no thread is waiting, the signal is
 * ignored.
 * 
 * @param cond 			Pointer to the condition variable
 * 
 * @see #kcond_wait
 * @see #kcond_broadcast
 */
void kcond_signal(kcond_t *cond);

/**
 * @brief Broadcast a condition variable
 * 
 * This function will wake up all threads that are waiting on the
 * condition variable. If no thread is waiting, the broadcast is
 * ignored.
 * 
 * @param cond 			Pointer to the condition variable
 * 
 * @see #kcond_wait
 * @see #kcond_signal
 */
void kcond_broadcast(kcond_t *cond);

/** @} */

#endif /* KERNEL_H */
