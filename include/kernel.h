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
 * An event, represented by a #kevent_t instance, is a signal that can
 * be broadcasted to many different threads. A thread can use
 * #kthread_wait_event to wait for a specific event to fire. The kernel
 * library includes a set of events bound to MI peripheral interrupts,
 * so that it's possible for threads to wait for specific interrupts and
 * act after them.
 *
 * Typically, applications will keep the main thread at low priority
 * for the main business logic, and will spawn a few threads to handle
 * high-priority / low-latency responses to hardware events.
 *
 * In addition to threads (#kthread_t) and events (#kevent_t), the kernel
 * library offers a message passing communication primitive called "mail
 * box" (#kmbox_t). A mailbox is a circular buffer of messages called
 * "mails"; any number of threads can try to send mails into a mailbox,
 * and any number of threads can try to read mails from a mailbox. Once a
 * thread receives a mail from a mailbox, the mail is removed and other
 * threads will not see it anymore. 
 *
 * Mailboxes can be useful as communicative primitives among threads
 * (eg: to send tasks to other threads), or a synchronization primitive
 * to wait for multiple events (see #kmbox_attach_event).
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
 * 
 * NOTE: this structure should be considered internal and subject
 * to future changes. It is exposed just for debugging purposes
 * and should not be relied upon. Please do not change any
 * field without going through a function of the public API.
 */
typedef struct kthread_s
{
	/** Pointer to the top of the stack, which contains the thread register state */
	reg_block_t* stack_state;
	/** Current state of interrupt depth for this thread */
	int interrupt_depth;
	/** Size of the stack in bytes */
	int stack_size;
	/** Name of thread (for debugging purposes) */
	const char *name;
	/** Internal flags */
	uint8_t flags;
	/** Priority of the thread (0=lowest, use positive numbers only) */
	int8_t pri;
	/** Intrusive link to next thread in a waiting list */
	struct kthread_s *next;
	/** Entry point function for the thread */
	void (*user_entry)(void*);
	/** Custom argument to be passed to the entry point */
	void *user_data;
	/** Pointer to the stack buffer */
	void *stack;
} kthread_t;

/**
 * @brief A kernel mailbox.
 *
 * A mailbox is a circular buffer of messages called
 * "mails"; any number of threads can try to send mails into a mailbox,
 * and any number of threads can try to read mails from a mailbox. Once a
 * thread receives a mail from a mailbox, the mail is removed and other
 * threads will not see it anymore.
 *
 * A mailbox can be allocated either on the heap (#mbox_new) or on
 * the stack (#mbox_new_stack). In both cases, the size of the buffer
 * (maximum number of mails that can be kept within the mailbox)
 * must be specified.
 *
 * Threads can send mails using #kmbox_send or #kmbox_try_send: the former
 * will block the thread if the mailbox is full, while the later will
 * simply return a failure in that case.
 *
 * Threads can receive mails using #kmbox_recv or #kmbox_try_recv.
 *
 * NOTE: this structure should be considered internal and subject
 * to future changes. It is exposed just for debugging purposes
 * and should not be relied upon. Please do not change any
 * field without going through a function of the public API.
 */
typedef struct kmbox_s
{
	/** Read index of the circular buffer */
	uint8_t r;
	/** Write index of the circular buffer */
	uint8_t w;
	/** Size of the buffer (maximum number of mails) */
	uint8_t max;
	/** Number of events this mailbox is attached to */
	uint8_t evts;
	/** Linked list of threads waiting on this mailbox */
	kthread_t *wait_list;
	/** Circular buffer of mails */
	void* mails[0];
} kmbox_t;

/** Maximum number of mailboxes that can be attached to a single event. */
#define MAX_MAILBOXES_PER_EVENT 8

/**
 * @brief a kernel event.
 *
 * An event, represented by a #kevent_t instance, is a signal that can
 * be broadcasted to many different threads. A thread can use
 * #kthread_wait_event to wait for a specific event to fire. The kernel
 * library includes a set of events bound to MI peripheral interrupts,
 * so that it's possible for threads to wait for specific interrupts and
 * act after them.
 *
 * It is also possible to attach a mailbox to an event using #mbox_attach_event.
 * This is useful if a thread needs to wait for multiple events.
 *
 * NOTE: this structure should be considered internal and subject
 * to future changes. It is exposed just for debugging purposes
 * and should not be relied upon. Please do not change any
 * field without going through a function of the public API.
 */
typedef struct kevent_s
{
	/** Mailboxes attached to this event */
	kmbox_t *mboxes[MAX_MAILBOXES_PER_EVENT];
} kevent_t;


/** @brief Event generated when a SP interrupt is triggered */
extern kevent_t KEVENT_IRQ_SP;
/** @brief Event generated when a SI interrupt is triggered */
extern kevent_t KEVENT_IRQ_SI;
/** @brief Event generated when a AI interrupt is triggered */
extern kevent_t KEVENT_IRQ_AI;
/** @brief Event generated when a VI interrupt is triggered */
extern kevent_t KEVENT_IRQ_VI;
/** @brief Event generated when a PI interrupt is triggered */
extern kevent_t KEVENT_IRQ_PI;
/** @brief Event generated when a DP interrupt is triggered */
extern kevent_t KEVENT_IRQ_DP;
/** @brief Event generated when the RESET button is pushed (pre-NMI exception) */
extern kevent_t KEVENT_RESET;


/** 
 * @brief Initialize the multi-threading kernel
 *
 * The current execution context becomes the main thread of the
 * program, with priority set to 0 (you can change priority
 * to the main thread as well using #thread_set_pri).
 *
 * The main thread uses the original stack allocated for the
 * whole process, so it is technically unbounded (or limited by
 * heap size).
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
 *            Priority of the thread (0-127). Negative values are not allowed.
 *            Higher number means higher priority.
 * @param[in] user_entry
 *            Entry point of the thread. This is the function that will
 *            be called when the thread begins execution. If this function
 *            ever returns, the thread is automatically killed.
 * @param[in] user_data
 *            Argument that will be passed to the entry point of the thread.
 *
 * @return    A pointer to the new thread. It is not necessary to store
 *            this reference if not required; the thread will clean up
 *            after itself when it exits.
 */
kthread_t* kthread_new(const char *name, int stack_size, int8_t pri,
	void (*user_entry)(void*), void *user_data);


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
 * its stack). This function can executed for any thread, including the
 * current one. The execution will be aborted and the memory released
 * to the heap for further use.
 *
 * @param[in]  th
 *             Reference to the thread to kill (NULL = current thread)
 *
 */
void kthread_kill(kthread_t *th);


/**
 * @brief Return the name of the specified thread.
 *
 * @param[in]  th
 *             Reference to thread (NULL = current thread)
 */
const char* kthread_name(kthread_t *th);

/**
 * @brief Wait for a single hardware event.
 *
 * This functions blocks the current thread until a specified
 * hardware event happens. The CPU is yielded so that other
 * threads will be scheduled.
 *
 * If the thread needs to handle multiple events and/or a more
 * elaborate communication with other threads, you can use
 * #kmbox with #kmbox_attach_event instead.
 *
 * @param[in] evt
 *            The event to wait for
 *
 */
void kthread_wait_event(kevent_t *evt);

/**
 * @brief Allocate a new mailbox (on the heap).
 *
 * @param[in] max_mails
 *            Maximum number of pending mails in the mailbox.
 *
 * @return The allocated mailbox
 *
 * @note See #mbox_new_stack to allocate a mailbox on the stack.
 */
kmbox_t* kmbox_new(int max_mails);

/**
 * @brief Allocate a new mailbox (on the stack).
 *
 * @param[in] max_mails
 *            Maximum number of pending mails in the mailbox.
 *
 * @return The allocated mailbox
 *
 * @note See #mbox_new to allocate a mailbox on the heap.
 */
#define kmbox_new_stack(max_mails) ({ \
	int sz = sizeof(kmbox_t) + (max_mails)*sizeof(void*); \
	kmbox_t *mb = alloca(sz); \
	bzero(mb, sz); \
	mb->max = max_mails; \
	mb; \
})

/**
 * @brief Free a heap mailbox when it's not needed anymore.
 *
 * @note Do not call this function on a mailbox allocated with
 *       #kmbox_new_stack. As the mailbox is on the stack, there is
 *       no need to explicitly free it.
 */
void kmbox_free(kmbox_t *mbox);

/** @brief Return true if the mailbox is empty */
bool kmbox_empty(kmbox_t *mbox);

/** @brief Return true if the mailbox is full */
bool kmbox_full(kmbox_t *mbox);

/** 
 * @brief Try sending a mail to a mailbox. 
 *
 * This function tries to send a mail to a mailbox. If the
 * mailbox is full, it returns false without blocking.
 *
 * @param[in] mbox The mailbox to send the mail to
 * @param[in] mail The mail to send (must not be NULL)
 *
 * @return true if the mail was sent, false if the mailbox was full
 */
bool kmbox_try_send(kmbox_t *mbox, void *mail);

/** 
 * @brief Send a mail to a mailbox. 
 *
 * This function sends a mail to a mailbox. If the
 * mailbox is full, it will block until another thread
 * receive a pending mail, so that the mailbox has space
 * for the new mail.
 *
 * @param[in] mbox The mailbox to send the mail to
 * @param[in] mail The mail to send (must not be NULL)
 *
 */
void kmbox_send(kmbox_t *mbox, void *mail);

/** 
 * @brief Try receiving a mail from a mailbox. 
 *
 * This function tries to receive a mail to a mailbox. If the
 * mailbox is empty, it will return NULL without blocking.
 *
 * @param[in] mbox The mailbox to receive the mail from
 *
 * @return the received mail, or NULL if the mailbox was empty
 *
 */
void* kmbox_try_recv(kmbox_t *mbox);

/** 
 * @brief Receive a mail from a mailbox. 
 *
 * This function receives a mail from a mailbox. If the
 * mailbox is empty, it will block until a mail is available.
 *
 * @param[in] mbox The mailbox to receive the mail from
 *
 * @return the received mail
 *
 */
void* kmbox_recv(kmbox_t *mbox);


/**
 * @brief Attach an event to a mailbox, so that a mail arrives when the event
 *        is triggered.
 *
 * This functions connects a mailbox to the specified event, so that
 * when the event is triggered, a mail is posted to the
 * mailbox. The thread can then poll the mailbox to see when the
 * event is arrived. The received mail will be the event itself.
 *
 * This function is useful for threads that need to react to multiple
 * events and/or handle messages from other threads. If you just
 * want to wait for a single interrupt, it is easier to simply
 * call #thread_wait_event.
 *
 * Example:
 *
 *     kmbox_t *mbox = kmbox_new_stack(4);
 *     kmbox_attach_event(mbox, &KEVENT_IRQ_SP);
 *     kmbox_attach_event(mbox, &KEVENT_IRQ_DP);
 *
 *     void *mail;
 *     while ((mail = kmbox_recv(mbox)))
 *     {
 *        if (mail == &KEVENT_IRQ_SP)
 *        {
 *            [...]
 *        }
 *        else if (mail == &KEVENT_IRQ_DP)
 *        {
 *            [...]
 *        }
 *     }
 *
 *     // Always detach mailboxes before freeing them!
 *     kmbox_detach_event(mbox, &KEVENT_IRQ_SP);
 *     kmbox_detach_event(mbox, &KEVENT_IRQ_DP);
 *
 *
 * @note It is mandatory to detach all attached events before
 *       destroying a mailbox.
 *
 * @note If the mailbox happens to be full when the event is triggered,
 *       the event will be missed. When you attach a mailbox to an event,
 *       make always sure to have enough space in the mailbox.
 *
 * @param[in] mbox The mailbox that will receive the event
 * @param[in] evt The event to attach to
 */
void kmbox_attach_event(kmbox_t *mbox, kevent_t *evt);


/**
 * @brief Detach an event from a mailbox.
 *
 * @note It is mandatory to detach all attached events before
 *       destroying a mailbox.
 *
 * @param[in] mbox The mailbox that is receiving the event
 * @param[in] evt The event to detach from
 */
void kmbox_detach_event(kmbox_t *mbox, kevent_t *evt);


/** @} */

#endif /* KERNEL_H */
