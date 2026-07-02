#ifndef RTOS_H
#define RTOS_H

#include <stdint.h>

typedef struct rtos_task  rtos_task_t;
typedef struct rtos_sem   rtos_sem_t;
typedef struct rtos_msgq  rtos_msgq_t;
typedef struct rtos_mailbox rtos_mailbox_t;

typedef struct {
  uint32_t id;
  uint32_t value0;
  uint32_t value1;
  uint32_t value2;
} rtos_mail_t;

/* ── Task API ─────────────────────────────────────────────────── */

/*
 * Create a task.  Returns NULL if out of slots (max 8).
 * The task will NOT run until rtos_start() is called.
 *   func       — thread entry (must never return)
 *   stack_size — in words (e.g. 256 → 1024 bytes)
 */
rtos_task_t *rtos_task_create(void (*func)(void *), void *arg,
                              uint32_t stack_words);

/* Yield for at least `ms` milliseconds.  Callable only from a task. */
void rtos_delay_ms(uint32_t ms);

/* ── Semaphore API ────────────────────────────────────────────── */

/*
 * Create a binary semaphore with initial count.
 * Returns NULL if out of slots (max 8).
 */
rtos_sem_t *rtos_sem_create(int32_t initial);

/* Decrement the semaphore. Blocks the calling task if count == 0. */
void rtos_sem_wait(rtos_sem_t *s);

/* Try to decrement the semaphore. Returns 1 on success, 0 if count == 0. */
int rtos_sem_try_wait(rtos_sem_t *s);

/* Increment the semaphore. Wakes one waiter if any. Safe to call from ISR. */
void rtos_sem_signal(rtos_sem_t *s);

/* ── Message Queue API ───────────────────────────────────────── */

/*
 * Create a fixed-depth message queue. Messages are 32-bit values.
 * Returns NULL if out of queue slots (max 8).
 */
rtos_msgq_t *rtos_msgq_create(void);

/* Send one message. Safe to call from ISR. Returns 1 on success. */
int rtos_msgq_send(rtos_msgq_t *q, uint32_t msg);

/* Receive one message without blocking. Returns 1 when a message was read. */
int rtos_msgq_try_recv(rtos_msgq_t *q, uint32_t *msg);

/* ── Mailbox Queue API ──────────────────────────────────────── */

/*
 * Create a fixed-depth mailbox queue. Each mail carries one id and three
 * 32-bit values, suitable for sending small structured data between tasks.
 * Returns NULL if out of mailbox slots (max 8).
 */
rtos_mailbox_t *rtos_mailbox_create(void);

/* Send one mail. Safe to call from ISR. Returns 1 on success. */
int rtos_mailbox_send(rtos_mailbox_t *box, const rtos_mail_t *mail);

/* Receive one mail without blocking. Returns 1 when a mail was read. */
int rtos_mailbox_try_recv(rtos_mailbox_t *box, rtos_mail_t *mail);

/* ── Scheduler ────────────────────────────────────────────────── */

/* Start the scheduler.  Never returns. */
void rtos_start(void);

/* Current tick counter (ms since start).  ISR-safe. */
uint32_t rtos_tick(void);

#endif
