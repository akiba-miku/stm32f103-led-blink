#include "lora.h"

#include <stdio.h>

#define PERIPH_BASE 0x40000000UL
#define APB1PERIPH_BASE (PERIPH_BASE + 0x00000000UL)
#define APB2PERIPH_BASE (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE (PERIPH_BASE + 0x00020000UL)

#define GPIOA_BASE (APB2PERIPH_BASE + 0x00000800UL)
#define RCC_BASE (AHBPERIPH_BASE + 0x00001000UL)
#define USART2_BASE (APB1PERIPH_BASE + 0x00004400UL)

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_APB2ENR REG32(RCC_BASE + 0x18UL)
#define RCC_APB1ENR REG32(RCC_BASE + 0x1CUL)

#define GPIOA_CRL REG32(GPIOA_BASE + 0x00UL)

#define USART2_SR REG32(USART2_BASE + 0x00UL)
#define USART2_DR REG32(USART2_BASE + 0x04UL)
#define USART2_BRR REG32(USART2_BASE + 0x08UL)
#define USART2_CR1 REG32(USART2_BASE + 0x0CUL)

#define RCC_APB2ENR_IOPAEN (1UL << 2)
#define RCC_APB1ENR_USART2EN (1UL << 17)

#define USART_SR_RXNE (1UL << 5)
#define USART_SR_TXE (1UL << 7)

#define USART_CR1_RE (1UL << 2)
#define USART_CR1_TE (1UL << 3)
#define USART_CR1_UE (1UL << 13)

#define LORA_LINE_MAX 64U

static char rx_line[LORA_LINE_MAX];
static uint32_t rx_line_len;

static void lora_write_char(char ch)
{
  while ((USART2_SR & USART_SR_TXE) == 0U) {
  }

  USART2_DR = (uint32_t)(uint8_t)ch;
}

static void lora_write_string(const char *text)
{
  while (*text != '\0') {
    lora_write_char(*text);
    text++;
  }
}

static int lora_read_char(char *ch)
{
  if ((USART2_SR & USART_SR_RXNE) == 0U) {
    return 0;
  }

  *ch = (char)(USART2_DR & 0xFFU);
  return 1;
}

static int parse_u32(const char **cursor, uint32_t *value)
{
  uint32_t result = 0U;
  uint32_t digits = 0U;

  while (**cursor >= '0' && **cursor <= '9') {
    result = (result * 10U) + (uint32_t)(**cursor - '0');
    (*cursor)++;
    digits++;
  }

  if (digits == 0U) {
    return 0;
  }

  *value = result;
  return 1;
}

static int parse_i32(const char **cursor, int *value)
{
  uint32_t magnitude = 0U;
  int sign = 1;

  if (**cursor == '-') {
    sign = -1;
    (*cursor)++;
  }

  if (!parse_u32(cursor, &magnitude)) {
    return 0;
  }

  *value = (int)magnitude * sign;
  return 1;
}

static int expect_char(const char **cursor, char expected)
{
  if (**cursor != expected) {
    return 0;
  }

  (*cursor)++;
  return 1;
}

static int parse_line(const char *line, lora_sample_t *sample)
{
  const char *p = line;
  uint32_t value;

  if (p[0] != 'L' || p[1] != 'H' || p[2] != ',') {
    return 0;
  }
  p += 3;

  if (*p != 'T' && *p != 'G') {
    return 0;
  }
  sample->role = (uint8_t)*p;
  p++;

  if (!expect_char(&p, ',')) {
    return 0;
  }
  if (!parse_u32(&p, &sample->sequence)) {
    return 0;
  }
  if (!expect_char(&p, ',')) {
    return 0;
  }
  if (!parse_u32(&p, &value) || value > 255U) {
    return 0;
  }
  sample->temperature = (uint8_t)value;

  if (!expect_char(&p, ',')) {
    return 0;
  }
  if (!parse_u32(&p, &value) || value > 255U) {
    return 0;
  }
  sample->humidity = (uint8_t)value;

  if (!expect_char(&p, ',')) {
    return 0;
  }
  if (!parse_i32(&p, &sample->error)) {
    return 0;
  }

  return 1;
}

void lora_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPAEN;
  RCC_APB1ENR |= RCC_APB1ENR_USART2EN;

  /*
   * PA2 = USART2_TX: alternate-function push-pull, 50 MHz.
   * PA3 = USART2_RX: floating input.
   */
  GPIOA_CRL &= ~((0xFUL << 8) | (0xFUL << 12));
  GPIOA_CRL |= (0xBUL << 8) | (0x4UL << 12);

  /* LoRa transparent UART modules commonly default to 9600 8N1. */
  USART2_BRR = 0x0341UL;
  USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void lora_send_sample(uint8_t role, uint32_t sequence, uint8_t temperature,
                      uint8_t humidity, int error)
{
  char frame[64];

  (void)snprintf(frame, sizeof(frame), "LH,%c,%lu,%u,%u,%d\n",
                 (char)role,
                 (unsigned long)sequence,
                 (unsigned)temperature,
                 (unsigned)humidity,
                 error);
  lora_write_string(frame);
}

int lora_poll_sample(lora_sample_t *sample)
{
  char ch;

  while (lora_read_char(&ch)) {
    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      rx_line[rx_line_len] = '\0';
      rx_line_len = 0U;
      return parse_line(rx_line, sample);
    }

    if (rx_line_len < (LORA_LINE_MAX - 1U)) {
      rx_line[rx_line_len++] = ch;
    } else {
      rx_line_len = 0U;
    }
  }

  return 0;
}
