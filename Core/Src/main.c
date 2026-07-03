#include <stdint.h>
#include <stdio.h>

#include "dht11.h"
#include "rtos.h"
#include "uart.h"

#define PERIPH_BASE      0x40000000UL
#define APB2PERIPH_BASE  (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE   (PERIPH_BASE + 0x00020000UL)

#define GPIOC_BASE       (APB2PERIPH_BASE + 0x00001000UL)
#define RCC_BASE         (AHBPERIPH_BASE + 0x00001000UL)

#define REG32(addr)      (*(volatile uint32_t *)(addr))

#define RCC_APB2ENR      REG32(RCC_BASE + 0x18UL)
#define GPIOC_CRH        REG32(GPIOC_BASE + 0x04UL)
#define GPIOC_BSRR       REG32(GPIOC_BASE + 0x10UL)
#define GPIOC_BRR        REG32(GPIOC_BASE + 0x14UL)

#define RCC_APB2ENR_IOPCEN (1UL << 4)
#define LED_PIN          13UL

static void led_gpio_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

  GPIOC_CRH &= ~(0xFUL << 20);
  GPIOC_CRH |= (0x2UL << 20);
  GPIOC_BSRR = (1UL << LED_PIN);
}

static void led_on(void)
{
  GPIOC_BRR = (1UL << LED_PIN);
}

static void led_off(void)
{
  GPIOC_BSRR = (1UL << LED_PIN);
}

static void print_terminal_ui(uint32_t seconds, int error,
                              uint8_t temperature, uint8_t humidity)
{
  printf("\033[2J\033[H");
  printf("+--------------------------------------------------+\r\n");
  printf("|              Wireless Access System              |\r\n");
  printf("+--------------------------------------------------+\r\n");
  printf("| Device : STM32F103C8T6 Blue Pill                 |\r\n");
  printf("| Sensor : DHT11 on PC15                           |\r\n");
  printf("| UART   : USART1 115200 8N1                       |\r\n");
  printf("+--------------------------------------------------+\r\n");

  if (error == DHT11_OK) {
    printf("| Temperature : %3u C                              |\r\n",
           (unsigned)temperature);
    printf("| Humidity    : %3u %%RH                            |\r\n",
           (unsigned)humidity);
    printf("| Status      : OK                                 |\r\n");
  } else {
    printf("| Temperature : --- C                              |\r\n");
    printf("| Humidity    : --- %%RH                            |\r\n");
    printf("| Status      : DHT11 read failed, error=%d        |\r\n",
           error);
  }

  printf("+--------------------------------------------------+\r\n");
  printf("| Refresh     : 1 second                           |\r\n");
  printf("| Runtime     : %lu s                              |\r\n",
         (unsigned long)seconds);
  printf("+--------------------------------------------------+\r\n");
  printf("For video: keep this terminal and the sensor/board in frame.\r\n");
}

static void app_task(void *arg)
{
  uint8_t led_state = 0U;
  uint32_t seconds = 0U;

  (void)arg;

  while (1) {
    uint8_t humidity = 0U;
    uint8_t temperature = 0U;
    const int error = dht11_read(&humidity, &temperature);

    print_terminal_ui(seconds, error, temperature, humidity);

    if (led_state == 0U) {
      led_on();
      led_state = 1U;
    } else {
      led_off();
      led_state = 0U;
    }

    seconds++;
    rtos_delay_ms(1000U);
  }
}

int main(void)
{
  uart1_init();
  led_gpio_init();
  dht11_init();

  rtos_task_create(app_task, 0, 256);
  rtos_start();

  while (1) {
  }
}
