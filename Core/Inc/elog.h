#ifndef ELOG_H
#define ELOG_H

#include <stdarg.h>

typedef enum {
  ELOG_LVL_ASSERT = 0,
  ELOG_LVL_ERROR = 1,
  ELOG_LVL_WARN = 2,
  ELOG_LVL_INFO = 3,
  ELOG_LVL_DEBUG = 4,
} elog_level_t;

void elog_init(void);
void elog_start(void);
void elog_set_filter_lvl(elog_level_t level);
void elog_printf(elog_level_t level, const char *tag, const char *func, int line, const char *fmt, ...);

#define elog_d(fmt, ...) elog_printf(ELOG_LVL_DEBUG, "D", __func__, __LINE__, fmt, ##__VA_ARGS__)
#define elog_i(fmt, ...) elog_printf(ELOG_LVL_INFO,  "I", __func__, __LINE__, fmt, ##__VA_ARGS__)
#define elog_e(fmt, ...) elog_printf(ELOG_LVL_ERROR, "E", __func__, __LINE__, fmt, ##__VA_ARGS__)

#endif
