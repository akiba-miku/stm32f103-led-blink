#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart1_init(void);
void uart1_write_char(char ch);
void uart1_write_string(const char *text);
int uart1_take_rx_frame(char *out, uint32_t len, uint32_t *out_len);

#endif
