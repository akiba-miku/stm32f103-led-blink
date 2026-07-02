#include <stdint.h>
#include <stdio.h>

#include "dht11.h"
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
  SYST_CVR = 0U;
  SYST_CSR = (1UL << 2) | (1UL << 1) | (1UL << 0);
}

static uint32_t millis(void)
{
  return systick_ms;
}

static void led_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

  GPIOC_CRH &= ~(0xFUL << 20);
  GPIOC_CRH |= (0x2UL << 20); /* PC13: output push-pull, 2 MHz. */

  GPIOC_BSRR = (1UL << LED_PIN); /* Blue Pill LED is active-low. */
}

static void led_toggle(uint32_t *is_on)
{
  if (*is_on != 0U) {
    GPIOC_BSRR = (1UL << LED_PIN);
    *is_on = 0U;
  } else {
    GPIOC_BRR = (1UL << LED_PIN);
    *is_on = 1U;
  }
}

int main(void)
{
  uint32_t next_read_ms = 1000U;
  uint32_t next_led_ms = 0U;
  uint32_t led_is_on = 0U;

  systick_init();
  uart1_init();
  led_init();
  dht11_init();

  uart1_write_string("\r\nDHT11 temperature/humidity demo\r\n");
  uart1_write_string("DHT11 DATA: PC15, UART1: 9600 8N1\r\n");

  while (1) {
    const uint32_t now = millis();

    if ((int32_t)(now - next_read_ms) >= 0) {
      uint8_t humidity = 0U;
      uint8_t temperature = 0U;
      const int status = dht11_read(&humidity, &temperature);

      if (status == DHT11_OK) {
        printf("Humidity=%u%%RH Temperature=%uC\r\n",
               (unsigned int)humidity,
               (unsigned int)temperature);
      } else {
        printf("DHT11 read failed, error=%d\r\n", status);
      }

      next_read_ms = now + 1000U;
    }

    if ((int32_t)(now - next_led_ms) >= 0) {
      led_toggle(&led_is_on);
      next_led_ms = now + 500U;
    }
  }
}
