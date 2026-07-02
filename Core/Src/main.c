#include <stdint.h>

#include "dht11.h"
#include "elog.h"
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
#define TEMP_THRESHOLD_C 29U

#define SENSOR_MAIL_SAMPLE 1U
#define SENSOR_MAIL_ERROR  2U

static rtos_mailbox_t *uart_mailbox;
static rtos_mailbox_t *led_mailbox;

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

static void publish_sensor_mail(const rtos_mail_t *mail)
{
  (void)rtos_mailbox_send(uart_mailbox, mail);
  (void)rtos_mailbox_send(led_mailbox, mail);
}

static void sensor_thread(void *arg)
{
  (void)arg;

  /* DHT11 needs a short power-up settling time before the first read. */
  rtos_delay_ms(1000U);

  while (1) {
    uint8_t humidity = 0U;
    uint8_t temperature = 0U;
    const int error = dht11_read(&humidity, &temperature);
    rtos_mail_t mail;

    if (error == DHT11_OK) {
      mail.id = SENSOR_MAIL_SAMPLE;
      mail.value0 = temperature;
      mail.value1 = humidity;
      mail.value2 = rtos_tick();
      elog_d("sensor thread sample temp=%lu C humidity=%lu %%RH",
             (unsigned long)mail.value0,
             (unsigned long)mail.value1);
    } else {
      mail.id = SENSOR_MAIL_ERROR;
      mail.value0 = 0U;
      mail.value1 = 0U;
      mail.value2 = (uint32_t)error;
      elog_e("sensor thread DHT11 read failed error=%lu",
             (unsigned long)mail.value2);
    }

    publish_sensor_mail(&mail);
    rtos_delay_ms(1000U);
  }
}

static void uart_thread(void *arg)
{
  (void)arg;

  while (1) {
    rtos_mail_t mail;

    while (rtos_mailbox_try_recv(uart_mailbox, &mail)) {
      if (mail.id == SENSOR_MAIL_SAMPLE) {
        elog_i("uart thread send temp=%lu C humidity=%lu %%RH",
               (unsigned long)mail.value0,
               (unsigned long)mail.value1);
      } else if (mail.id == SENSOR_MAIL_ERROR) {
        elog_e("uart thread send DHT11 error=%lu",
               (unsigned long)mail.value2);
      }
    }

    rtos_delay_ms(20U);
  }
}

static void led_thread(void *arg)
{
  uint32_t led_is_on = 0U;
  uint32_t blink_period_ms = 800U;
  uint32_t next_toggle_ms = 0U;

  (void)arg;

  while (1) {
    rtos_mail_t mail;
    const uint32_t now = rtos_tick();

    while (rtos_mailbox_try_recv(led_mailbox, &mail)) {
      if (mail.id == SENSOR_MAIL_SAMPLE && mail.value0 > TEMP_THRESHOLD_C) {
        blink_period_ms = 100U;
        elog_i("led thread fast blink temp=%lu C", (unsigned long)mail.value0);
      } else {
        blink_period_ms = 800U;
        if (mail.id == SENSOR_MAIL_SAMPLE) {
          elog_i("led thread slow blink temp=%lu C", (unsigned long)mail.value0);
        } else {
          elog_e("led thread slow blink because sensor error=%lu",
                 (unsigned long)mail.value2);
        }
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
  led_init();
  dht11_init();
  elog_init();
  elog_set_filter_lvl(ELOG_LVL_DEBUG);
  elog_start();

  uart_mailbox = rtos_mailbox_create();
  led_mailbox = rtos_mailbox_create();

  elog_i("RTOS EasyLogger mutex demo start");
  elog_i("DHT11 PC15, UART1 PA9/PA10 9600 8N1");
  elog_i("temperature > 29 C: fast blink, otherwise slow blink");

  rtos_task_create(sensor_thread, 0, 256);
  rtos_task_create(uart_thread, 0, 256);
  rtos_task_create(led_thread, 0, 256);
  rtos_start();

  while (1) {
  }
}
