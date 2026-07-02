#include <stdint.h>
#include <stdio.h>

#include "dht11.h"
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

#define KEY0_CODE        0x10UL
#define KEY1_CODE        0x20UL
#define KEY_DEBOUNCE_TICKS 3U

#define HUMIDITY_HIGH_THRESHOLD 60U
#define LED_FAST_BLINK_MS       100U
#define LED_SLOW_BLINK_MS       800U

enum {
  APP_EVT_KEY_RELEASED = 1,
  APP_EVT_SENSOR_SAMPLE_DUE = 2,
};

enum {
  UART_EVT_PRINT_SAMPLE = 1,
  UART_EVT_PRINT_ERROR = 2,
};

enum {
  LED_EVT_HUMIDITY = 1,
  LED_EVT_SENSOR_ERROR = 2,
};

typedef struct {
  uint32_t pin;
  uint32_t code;
  uint8_t stable_pressed;
  uint8_t candidate_pressed;
  uint8_t debounce_ticks;
} key_button_t;

typedef struct {
  uint8_t valid;
  uint8_t temperature;
  uint8_t humidity;
  uint32_t tick;
  int error;
} sensor_state_t;

static rtos_mailbox_t *app_event_q;
static rtos_mailbox_t *uart_event_q;
static rtos_mailbox_t *led_event_q;

static sensor_state_t sensor_state;

static key_button_t key_buttons[] = {
  {KEY0_PIN, KEY0_CODE, 0U, 0U, 0U},
  {KEY1_PIN, KEY1_CODE, 0U, 0U, 0U},
};

static void app_post_event(uint32_t id, uint32_t value0, uint32_t value1,
                           uint32_t value2)
{
  rtos_mail_t event;

  event.id = id;
  event.value0 = value0;
  event.value1 = value1;
  event.value2 = value2;

  if (app_event_q != 0) {
    (void)rtos_mailbox_send(app_event_q, &event);
  }
}

static void uart_post_event(uint32_t id, uint32_t value0, uint32_t value1,
                            uint32_t value2)
{
  rtos_mail_t event;

  event.id = id;
  event.value0 = value0;
  event.value1 = value1;
  event.value2 = value2;

  if (uart_event_q != 0) {
    (void)rtos_mailbox_send(uart_event_q, &event);
  }
}

static void led_post_event(uint32_t id, uint32_t value0, uint32_t value1,
                           uint32_t value2)
{
  rtos_mail_t event;

  event.id = id;
  event.value0 = value0;
  event.value1 = value1;
  event.value2 = value2;

  if (led_event_q != 0) {
    (void)rtos_mailbox_send(led_event_q, &event);
  }
}

static void led_gpio_init(void)
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

static void key_scan_init(void)
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

static void key_scan_poll_button(key_button_t *button)
{
  const uint8_t raw_pressed = (uint8_t)key_is_pressed(button->pin);

  if (raw_pressed == button->candidate_pressed) {
    if (button->debounce_ticks < KEY_DEBOUNCE_TICKS) {
      button->debounce_ticks++;
    }
  } else {
    button->candidate_pressed = raw_pressed;
    button->debounce_ticks = 1U;
  }

  if (button->debounce_ticks < KEY_DEBOUNCE_TICKS ||
      button->stable_pressed == button->candidate_pressed) {
    return;
  }

  button->stable_pressed = button->candidate_pressed;
  if (button->stable_pressed == 0U) {
    app_post_event(APP_EVT_KEY_RELEASED, button->code, 0U, rtos_tick());
  }
}

static void key_scan_poll_10ms(void)
{
  for (uint32_t i = 0U; i < (sizeof(key_buttons) / sizeof(key_buttons[0])); i++) {
    key_scan_poll_button(&key_buttons[i]);
  }
}

static void timer_event_init(void)
{
  RCC_APB1ENR |= RCC_APB1ENR_TIM2EN;

  TIM2_CR1 = 0U;
  TIM2_PSC = 7999U; /* 8 MHz / (7999 + 1) = 1 kHz. */
  TIM2_ARR = 9U;    /* 1 kHz / (9 + 1) = 100 Hz, tick every 10 ms. */
  TIM2_EGR = TIM_EGR_UG;
  TIM2_SR = 0U;
  TIM2_DIER = TIM_DIER_UIE;

  NVIC_IPR7 = (NVIC_IPR7 & ~0x000000FFUL) | 0x00000080UL;
  NVIC_ISER0 = (1UL << TIM2_IRQN);

  TIM2_CR1 = TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
  static uint32_t sensor_ticks;

  if ((TIM2_SR & TIM_SR_UIF) == 0U) {
    return;
  }

  TIM2_SR &= ~TIM_SR_UIF;

  key_scan_poll_10ms();

  sensor_ticks++;
  if (sensor_ticks >= 100U) {
    sensor_ticks = 0U;
    app_post_event(APP_EVT_SENSOR_SAMPLE_DUE, 0U, 0U, rtos_tick());
  }
}

static void sensor_module_sample(void)
{
  uint8_t humidity = 0U;
  uint8_t temperature = 0U;
  const int error = dht11_read(&humidity, &temperature);

  sensor_state.error = error;
  sensor_state.tick = rtos_tick();

  if (error == DHT11_OK) {
    sensor_state.valid = 1U;
    sensor_state.temperature = temperature;
    sensor_state.humidity = humidity;
    led_post_event(LED_EVT_HUMIDITY, humidity, temperature, sensor_state.tick);
  } else {
    sensor_state.valid = 0U;
    led_post_event(LED_EVT_SENSOR_ERROR, (uint32_t)error, 0U, sensor_state.tick);
  }
}

static void uart_module_print_current_sample(uint32_t key_code)
{
  if (sensor_state.valid != 0U) {
    uart_post_event(UART_EVT_PRINT_SAMPLE,
                    key_code,
                    sensor_state.temperature,
                    sensor_state.humidity);
  } else {
    uart_post_event(UART_EVT_PRINT_ERROR,
                    key_code,
                    (uint32_t)sensor_state.error,
                    sensor_state.tick);
  }
}

static void event_thread(void *arg)
{
  (void)arg;

  while (1) {
    rtos_mail_t event;

    while (rtos_mailbox_try_recv(app_event_q, &event)) {
      if (event.id == APP_EVT_SENSOR_SAMPLE_DUE) {
        sensor_module_sample();
      } else if (event.id == APP_EVT_KEY_RELEASED) {
        uart_module_print_current_sample(event.value0);
      }
    }

    rtos_delay_ms(10U);
  }
}

static void uart_thread(void *arg)
{
  (void)arg;

  while (1) {
    rtos_mail_t event;

    while (rtos_mailbox_try_recv(uart_event_q, &event)) {
      if (event.id == UART_EVT_PRINT_SAMPLE) {
        printf("[UART] key=0x%02lX temp=%lu C humidity=%lu %%RH\r\n",
               (unsigned long)event.value0,
               (unsigned long)event.value1,
               (unsigned long)event.value2);
      } else if (event.id == UART_EVT_PRINT_ERROR) {
        if (event.value2 == 0U) {
          printf("[UART] key=0x%02lX no sensor sample yet\r\n",
                 (unsigned long)event.value0);
        } else {
          printf("[UART] key=0x%02lX DHT11 read failed, error=%lu\r\n",
                 (unsigned long)event.value0,
                 (unsigned long)event.value1);
        }
      }
    }

    rtos_delay_ms(20U);
  }
}

static void led_thread(void *arg)
{
  uint32_t led_is_on = 0U;
  uint32_t blink_period_ms = LED_SLOW_BLINK_MS;
  uint32_t next_toggle_ms = 0U;

  (void)arg;

  while (1) {
    rtos_mail_t event;
    const uint32_t now = rtos_tick();

    while (rtos_mailbox_try_recv(led_event_q, &event)) {
      if (event.id == LED_EVT_HUMIDITY) {
        if (event.value0 >= HUMIDITY_HIGH_THRESHOLD) {
          blink_period_ms = LED_FAST_BLINK_MS;
        } else {
          blink_period_ms = LED_SLOW_BLINK_MS;
        }
      } else if (event.id == LED_EVT_SENSOR_ERROR) {
        blink_period_ms = LED_SLOW_BLINK_MS;
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

int main(void)
{
  uart1_init();
  led_gpio_init();
  key_scan_init();
  dht11_init();

  app_event_q = rtos_mailbox_create();
  uart_event_q = rtos_mailbox_create();
  led_event_q = rtos_mailbox_create();

  uart1_write_string("\r\nEvent-driven DHT11 key demo\r\n");
  uart1_write_string("TIM2: key scan every 10 ms, DHT11 sample every 1 second\r\n");
  uart1_write_string("Press and release PA0/PA1 to print current temperature/humidity\r\n");
  uart1_write_string("LED: humidity >= 60%RH fast blink, otherwise slow blink\r\n");
  uart1_write_string("DHT11 DATA: PC15, UART1: PA9 TX / PA10 RX, 9600 8N1\r\n\r\n");

  rtos_task_create(event_thread, 0, 256);
  rtos_task_create(uart_thread, 0, 256);
  rtos_task_create(led_thread, 0, 256);

  timer_event_init();
  rtos_start();

  while (1) {
  }
}
