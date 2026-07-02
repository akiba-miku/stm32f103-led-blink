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

#define GPIOC_CRH        REG32(GPIOC_BASE + 0x04UL)
#define GPIOC_BSRR       REG32(GPIOC_BASE + 0x10UL)
#define GPIOC_BRR        REG32(GPIOC_BASE + 0x14UL)

#define NVIC_ISER0       REG32(0xE000E100UL)
#define NVIC_IPR7        REG32(0xE000E41CUL)

#define RCC_APB1ENR_TIM2EN (1UL << 0)
#define RCC_APB2ENR_IOPCEN (1UL << 4)

#define TIM_CR1_CEN      (1UL << 0)
#define TIM_DIER_UIE     (1UL << 0)
#define TIM_SR_UIF       (1UL << 0)
#define TIM_EGR_UG       (1UL << 0)

#define TIM2_IRQN        28UL
#define LED_PIN          13UL
#define TEMP_THRESHOLD_C 29U

#define SENSOR_MAIL_SAMPLE 1U
#define SENSOR_MAIL_ERROR  2U

static rtos_sem_t *sensor_timer_sem;
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

static void tim2_1hz_init(void)
{
  RCC_APB1ENR |= RCC_APB1ENR_TIM2EN;

  TIM2_CR1 = 0U;
  TIM2_PSC = 7999U; /* 8 MHz / (7999 + 1) = 1 kHz. */
  TIM2_ARR = 999U;  /* 1 kHz / (999 + 1) = 1 Hz. */
  TIM2_EGR = TIM_EGR_UG;
  TIM2_SR = 0U;
  TIM2_DIER = TIM_DIER_UIE;

  NVIC_IPR7 = (NVIC_IPR7 & ~0x000000FFUL) | 0x00000080UL;
  NVIC_ISER0 = (1UL << TIM2_IRQN);

  TIM2_CR1 = TIM_CR1_CEN;
}

static void publish_sensor_mail(const rtos_mail_t *mail)
{
  (void)rtos_mailbox_send(uart_mailbox, mail);
  (void)rtos_mailbox_send(led_mailbox, mail);
}

void TIM2_IRQHandler(void)
{
  if ((TIM2_SR & TIM_SR_UIF) == 0U) {
    return;
  }

  TIM2_SR &= ~TIM_SR_UIF;

  if (sensor_timer_sem != 0) {
    rtos_sem_signal(sensor_timer_sem);
  }
}

static void sensor_thread(void *arg)
{
  (void)arg;

  while (1) {
    uint8_t humidity = 0U;
    uint8_t temperature = 0U;
    int error;
    rtos_mail_t mail;

    rtos_sem_wait(sensor_timer_sem);
    error = dht11_read(&humidity, &temperature);

    if (error == DHT11_OK) {
      mail.id = SENSOR_MAIL_SAMPLE;
      mail.value0 = temperature;
      mail.value1 = humidity;
      mail.value2 = rtos_tick();
    } else {
      mail.id = SENSOR_MAIL_ERROR;
      mail.value0 = 0U;
      mail.value1 = 0U;
      mail.value2 = (uint32_t)error;
    }

    publish_sensor_mail(&mail);
  }
}

static void uart_thread(void *arg)
{
  (void)arg;

  while (1) {
    rtos_mail_t mail;

    while (rtos_mailbox_try_recv(uart_mailbox, &mail)) {
      if (mail.id == SENSOR_MAIL_SAMPLE) {
        printf("[UART thread] temp=%lu C, humidity=%lu %%RH\r\n",
               (unsigned long)mail.value0,
               (unsigned long)mail.value1);
      } else if (mail.id == SENSOR_MAIL_ERROR) {
        printf("[UART thread] DHT11 read failed, error=%lu\r\n",
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
      } else {
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

int main(void)
{
  uart1_init();
  led_init();
  dht11_init();

  sensor_timer_sem = rtos_sem_create(0);
  uart_mailbox = rtos_mailbox_create();
  led_mailbox = rtos_mailbox_create();

  uart1_write_string("\r\nRTOS timer DHT11 demo\r\n");
  uart1_write_string("TIM2: trigger DHT11 sampling every 1 second\r\n");
  uart1_write_string("UART thread: print temperature and humidity via USART1\r\n");
  uart1_write_string("LED thread: PC13 fast blink when temp > 29 C, otherwise slow blink\r\n");
  uart1_write_string("DHT11 DATA: PC15, UART1: PA9 TX / PA10 RX, 9600 8N1\r\n\r\n");

  rtos_task_create(sensor_thread, 0, 256);
  rtos_task_create(uart_thread, 0, 256);
  rtos_task_create(led_thread, 0, 256);

  tim2_1hz_init();
  rtos_start();

  while (1) {
  }
}
