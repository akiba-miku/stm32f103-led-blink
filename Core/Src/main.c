#include <stdint.h>

#include "elog.h"
#include "uart.h"

#define PERIPH_BASE        0x40000000UL
#define APB2PERIPH_BASE    (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE     (PERIPH_BASE + 0x00020000UL)

#define GPIOC_BASE         (APB2PERIPH_BASE + 0x00001000UL)
#define RCC_BASE           (AHBPERIPH_BASE + 0x00001000UL)
#define SYSTICK_BASE       0xE000E010UL

#define REG32(addr)        (*(volatile uint32_t *)(addr))

#define RCC_APB2ENR        REG32(RCC_BASE + 0x18UL)

#define GPIOC_CRH          REG32(GPIOC_BASE + 0x04UL)
#define GPIOC_BSRR         REG32(GPIOC_BASE + 0x10UL)
#define GPIOC_BRR          REG32(GPIOC_BASE + 0x14UL)

#define SYST_CSR           REG32(SYSTICK_BASE + 0x00UL)
#define SYST_RVR           REG32(SYSTICK_BASE + 0x04UL)
#define SYST_CVR           REG32(SYSTICK_BASE + 0x08UL)

#define RCC_APB2ENR_IOPCEN (1UL << 4)
#define LED_PIN            13UL

static volatile uint32_t systick_ms;

void SysTick_Handler(void)
{
  systick_ms++;
}

static void systick_init(void)
{
  /* STM32F103 starts from 8 MHz HSI after reset: 8000 cycles = 1 ms. */
  SYST_RVR = 8000UL - 1UL;
  SYST_CVR = 0;
  SYST_CSR = (1UL << 2) | (1UL << 1) | (1UL << 0);
}

static uint32_t millis(void)
{
  return systick_ms;
}

static void led_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

  /* PC13: output push-pull, 2 MHz. CRH bits 23:20 = 0b0010. */
  GPIOC_CRH &= ~(0xFUL << 20);
  GPIOC_CRH |=  (0x2UL << 20);

  GPIOC_BSRR = (1UL << LED_PIN); /* Blue Pill LED is active-low: high = off. */
}

static void led_on(void)
{
  GPIOC_BRR = (1UL << LED_PIN);
}

static void led_off(void)
{
  GPIOC_BSRR = (1UL << LED_PIN);
}

int main(void)
{
  enum { RX_FRAME_LEN = 10 };

  systick_init();
  led_init();
  uart1_init();
  elog_init();
  elog_start();
  elog_set_filter_lvl(ELOG_LVL_DEBUG);

  elog_d("debug log demo");
  elog_i("info log demo");
  elog_e("error log demo");

  uint32_t next_led_ms = 0;
  uint32_t led_is_on = 0;
  char rx_frame[RX_FRAME_LEN];
  uint32_t rx_len = 0;

  while (1) {
    const uint32_t now = millis();

    if (uart1_take_rx_frame(rx_frame, sizeof(rx_frame), &rx_len)) {
      for (uint32_t i = 0U; i < rx_len; i++) {
        uart1_write_char(rx_frame[i]);
      }
      uart1_write_string("\r\n");
    }

    if ((int32_t)(now - next_led_ms) >= 0) {
      if (led_is_on) {
        led_off();
        led_is_on = 0;
      } else {
        led_on();
        led_is_on = 1;
      }

      next_led_ms = now + 500U;
    }
  }
}
