#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart1_init(void);
void uart1_write_char(char ch);
void uart1_write_string(const char *text);
int uart1_read_char_nonblocking(char *ch);

#endif
