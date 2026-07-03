#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart1_init(void);
void uart1_set_baud(uint32_t baud);
void uart1_write_byte(uint8_t byte);
void uart1_write_bytes(const uint8_t *data, uint32_t len);
void uart1_write_char(char ch);
void uart1_write_string(const char *text);
int uart1_read_char(char *ch);
int uart1_take_rx_frame(char *out, uint32_t len, uint32_t *out_len);

#endif
