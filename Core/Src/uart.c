#include "uart.h"

#define PERIPH_BASE         0x40000000UL
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE      (PERIPH_BASE + 0x00020000UL)

#define GPIOA_BASE          (APB2PERIPH_BASE + 0x00000800UL)
#define RCC_BASE            (AHBPERIPH_BASE + 0x00001000UL)
#define USART1_BASE         (APB2PERIPH_BASE + 0x00003800UL)

#define REG32(addr)         (*(volatile uint32_t *)(addr))

#define RCC_APB2ENR         REG32(RCC_BASE + 0x18UL)

#define GPIOA_CRH           REG32(GPIOA_BASE + 0x04UL)

#define USART1_SR           REG32(USART1_BASE + 0x00UL)
#define USART1_DR           REG32(USART1_BASE + 0x04UL)
#define USART1_BRR          REG32(USART1_BASE + 0x08UL)
#define USART1_CR1          REG32(USART1_BASE + 0x0CUL)

#define RCC_APB2ENR_IOPAEN  (1UL << 2)
#define RCC_APB2ENR_USART1EN (1UL << 14)

#define USART_SR_RXNE       (1UL << 5)
#define USART_SR_TXE        (1UL << 7)

#define USART_CR1_RE        (1UL << 2)
#define USART_CR1_TE        (1UL << 3)
#define USART_CR1_UE        (1UL << 13)

void uart1_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

  /*
   * PA9  = USART1_TX: alternate-function push-pull, 50 MHz.
   * PA10 = USART1_RX: floating input.
   */
  GPIOA_CRH &= ~((0xFUL << 4) | (0xFUL << 8));
  GPIOA_CRH |=  (0xBUL << 4) | (0x4UL << 8);

  /* PCLK2 defaults to 8 MHz from HSI. 8 MHz / 115200 = BRR 0x0457. */
  USART1_BRR = 0x0457UL;
  USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void uart1_write_char(char ch)
{
  while ((USART1_SR & USART_SR_TXE) == 0U) {
  }

  USART1_DR = (uint32_t)(uint8_t)ch;
}

void uart1_write_string(const char *text)
{
  while (*text != '\0') {
    uart1_write_char(*text);
    text++;
  }
}

int uart1_read_char_nonblocking(char *ch)
{
  if ((USART1_SR & USART_SR_RXNE) == 0U) {
    return 0;
  }

  *ch = (char)(USART1_DR & 0xFFU);
  return 1;
}
