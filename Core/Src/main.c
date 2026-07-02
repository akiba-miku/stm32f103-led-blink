#include <stdint.h>
#include <stdio.h>

#include "rtos.h"
#include "uart.h"

#define PERIPH_BASE      0x40000000UL
#define APB1PERIPH_BASE  (PERIPH_BASE + 0x00000000UL)
#define APB2PERIPH_BASE  (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE   (PERIPH_BASE + 0x00020000UL)

#define TIM2_BASE        (APB1PERIPH_BASE + 0x00000000UL)
#define GPIOA_BASE       (APB2PERIPH_BASE + 0x00000800UL)
#define GPIOC_BASE       (APB2PERIPH_BASE + 0x00001000UL)
#define RCC_BASE         (AHBPERIPH_BASE + 0x00001000UL)

#define REG32(addr)      (*(volatile uint32_t *)(addr))

#define RCC_APB1ENR      REG32(RCC_BASE + 0x1CUL)
#define RCC_APB2ENR      REG32(RCC_BASE + 0x18UL)

#define TIM2_CR1         REG32(TIM2_BASE + 0x00UL)
#define TIM2_DIER        REG32(TIM2_BASE + 0x0CUL)
#define TIM2_SR          REG32(TIM2_BASE + 0x10UL)
#define TIM2_EGR         REG32(TIM2_BASE + 0x14UL)
#define TIM2_PSC         REG32(TIM2_BASE + 0x28UL)
#define TIM2_ARR         REG32(TIM2_BASE + 0x2CUL)

#define GPIOA_CRL        REG32(GPIOA_BASE + 0x00UL)
#define GPIOA_IDR        REG32(GPIOA_BASE + 0x08UL)
#define GPIOA_BSRR       REG32(GPIOA_BASE + 0x10UL)
#define GPIOC_CRH        REG32(GPIOC_BASE + 0x04UL)
#define GPIOC_BSRR       REG32(GPIOC_BASE + 0x10UL)
#define GPIOC_BRR        REG32(GPIOC_BASE + 0x14UL)

#define NVIC_ISER0       REG32(0xE000E100UL)
#define NVIC_IPR7        REG32(0xE000E41CUL)

#define RCC_APB1ENR_TIM2EN (1UL << 0)
#define RCC_APB2ENR_IOPAEN (1UL << 2)
#define RCC_APB2ENR_IOPCEN (1UL << 4)

#define TIM_CR1_CEN      (1UL << 0)
#define TIM_DIER_UIE     (1UL << 0)
#define TIM_SR_UIF       (1UL << 0)
#define TIM_EGR_UG       (1UL << 0)

#define TIM2_IRQN        28UL
#define LED_PIN          13UL

#define KEY0_PIN         0UL
#define KEY1_PIN         1UL
#define KEY0_MSG         0x10UL
#define KEY1_MSG         0x20UL
#define KEY_DEBOUNCE_TICKS 3U

typedef struct {
  uint32_t pin;
  uint32_t msg;
  uint8_t stable_pressed;
  uint8_t candidate_pressed;
  uint8_t debounce_ticks;
} key_scan_t;

static rtos_msgq_t *led_msgq;
static rtos_msgq_t *uart_msgq;

static key_scan_t keys[] = {
  {KEY0_PIN, KEY0_MSG, 0U, 0U, 0U},
  {KEY1_PIN, KEY1_MSG, 0U, 0U, 0U},
};

static void led_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

  GPIOC_CRH &= ~(0xFUL << 20);
  GPIOC_CRH |= (0x2UL << 20); /* PC13: output push-pull, 2 MHz. */
  GPIOC_BSRR = (1UL << LED_PIN); /* Blue Pill LED is active-low. */
}

static void led_on(void)
{
  GPIOC_BRR = (1UL << LED_PIN);
}

static void led_off(void)
{
  GPIOC_BSRR = (1UL << LED_PIN);
}

static void led_toggle(uint32_t *is_on)
{
  if (*is_on != 0U) {
    led_off();
    *is_on = 0U;
  } else {
    led_on();
    *is_on = 1U;
  }
}

static void button_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPAEN;

  GPIOA_CRL &= ~((0xFUL << 0) | (0xFUL << 4));
  GPIOA_CRL |= (0x8UL << 0) | (0x8UL << 4); /* PA0/PA1 input pull-up/down. */
  GPIOA_BSRR = (1UL << KEY0_PIN) | (1UL << KEY1_PIN); /* Active-low buttons. */
}

static uint32_t key_is_pressed(uint32_t pin)
{
  return ((GPIOA_IDR & (1UL << pin)) == 0U) ? 1U : 0U;
}

static void post_key_release_message(uint32_t msg)
{
  (void)rtos_msgq_send(led_msgq, msg);
  (void)rtos_msgq_send(uart_msgq, msg);
}

static void scan_one_key(key_scan_t *key)
{
  const uint8_t raw_pressed = (uint8_t)key_is_pressed(key->pin);

  if (raw_pressed == key->candidate_pressed) {
    if (key->debounce_ticks < KEY_DEBOUNCE_TICKS) {
      key->debounce_ticks++;
    }
  } else {
    key->candidate_pressed = raw_pressed;
    key->debounce_ticks = 1U;
  }

  if (key->debounce_ticks < KEY_DEBOUNCE_TICKS ||
      key->stable_pressed == key->candidate_pressed) {
    return;
  }

  key->stable_pressed = key->candidate_pressed;
  if (key->stable_pressed == 0U) {
    post_key_release_message(key->msg);
  }
}

static void scan_keys(void)
{
  for (uint32_t i = 0U; i < (sizeof(keys) / sizeof(keys[0])); i++) {
    scan_one_key(&keys[i]);
  }
}

static void tim2_key_scan_init(void)
{
  RCC_APB1ENR |= RCC_APB1ENR_TIM2EN;

  TIM2_CR1 = 0U;
  TIM2_PSC = 7999U; /* 8 MHz / (7999 + 1) = 1 kHz. */
  TIM2_ARR = 9U;    /* 1 kHz / (9 + 1) = 100 Hz, scan every 10 ms. */
  TIM2_EGR = TIM_EGR_UG;
  TIM2_SR = 0U;
  TIM2_DIER = TIM_DIER_UIE;

  NVIC_IPR7 = (NVIC_IPR7 & ~0x000000FFUL) | 0x00000080UL;
  NVIC_ISER0 = (1UL << TIM2_IRQN);

  TIM2_CR1 = TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
  if ((TIM2_SR & TIM_SR_UIF) == 0U) {
    return;
  }

  TIM2_SR &= ~TIM_SR_UIF;
  scan_keys();
}

static void led_thread(void *arg)
{
  uint32_t led_is_on = 0U;
  uint32_t blink_period_ms = 500U;
  uint32_t next_toggle_ms = 0U;

  (void)arg;

  while (1) {
    uint32_t msg;
    const uint32_t now = rtos_tick();

    while (rtos_msgq_try_recv(led_msgq, &msg)) {
      if (msg == KEY0_MSG) {
        blink_period_ms = 100U;
      } else if (msg == KEY1_MSG) {
        blink_period_ms = 800U;
      }
      next_toggle_ms = now;
    }

    if ((int32_t)(now - next_toggle_ms) >= 0) {
      led_toggle(&led_is_on);
      next_toggle_ms = now + blink_period_ms;
    }

    rtos_delay_ms(20U);
  }
}

static void uart_thread(void *arg)
{
  char next_char = 'A';
  uint32_t next_send_ms = 0U;

  (void)arg;

  while (1) {
    uint32_t msg;
    const uint32_t now = rtos_tick();

    while (rtos_msgq_try_recv(uart_msgq, &msg)) {
      printf("[UART thread] key released, code=0x%02lX\r\n",
             (unsigned long)msg);
    }

    if ((int32_t)(now - next_send_ms) >= 0) {
      printf("[UART thread] send char: %c\r\n", next_char);
      next_char++;
      if (next_char > 'Z') {
        next_char = 'A';
      }
      next_send_ms = now + 1000U;
    }

    rtos_delay_ms(20U);
  }
}

int main(void)
{
  uart1_init();
  led_init();
  button_init();

  led_msgq = rtos_msgq_create();
  uart_msgq = rtos_msgq_create();

  uart1_write_string("\r\nRTOS timer key scan message queue demo\r\n");
  uart1_write_string("TIM2 scans PA0/PA1 every 10 ms and sends messages on key release\r\n");
  uart1_write_string("PA0 release -> code 0x10, LED fast blink\r\n");
  uart1_write_string("PA1 release -> code 0x20, LED slow blink\r\n");
  uart1_write_string("UART1: PA9 TX, PA10 RX, 9600 8N1\r\n\r\n");

  rtos_task_create(led_thread, 0, 256);
  rtos_task_create(uart_thread, 0, 256);

  tim2_key_scan_init();
  rtos_start();

  while (1) {
  }
}
