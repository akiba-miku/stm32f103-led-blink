#include <stdint.h>
#include <stdio.h>

#include "rtos.h"
#include "uart.h"

#define PERIPH_BASE      0x40000000UL
#define APB2PERIPH_BASE  (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE   (PERIPH_BASE + 0x00020000UL)

#define GPIOA_BASE       (APB2PERIPH_BASE + 0x00000800UL)
#define GPIOC_BASE       (APB2PERIPH_BASE + 0x00001000UL)
#define AFIO_BASE        (APB2PERIPH_BASE + 0x00000000UL)
#define EXTI_BASE        (APB2PERIPH_BASE + 0x00000400UL)
#define RCC_BASE         (AHBPERIPH_BASE + 0x00001000UL)

#define REG32(addr)      (*(volatile uint32_t *)(addr))

#define RCC_APB2ENR      REG32(RCC_BASE + 0x18UL)
#define GPIOA_CRL        REG32(GPIOA_BASE + 0x00UL)
#define GPIOA_BSRR       REG32(GPIOA_BASE + 0x10UL)
#define GPIOC_CRH        REG32(GPIOC_BASE + 0x04UL)
#define GPIOC_BSRR       REG32(GPIOC_BASE + 0x10UL)
#define GPIOC_BRR        REG32(GPIOC_BASE + 0x14UL)
#define AFIO_EXTICR1     REG32(AFIO_BASE + 0x08UL)
#define EXTI_IMR         REG32(EXTI_BASE + 0x00UL)
#define EXTI_FTSR        REG32(EXTI_BASE + 0x0CUL)
#define EXTI_PR          REG32(EXTI_BASE + 0x14UL)
#define NVIC_ISER0       REG32(0xE000E100UL)
#define NVIC_IPR1        REG32(0xE000E404UL)

#define RCC_APB2ENR_AFIOEN (1UL << 0)
#define RCC_APB2ENR_IOPAEN (1UL << 2)
#define RCC_APB2ENR_IOPCEN (1UL << 4)

#define LED_PIN          13UL
#define KEY0_MSG         0x10UL
#define KEY1_MSG         0x20UL

static rtos_msgq_t *led_msgq;
static rtos_msgq_t *uart_msgq;

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
  RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;

  GPIOA_CRL &= ~((0xFUL << 0) | (0xFUL << 4));
  GPIOA_CRL |= (0x8UL << 0) | (0x8UL << 4); /* PA0/PA1 input pull-up/down. */
  GPIOA_BSRR = (1UL << 0) | (1UL << 1);     /* Pull-up: buttons active-low. */

  AFIO_EXTICR1 &= ~((0xFUL << 0) | (0xFUL << 4)); /* EXTI0/1 source = PA0/1. */
  EXTI_FTSR |= (1UL << 0) | (1UL << 1);
  EXTI_IMR |= (1UL << 0) | (1UL << 1);

  NVIC_IPR1 &= ~((0xFFUL << 16) | (0xFFUL << 24));
  NVIC_IPR1 |= (0x80UL << 16) | (0x80UL << 24); /* EXTI0/1 priorities. */
  NVIC_ISER0 = (1UL << 6) | (1UL << 7);
}

static void post_key_message(uint32_t msg)
{
  (void)rtos_msgq_send(led_msgq, msg);
  (void)rtos_msgq_send(uart_msgq, msg);
}

void EXTI0_IRQHandler(void)
{
  static uint32_t last_press_ms;
  const uint32_t now = rtos_tick();

  EXTI_PR = (1UL << 0);

  if ((int32_t)(now - last_press_ms) < 200) {
    return;
  }
  last_press_ms = now;

  post_key_message(KEY0_MSG);
}

void EXTI1_IRQHandler(void)
{
  static uint32_t last_press_ms;
  const uint32_t now = rtos_tick();

  EXTI_PR = (1UL << 1);

  if ((int32_t)(now - last_press_ms) < 200) {
    return;
  }
  last_press_ms = now;

  post_key_message(KEY1_MSG);
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

    rtos_delay_ms(20);
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
      printf("[UART thread] key message code=0x%02lX\r\n",
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

    rtos_delay_ms(20);
  }
}

int main(void)
{
  uart1_init();
  led_init();

  led_msgq = rtos_msgq_create();
  uart_msgq = rtos_msgq_create();

  button_init();

  uart1_write_string("\r\nRTOS message queue demo\r\n");
  uart1_write_string("Thread1: PC13 LED, PA0 -> fast blink, PA1 -> slow blink\r\n");
  uart1_write_string("Thread2: USART1 send char, PA0 -> 0x10, PA1 -> 0x20\r\n");
  uart1_write_string("Buttons: PA0/PA1 active-low, UART1: 9600 8N1\r\n\r\n");

  rtos_task_create(led_thread, 0, 256);
  rtos_task_create(uart_thread, 0, 256);
  rtos_start();

  while (1) {
  }
}
