#include "rtos.h"

/* ── Register map ─────────────────────────────────────────────── */

#define SCB_BASE    0xE000ED00UL
#define SCB_ICSR    (*(volatile uint32_t *)(SCB_BASE + 0x04UL))
#define SCB_SHPR3   (*(volatile uint32_t *)(SCB_BASE + 0x20UL))

#define SCB_ICSR_PENDSVSET (1UL << 28)

#define SYST_BASE   0xE000E010UL
#define SYST_CSR    (*(volatile uint32_t *)(SYST_BASE + 0x00UL))
#define SYST_RVR    (*(volatile uint32_t *)(SYST_BASE + 0x04UL))
#define SYST_CVR    (*(volatile uint32_t *)(SYST_BASE + 0x08UL))

/* ── Constants ────────────────────────────────────────────────── */

#define MAX_TASKS      8
#define MAX_SEMS       8

/* ── TCB ──────────────────────────────────────────────────────── */

enum { TASK_READY, TASK_BLOCKED_DELAY, TASK_BLOCKED_SEM };

struct rtos_task {
  uint32_t *sp;
  uint32_t  state;
  uint32_t  delay_until;
  struct rtos_sem *waiting_on;
  struct rtos_task *next;
};

/* ── Semaphore ────────────────────────────────────────────────── */

struct rtos_sem {
  int32_t  count;
  struct rtos_task *wait_task;   /* only one waiter (FIFO not needed here) */
  struct rtos_sem *next;         /* free list */
};

/* ── Kernel globals ───────────────────────────────────────────── */

static rtos_task_t  s_tcbs[MAX_TASKS];
static uint32_t     s_stacks[MAX_TASKS][256];     /* 1 KB each */
static rtos_sem_t   s_sems[MAX_SEMS];
static rtos_task_t  s_idle_tcb;
static uint32_t     s_idle_stack[64];

static rtos_task_t *s_ready;
static rtos_sem_t  *s_sem_free;
static int          s_num_tasks;

rtos_task_t *g_curr_tcb;
static volatile uint32_t s_tick_ms;

/* ── Critical section ─────────────────────────────────────────── */

static uint32_t crit_enter(void)
{
  uint32_t primask;
  __asm volatile("mrs %0, primask" : "=r"(primask));
  __asm volatile("cpsid i" ::: "memory");
  return primask;
}

static void crit_exit(uint32_t primask)
{
  if ((primask & 1U) == 0U) {
    __asm volatile("cpsie i" ::: "memory");
  }
}

/* ── Ready list helpers ───────────────────────────────────────── */

/* Append task to tail of ready list */
static void ready_push(rtos_task_t *t)
{
  uint32_t p = crit_enter();
  t->next = 0;

  if (!s_ready) { s_ready = t; crit_exit(p); return; }

  rtos_task_t *cur = s_ready;
  while (cur->next) cur = cur->next;
  cur->next = t;

  crit_exit(p);
}

/* Pop head of ready list */
static rtos_task_t *ready_pop(void)
{
  uint32_t p = crit_enter();
  rtos_task_t *t = s_ready;
  if (t) { s_ready = t->next; t->next = 0; }
  crit_exit(p);
  return t;
}

/* ── Trigger PendSV (ISR-safe, no critical section needed) ────── */

static void trigger_pendsv(void)
{
  SCB_ICSR = SCB_ICSR_PENDSVSET;
}

static void idle_thread(void *arg)
{
  (void)arg;

  while (1) {
    __asm volatile("wfi");
  }
}

static void idle_task_init(void)
{
  uint32_t *sp = &s_idle_stack[64];

  *(--sp) = 0x01000000UL;
  *(--sp) = (uint32_t)(uintptr_t)idle_thread;
  *(--sp) = 0xFFFFFFF9UL;
  *(--sp) = 0;
  *(--sp) = 0;
  *(--sp) = 0;
  *(--sp) = 0;
  *(--sp) = 0;

  for (int i = 0; i < 8; i++) {
    *(--sp) = 0;
  }

  s_idle_tcb.sp = sp;
  s_idle_tcb.state = TASK_READY;
  s_idle_tcb.delay_until = 0;
  s_idle_tcb.waiting_on = 0;
  s_idle_tcb.next = 0;
}

/* ── Task create ──────────────────────────────────────────────── */

rtos_task_t *rtos_task_create(void (*func)(void *), void *arg,
                              uint32_t stack_words)
{
  if (s_num_tasks >= MAX_TASKS) return 0;
  if (stack_words < 64) stack_words = 64;
  if (stack_words > 256) stack_words = 256;

  rtos_task_t *t = &s_tcbs[s_num_tasks++];

  uint32_t *sp = &s_stacks[t - s_tcbs][stack_words];

  /* Cortex-M exception frame (hardware pushes on entry):
   *   low addr: xPSR, PC, LR, R12, R3, R2, R1, R0
   * Above this: R4-R11 (manually saved by PendSV) */

  *(--sp) = 0x01000000UL;                   /* xPSR  */
  *(--sp) = (uint32_t)(uintptr_t)func;      /* PC    */
  *(--sp) = 0xFFFFFFF9UL;                   /* LR: return to thread, PSP */
  *(--sp) = 0;                              /* R12   */
  *(--sp) = 0;                              /* R3    */
  *(--sp) = 0;                              /* R2    */
  *(--sp) = 0;                              /* R1    */
  *(--sp) = (uint32_t)(uintptr_t)arg;       /* R0    */

  for (int i = 0; i < 8; i++) *(--sp) = 0; /* R4-R11 */

  t->sp          = sp;
  t->state       = TASK_READY;
  t->delay_until = 0;
  t->waiting_on  = 0;
  t->next        = 0;

  ready_push(t);

  return t;
}

/* ── Semaphore create ─────────────────────────────────────────── */

rtos_sem_t *rtos_sem_create(int32_t initial)
{
  if (s_sem_free) {
    rtos_sem_t *s = s_sem_free;
    s_sem_free    = s->next;
    s->count     = initial;
    s->wait_task = 0;
    s->next      = 0;
    return s;
  }
  /* allocate from pool */
  static int sem_idx;
  if (sem_idx >= MAX_SEMS) return 0;
  rtos_sem_t *s = &s_sems[sem_idx++];
  s->count     = initial;
  s->wait_task = 0;
  s->next      = 0;
  return s;
}

/* ── Semaphore ops ────────────────────────────────────────────── */

void rtos_sem_wait(rtos_sem_t *s)
{
  uint32_t p = crit_enter();

  if (s->count > 0) {
    s->count--;
    crit_exit(p);
    return;
  }

  /* Block current task */
  g_curr_tcb->state      = TASK_BLOCKED_SEM;
  g_curr_tcb->waiting_on = s;
  s->wait_task           = g_curr_tcb;

  crit_exit(p);
  trigger_pendsv();

  /* Spin until PendSV fires and wakes us */
  while (g_curr_tcb->state != TASK_READY) {
    __asm volatile("nop");
  }
}

int rtos_sem_try_wait(rtos_sem_t *s)
{
  uint32_t p = crit_enter();

  if (s->count > 0) {
    s->count--;
    crit_exit(p);
    return 1;
  }

  crit_exit(p);
  return 0;
}

void rtos_sem_signal(rtos_sem_t *s)
{
  uint32_t p = crit_enter();

  if (s->wait_task) {
    rtos_task_t *t = s->wait_task;
    s->wait_task   = 0;
    t->state       = TASK_READY;
    t->waiting_on  = 0;
    ready_push(t);
    trigger_pendsv();
  } else {
    s->count++;
  }

  crit_exit(p);
}

/* ── Delay ────────────────────────────────────────────────────── */

void rtos_delay_ms(uint32_t ms)
{
  if (ms == 0) return;

  uint32_t p = crit_enter();

  g_curr_tcb->state       = TASK_BLOCKED_DELAY;
  g_curr_tcb->delay_until = s_tick_ms + ms;

  crit_exit(p);
  trigger_pendsv();

  while (g_curr_tcb->state != TASK_READY) {
    __asm volatile("nop");
  }
}

/* ── Tick ─────────────────────────────────────────────────────── */

uint32_t rtos_tick(void)
{
  return s_tick_ms;
}

/* ── Scheduler (C helper called from PendSV asm) ──────────────── */

void rtos_schedule(void)
{
  /* Re-queue current task if still ready */
  if (g_curr_tcb && g_curr_tcb != &s_idle_tcb &&
      g_curr_tcb->state == TASK_READY) {
    ready_push(g_curr_tcb);
  }

  g_curr_tcb = ready_pop();

  if (!g_curr_tcb) {
    g_curr_tcb = &s_idle_tcb;
  }
}

/* ── SysTick handler ──────────────────────────────────────────── */

void SysTick_Handler(void)
{
  s_tick_ms++;

  /* Wake delayed tasks */
  for (int i = 0; i < s_num_tasks; i++) {
    if (s_tcbs[i].state == TASK_BLOCKED_DELAY &&
        (int32_t)(s_tick_ms - s_tcbs[i].delay_until) >= 0) {
      s_tcbs[i].state = TASK_READY;
      ready_push(&s_tcbs[i]);
    }
  }

  trigger_pendsv();
}

/* ── PendSV handler (naked) ───────────────────────────────────── */

__attribute__((naked)) void PendSV_Handler(void)
{
  __asm volatile(
    /* Save context */
    "  mrs    r0, psp                       \n"
    "  cpsid  i                             \n"
    "  stmdb  r0!, {r4-r11}                 \n"

    "  ldr    r1, =g_curr_tcb               \n"
    "  ldr    r1, [r1]                      \n"
    "  cbz    r1, 1f                        \n"
    "  str    r0, [r1, #0]                  \n"  /* curr->sp = r0 */

    /* Schedule */
    "1:                                     \n"
    "  push   {r14}                         \n"
    "  bl     rtos_schedule                 \n"
    "  pop    {r14}                         \n"

    /* Restore context */
    "  ldr    r1, =g_curr_tcb               \n"
    "  ldr    r1, [r1]                      \n"
    "  ldr    r0, [r1, #0]                  \n"  /* r0 = next->sp */
    "  ldmia  r0!, {r4-r11}                 \n"
    "  msr    psp, r0                       \n"

    "  cpsie  i                             \n"
    "  bx     lr                            \n"
  );
}

/* ── SVC handler ──────────────────────────────────────────────── */

__attribute__((naked)) void SVC_Handler(void)
{
  __asm volatile(
    "  ldr    r0, =g_curr_tcb               \n"
    "  ldr    r0, [r0]                      \n"
    "  ldr    r0, [r0, #0]                  \n"
    "  ldmia  r0!, {r4-r11}                 \n"
    "  msr    psp, r0                       \n"
    "  movs   r0, #2                        \n"
    "  msr    control, r0                   \n"
    "  isb                                  \n"
    "  ldr    lr, =0xFFFFFFFD               \n"
    "  bx     lr                            \n"
  );
}

/* ── Start scheduler ──────────────────────────────────────────── */

void rtos_start(void)
{
  idle_task_init();

  /* Make PendSV and SysTick lowest priority */
  SCB_SHPR3 = (SCB_SHPR3 & 0x00FFFFFFUL) | (0xFFUL << 16) | (0xFFUL << 24);

  /* SysTick: 8 MHz / 8000 = 1 ms */
  SYST_RVR = 7999UL;
  SYST_CVR = 0;
  SYST_CSR = (1UL << 2) | (1UL << 1) | (1UL << 0);

  /* Pick first task */
  g_curr_tcb = ready_pop();

  __asm volatile(
    "  cpsie  i                             \n"
    "  svc    #0                            \n"
  );

  while (1) { }
}
