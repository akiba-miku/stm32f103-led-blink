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

static rtos_sem_t *led_signal;
static rtos_sem_t *uart_signal;

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

  GPIOA_CRL &= ~(0xFUL << 0);
  GPIOA_CRL |= (0x8UL << 0); /* PA0: input pull-up/down. */
  GPIOA_BSRR = (1UL << 0);   /* Pull-up: button active-low. */

  AFIO_EXTICR1 &= ~(0xFUL << 0); /* EXTI0 source = PA0. */
  EXTI_FTSR |= (1UL << 0);
  EXTI_IMR |= (1UL << 0);

  NVIC_IPR1 &= ~(0xFFUL << 16);
  NVIC_IPR1 |= (0x80UL << 16); /* EXTI0 = IRQ6. */
  NVIC_ISER0 = (1UL << 6);
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

  rtos_sem_signal(led_signal);
  rtos_sem_signal(uart_signal);
}

static void led_thread(void *arg)
{
  uint32_t led_is_on = 0U;

  (void)arg;

  while (1) {
    led_toggle(&led_is_on);

    if (rtos_sem_try_wait(led_signal)) {
      for (uint32_t i = 0U; i < 6U; i++) {
        led_toggle(&led_is_on);
        rtos_delay_ms(80);
      }
      led_off();
      led_is_on = 0U;
    }

    rtos_delay_ms(500);
  }
}

static void uart_thread(void *arg)
{
  char next_char = 'A';

  (void)arg;

  while (1) {
    printf("[UART thread] send char: %c\r\n", next_char);

    next_char++;
    if (next_char > 'Z') {
      next_char = 'A';
    }

    if (rtos_sem_try_wait(uart_signal)) {
      printf("[UART thread] button signal received at %lu ms\r\n",
             (unsigned long)rtos_tick());
    }

    rtos_delay_ms(1000);
  }
}

int main(void)
{
  uart1_init();
  led_init();
  button_init();

  uart1_write_string("\r\nRTOS semaphore sync demo\r\n");
  uart1_write_string("Thread1: PC13 LED blink, button -> rapid blink\r\n");
  uart1_write_string("Thread2: USART1 send char, button -> response message\r\n");
  uart1_write_string("Button: PA0 active-low, UART1: 9600 8N1\r\n\r\n");

  led_signal = rtos_sem_create(0);
  uart_signal = rtos_sem_create(0);

  rtos_task_create(led_thread, 0, 256);
  rtos_task_create(uart_thread, 0, 256);
  rtos_start();

  while (1) {
  }
}
