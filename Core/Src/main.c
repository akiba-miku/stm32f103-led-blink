#include <stdint.h>
#include <stdio.h>

#include "dht11.h"
#include "lora.h"
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

#define ROLE_TERMINAL    1
#define ROLE_GATEWAY     2

#ifndef NODE_ROLE
#define NODE_ROLE TERMINAL
#endif

#if NODE_ROLE == ROLE_TERMINAL
#define LOCAL_ROLE_CHAR  'T'
#define LOCAL_ROLE_NAME  "Terminal"
#define REMOTE_ROLE_NAME "Gateway"
#elif NODE_ROLE == ROLE_GATEWAY
#define LOCAL_ROLE_CHAR  'G'
#define LOCAL_ROLE_NAME  "Gateway"
#define REMOTE_ROLE_NAME "Terminal"
#else
#error "NODE_ROLE must be TERMINAL or GATEWAY"
#endif

typedef struct {
  uint8_t valid;
  uint8_t temperature;
  uint8_t humidity;
  int error;
  uint32_t sequence;
  uint32_t updated_ms;
} sensor_view_t;

static sensor_view_t local_view;
static sensor_view_t remote_view;

static void led_gpio_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

  GPIOC_CRH &= ~(0xFUL << 20);
  GPIOC_CRH |= (0x2UL << 20);
  GPIOC_BSRR = (1UL << LED_PIN);
}

static void led_set(uint8_t on)
{
  if (on != 0U) {
    GPIOC_BRR = (1UL << LED_PIN);
  } else {
    GPIOC_BSRR = (1UL << LED_PIN);
  }
}

static uint32_t age_seconds(const sensor_view_t *view, uint32_t now)
{
  if (view->valid == 0U) {
    return 0U;
  }

  return (now - view->updated_ms) / 1000U;
}

static void print_sample_row(const char *label, const sensor_view_t *view,
                             uint32_t now)
{
  if (view->valid == 0U) {
    printf("| %-8s |   --- C |   --- %%RH | waiting               |\r\n", label);
    return;
  }

  if (view->error == DHT11_OK) {
    printf("| %-8s | %5u C | %6u %%RH | seq=%-4lu age=%-3lus |\r\n",
           label,
           (unsigned)view->temperature,
           (unsigned)view->humidity,
           (unsigned long)view->sequence,
           (unsigned long)age_seconds(view, now));
  } else {
    printf("| %-8s |   --- C |   --- %%RH | DHT11 error=%-8d |\r\n",
           label,
           view->error);
  }
}

static void print_terminal_ui(uint32_t runtime_s)
{
  const uint32_t now = rtos_tick();

  printf("\033[2J\033[H");
  printf("+-------------------------------------------------------------+\r\n");
  printf("|                  LoRa Temperature Gateway                   |\r\n");
  printf("+-------------------------------------------------------------+\r\n");
  printf("| Node role : %-8s                                      |\r\n",
         LOCAL_ROLE_NAME);
  printf("| Debug UART: USART1 PA9/PA10 115200 8N1                     |\r\n");
  printf("| LoRa UART : USART2 PA2/PA3   9600 8N1 transparent mode      |\r\n");
  printf("| DHT11     : PC15                                            |\r\n");
  printf("+----------+---------+----------+-----------------------------+\r\n");
  printf("| Source   | Temp    | Humidity | Status                      |\r\n");
  printf("+----------+---------+----------+-----------------------------+\r\n");
  print_sample_row("Local", &local_view, now);
  print_sample_row("Remote", &remote_view, now);
  printf("+----------+---------+----------+-----------------------------+\r\n");
  printf("| Local program : %-8s  Remote expected : %-8s           |\r\n",
         LOCAL_ROLE_NAME, REMOTE_ROLE_NAME);
  printf("| Runtime       : %-8lu s                                  |\r\n",
         (unsigned long)runtime_s);
  printf("+-------------------------------------------------------------+\r\n");
  printf("Video: show both boards/LoRa modules or show one node receiving remote data.\r\n");
}

static void update_local_sample(uint32_t sequence)
{
  uint8_t humidity = 0U;
  uint8_t temperature = 0U;
  const int error = dht11_read(&humidity, &temperature);

  local_view.valid = 1U;
  local_view.temperature = temperature;
  local_view.humidity = humidity;
  local_view.error = error;
  local_view.sequence = sequence;
  local_view.updated_ms = rtos_tick();

  lora_send_sample((uint8_t)LOCAL_ROLE_CHAR, sequence, temperature, humidity,
                   error);
}

static void poll_remote_samples(void)
{
  lora_sample_t sample;

  while (lora_poll_sample(&sample)) {
    if (sample.role == (uint8_t)LOCAL_ROLE_CHAR) {
      continue;
    }

    remote_view.valid = 1U;
    remote_view.temperature = sample.temperature;
    remote_view.humidity = sample.humidity;
    remote_view.error = sample.error;
    remote_view.sequence = sample.sequence;
    remote_view.updated_ms = rtos_tick();
  }
}

static void app_task(void *arg)
{
  uint8_t led_state = 0U;
  uint32_t sequence = 0U;
  uint32_t runtime_s = 0U;
  uint32_t next_sample_ms = 0U;
  uint32_t next_print_ms = 0U;

  (void)arg;

  while (1) {
    const uint32_t now = rtos_tick();

    poll_remote_samples();

    if ((int32_t)(now - next_sample_ms) >= 0) {
      update_local_sample(sequence);
      sequence++;
      next_sample_ms = now + 1000U;
      runtime_s++;
      led_state ^= 1U;
      led_set(led_state);
    }

    if ((int32_t)(now - next_print_ms) >= 0) {
      print_terminal_ui(runtime_s);
      next_print_ms = now + 500U;
    }

    rtos_delay_ms(20U);
  }
}

int main(void)
{
  uart1_init();
  lora_init();
  led_gpio_init();
  dht11_init();

  rtos_task_create(app_task, 0, 256);
  rtos_start();

  while (1) {
  }
}
