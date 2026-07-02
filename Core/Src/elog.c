#include <stdio.h>

#include "elog.h"
#include "rtos.h"
#include "uart.h"

static elog_level_t s_filter_level = ELOG_LVL_DEBUG;
static rtos_sem_t *s_output_mutex;

static void elog_lock(void)
{
  if (s_output_mutex != 0) {
    rtos_sem_wait(s_output_mutex);
  }
}

static void elog_unlock(void)
{
  if (s_output_mutex != 0) {
    rtos_sem_signal(s_output_mutex);
  }
}

void elog_init(void)
{
  s_filter_level = ELOG_LVL_DEBUG;
  if (s_output_mutex == 0) {
    s_output_mutex = rtos_sem_create(1);
  }
}

void elog_start(void)
{
}

void elog_set_filter_lvl(elog_level_t level)
{
  s_filter_level = level;
}

void elog_printf(elog_level_t level, const char *tag, const char *func, int line, const char *fmt, ...)
{
  va_list args;

  if (level > s_filter_level) {
    return;
  }

  elog_lock();

  uart1_write_string("[");
  uart1_write_string(tag);
  uart1_write_string("] ");
  uart1_write_string(func);
  uart1_write_string(":");

  {
    char line_buf[12];
    snprintf(line_buf, sizeof(line_buf), "%d", line);
    uart1_write_string(line_buf);
  }

  uart1_write_string(" ");

  va_start(args, fmt);
  {
    char msg_buf[128];
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    uart1_write_string(msg_buf);
  }
  va_end(args);

  uart1_write_string("\r\n");

  elog_unlock();
}
