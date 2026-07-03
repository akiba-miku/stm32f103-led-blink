#include "uart.h"

#define PERIPH_BASE 0x40000000UL
#define APB2PERIPH_BASE (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE (PERIPH_BASE + 0x00020000UL)

#define GPIOA_BASE (APB2PERIPH_BASE + 0x00000800UL)
#define RCC_BASE (AHBPERIPH_BASE + 0x00001000UL)
#define USART1_BASE (APB2PERIPH_BASE + 0x00003800UL)
#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_APB2ENR REG32(RCC_BASE + 0x18UL)

#define GPIOA_CRH REG32(GPIOA_BASE + 0x04UL)

#define USART1_SR REG32(USART1_BASE + 0x00UL)
#define USART1_DR REG32(USART1_BASE + 0x04UL)
#define USART1_BRR REG32(USART1_BASE + 0x08UL)
#define USART1_CR1 REG32(USART1_BASE + 0x0CUL)

#define RCC_APB2ENR_IOPAEN (1UL << 2)
#define RCC_APB2ENR_USART1EN (1UL << 14)

#define USART_SR_RXNE (1UL << 5)
#define USART_SR_TXE (1UL << 7)

#define USART_CR1_RE (1UL << 2)
#define USART_CR1_TE (1UL << 3)
#define USART_CR1_UE (1UL << 13)

static uint32_t uart1_brr_from_baud(uint32_t baud)
{
  if (baud == 9600U) {
    return 0x0341UL;
  }

  return 0x0045UL; /* 115200 baud at 8 MHz PCLK2. */
}

void uart1_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

  /*
   * PA9  = USART1_TX: alternate-function push-pull, 50 MHz.
   * PA10 = USART1_RX: floating input.
   */
  GPIOA_CRH &= ~((0xFUL << 4) | (0xFUL << 8));
  GPIOA_CRH |= (0xBUL << 4) | (0x4UL << 8);

  USART1_BRR = uart1_brr_from_baud(115200U);
  USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void uart1_set_baud(uint32_t baud)
{
  USART1_BRR = uart1_brr_from_baud(baud);
}

void uart1_write_char(char ch)
{
  uart1_write_byte((uint8_t)ch);
}

void uart1_write_byte(uint8_t byte)
{
  while ((USART1_SR & USART_SR_TXE) == 0U) {
  }

  USART1_DR = (uint32_t)byte;
}

void uart1_write_bytes(const uint8_t *data, uint32_t len)
{
  while (len > 0U) {
    uart1_write_byte(*data);
    data++;
    len--;
  }
}

void uart1_write_string(const char *text)
{
  while (*text != '\0') {
    uart1_write_char(*text);
    text++;
  }
}

int uart1_take_rx_frame(char *out, uint32_t len, uint32_t *out_len)
{
  char ch;

  if (out_len != 0) {
    *out_len = 0U;
  }
  if (len == 0U) {
    return 0;
  }
  if (!uart1_read_char(&ch)) {
    return 0;
  }
  out[0] = ch;
  if (out_len != 0) {
    *out_len = 1U;
  }
  return 1;
}

int _write(int file, char *ptr, int len)
{
  int i;

  (void)file;

  for (i = 0; i < len; i++) {
    uart1_write_char(ptr[i]);
  }

  return len;
}

int uart1_read_char(char *ch)
{
  if ((USART1_SR & USART_SR_RXNE) == 0U) {
    return 0;
  }

  *ch = (char)(USART1_DR & 0xFFU);
  return 1;
}

void DMA1_Channel5_IRQHandler(void)
{
}

void USART1_IRQHandler(void)
{
}
