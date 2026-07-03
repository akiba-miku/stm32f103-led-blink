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

#define ROLE_TERMINAL    1U
#define ROLE_GATEWAY     2U
#define PAGE_MAIN        0U
#define PAGE_CONFIG      1U

#ifndef NODE_ROLE
#define NODE_ROLE ROLE_TERMINAL
#endif

typedef struct {
  uint8_t valid;
  uint8_t temperature;
  uint8_t humidity;
  int error;
  uint32_t sequence;
  uint32_t updated_ms;
} sensor_view_t;

typedef struct {
  uint8_t role;
  uint8_t page;
  uint32_t sample_interval_ms;
  uint32_t lora_baud;
} app_config_t;

static sensor_view_t local_view;
static sensor_view_t remote_view;
static char last_command = '-';
static app_config_t config = {
#if NODE_ROLE == ROLE_GATEWAY
  ROLE_GATEWAY,
#else
  ROLE_TERMINAL,
#endif
  PAGE_MAIN,
  1000U,
  9600U
};

static const uint32_t lora_bauds[] = {9600U, 19200U, 115200U};

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

static char local_role_char(void)
{
  return (config.role == ROLE_GATEWAY) ? 'G' : 'T';
}

static const char *local_role_name(void)
{
  return (config.role == ROLE_GATEWAY) ? "Gateway" : "Terminal";
}

static const char *remote_role_name(void)
{
  return (config.role == ROLE_GATEWAY) ? "Terminal" : "Gateway";
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

static void print_main_page(uint32_t runtime_s)
{
  const uint32_t now = rtos_tick();

  printf("\033[2J\033[H");
  printf("+-------------------------------------------------------------+\r\n");
  printf("|                  LoRa Temperature System                    |\r\n");
  printf("+-------------------------------------------------------------+\r\n");
  printf("| Node role : %-8s                                      |\r\n",
         local_role_name());
  printf("| Debug UART: USART1 PA9/PA10 115200 8N1                     |\r\n");
  printf("| LoRa UART : USART2 PA2/PA3   %-6lu 8N1 transparent mode    |\r\n",
         (unsigned long)config.lora_baud);
  printf("| DHT11     : PC15                                            |\r\n");
  printf("+----------+---------+----------+-----------------------------+\r\n");
  printf("| Source   | Temp    | Humidity | Status                      |\r\n");
  printf("+----------+---------+----------+-----------------------------+\r\n");
  print_sample_row("Local", &local_view, now);
  print_sample_row("Remote", &remote_view, now);
  printf("+----------+---------+----------+-----------------------------+\r\n");
  printf("| Local role : %-8s  Remote expected : %-8s             |\r\n",
         local_role_name(), remote_role_name());
  printf("| Interval   : %-4lu ms   Runtime : %-8lu s              |\r\n",
         (unsigned long)config.sample_interval_ms,
         (unsigned long)runtime_s);
  printf("+-------------------------------------------------------------+\r\n");
  printf("Keys: c=config, r=role, +/-=interval, b=baud  Last key: %c\r\n",
         last_command);
}

static void print_config_page(void)
{
  printf("\033[2J\033[H");
  printf("+-------------------------------------------------------------+\r\n");
  printf("|                     System Configuration                    |\r\n");
  printf("+-------------------------------------------------------------+\r\n");
  printf("| Current role         : %-8s                            |\r\n",
         local_role_name());
  printf("| Remote expected      : %-8s                            |\r\n",
         remote_role_name());
  printf("| Sample interval      : %-4lu ms                           |\r\n",
         (unsigned long)config.sample_interval_ms);
  printf("| LoRa UART baud       : %-6lu 8N1                          |\r\n",
         (unsigned long)config.lora_baud);
  printf("| DHT11 data pin       : PC15                                |\r\n");
  printf("| LoRa pins            : PA2 TX / PA3 RX                     |\r\n");
  printf("+-------------------------------------------------------------+\r\n");
  printf("| Commands                                                    |\r\n");
  printf("| r : switch Terminal/Gateway role                            |\r\n");
  printf("| + : increase sample interval                                |\r\n");
  printf("| - : decrease sample interval                                |\r\n");
  printf("| b : cycle LoRa baud 9600/19200/115200                       |\r\n");
  printf("| m : return to main page                                     |\r\n");
  printf("| c : stay on configuration page                              |\r\n");
  printf("+-------------------------------------------------------------+\r\n");
  printf("Last key: %c. Changes take effect immediately.\r\n", last_command);
}

static void print_current_page(uint32_t runtime_s)
{
  if (config.page == PAGE_CONFIG) {
    print_config_page();
  } else {
    print_main_page(runtime_s);
  }
}

static void cycle_lora_baud(void)
{
  uint32_t i;

  for (i = 0U; i < (sizeof(lora_bauds) / sizeof(lora_bauds[0])); i++) {
    if (lora_bauds[i] == config.lora_baud) {
      break;
    }
  }

  i++;
  if (i >= (sizeof(lora_bauds) / sizeof(lora_bauds[0]))) {
    i = 0U;
  }

  config.lora_baud = lora_bauds[i];
  lora_set_baud(config.lora_baud);
}

static int handle_command(char ch)
{
  if (ch == '\r' || ch == '\n') {
    return 0;
  }

  if (ch >= 'A' && ch <= 'Z') {
    ch = (char)(ch - 'A' + 'a');
  }

  if (ch == 'c') {
    config.page = PAGE_CONFIG;
  } else if (ch == 'm') {
    config.page = PAGE_MAIN;
  } else if (ch == 'r') {
    config.role = (config.role == ROLE_GATEWAY) ? ROLE_TERMINAL : ROLE_GATEWAY;
    remote_view.valid = 0U;
  } else if (ch == '+') {
    if (config.sample_interval_ms < 10000U) {
      config.sample_interval_ms += 1000U;
    }
  } else if (ch == '-') {
    if (config.sample_interval_ms > 1000U) {
      config.sample_interval_ms -= 1000U;
    }
  } else if (ch == 'b') {
    cycle_lora_baud();
  } else {
    return 0;
  }

  last_command = ch;
  return 1;
}

static int poll_commands(void)
{
  char ch;
  int changed = 0;

  while (uart1_read_char(&ch)) {
    if (handle_command(ch)) {
      changed = 1;
    }
  }

  return changed;
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

  lora_send_sample((uint8_t)local_role_char(), sequence, temperature, humidity,
                   error);
}

static int poll_remote_samples(void)
{
  lora_sample_t sample;
  int changed = 0;

  while (lora_poll_sample(&sample)) {
    if (sample.role == (uint8_t)local_role_char()) {
      continue;
    }

    remote_view.valid = 1U;
    remote_view.temperature = sample.temperature;
    remote_view.humidity = sample.humidity;
    remote_view.error = sample.error;
    remote_view.sequence = sample.sequence;
    remote_view.updated_ms = rtos_tick();
    changed = 1;
  }

  return changed;
}

static void app_task(void *arg)
{
  uint8_t led_state = 0U;
  uint32_t sequence = 0U;
  uint32_t next_sample_ms = 0U;
  int screen_dirty = 1;

  (void)arg;

  while (1) {
    const uint32_t now = rtos_tick();

    if (poll_commands()) {
      screen_dirty = 1;
    }
    if (poll_remote_samples()) {
      screen_dirty = 1;
    }

    if ((int32_t)(now - next_sample_ms) >= 0) {
      update_local_sample(sequence);
      sequence++;
      next_sample_ms = now + config.sample_interval_ms;
      led_state ^= 1U;
      led_set(led_state);
      screen_dirty = 1;
    }

    if (screen_dirty) {
      print_current_page(now / 1000U);
      screen_dirty = 0;
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
