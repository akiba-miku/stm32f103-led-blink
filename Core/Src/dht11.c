#include "dht11.h"

#include "elog.h"

/*
 * DHT11 DATA is connected to PC15.
 * The board clock is the STM32F103 reset default: 8 MHz HSI.
 */

#define PERIPH_BASE      0x40000000UL
#define APB2PERIPH_BASE  (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE   (PERIPH_BASE + 0x00020000UL)

#define GPIOC_BASE       (APB2PERIPH_BASE + 0x00001000UL)
#define RCC_BASE         (AHBPERIPH_BASE + 0x00001000UL)

#define RCC_APB2ENR      (*(volatile uint32_t *)(RCC_BASE + 0x18UL))
#define RCC_APB2ENR_IOPCEN (1UL << 4)

#define GPIOC_CRH        (*(volatile uint32_t *)(GPIOC_BASE + 0x04UL))
#define GPIOC_IDR        (*(volatile uint32_t *)(GPIOC_BASE + 0x08UL))
#define GPIOC_BSRR       (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))
#define GPIOC_BRR        (*(volatile uint32_t *)(GPIOC_BASE + 0x14UL))

#define DEMCR            (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL         (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT       (*(volatile uint32_t *)0xE0001004UL)

#define DEMCR_TRCENA     (1UL << 24)
#define DWT_CTRL_CYCCNTENA (1UL << 0)

#define DHT11_PIN        15U
#define DHT11_CRH_SHIFT  ((DHT11_PIN - 8U) * 4U)
#define CYCLES_PER_US    8U

static void dwt_init(void)
{
  DEMCR |= DEMCR_TRCENA;
  DWT_CYCCNT = 0U;
  DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

static uint32_t cycles_now(void)
{
  return DWT_CYCCNT;
}

static void delay_us(uint32_t us)
{
  const uint32_t start = cycles_now();
  const uint32_t ticks = us * CYCLES_PER_US;

  while ((cycles_now() - start) < ticks) {
  }
}

static uint32_t elapsed_us(uint32_t start)
{
  return (cycles_now() - start) / CYCLES_PER_US;
}

static void data_output_open_drain(void)
{
  GPIOC_CRH &= ~(0xFUL << DHT11_CRH_SHIFT);
  GPIOC_CRH |= (0x6UL << DHT11_CRH_SHIFT); /* output open-drain, 2 MHz */
}

static void data_input_pullup(void)
{
  GPIOC_CRH &= ~(0xFUL << DHT11_CRH_SHIFT);
  GPIOC_CRH |= (0x8UL << DHT11_CRH_SHIFT); /* input pull-up/down */
  GPIOC_BSRR = (1UL << DHT11_PIN);         /* pull-up */
}

static void data_low(void)
{
  GPIOC_BRR = (1UL << DHT11_PIN);
}

static void data_release(void)
{
  GPIOC_BSRR = (1UL << DHT11_PIN);
}

static int data_read(void)
{
  return (int)((GPIOC_IDR >> DHT11_PIN) & 1U);
}

static uint32_t irq_save(void)
{
  uint32_t primask;
  __asm volatile("mrs %0, primask" : "=r"(primask));
  __asm volatile("cpsid i" ::: "memory");
  return primask;
}

static void irq_restore(uint32_t primask)
{
  if ((primask & 1U) == 0U) {
    __asm volatile("cpsie i" ::: "memory");
  }
}

static int wait_data_level(int level, uint32_t timeout_us)
{
  const uint32_t start = cycles_now();
  const uint32_t timeout_ticks = timeout_us * CYCLES_PER_US;

  while (data_read() != level) {
    if ((cycles_now() - start) > timeout_ticks) {
      return 0;
    }
  }

  return 1;
}

static void print_diag(const char *message)
{
  elog_e("DHT11 error: %s", message);
}

void dht11_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;
  dwt_init();
  data_input_pullup();
}

int dht11_read(uint8_t *humidity_int, uint8_t *temperature_int)
{
  uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
  const char *diag = 0;
  uint32_t primask;
  int result = DHT11_OK;

  primask = irq_save();
  data_output_open_drain();
  data_low();
  delay_us(18000U);

  data_release();
  delay_us(30U);
  data_input_pullup();

  if (!wait_data_level(0, 120U)) {
    diag = "no response on PC15";
    result = DHT11_ERR_NO_RESPONSE;
    goto done;
  }

  if (!wait_data_level(1, 120U)) {
    diag = "response low timeout";
    result = DHT11_ERR_LOW_TIMEOUT;
    goto done;
  }

  if (!wait_data_level(0, 120U)) {
    diag = "response high timeout";
    result = DHT11_ERR_HIGH_TIMEOUT;
    goto done;
  }

  for (uint32_t byte = 0U; byte < 5U; byte++) {
    for (uint32_t bit = 0U; bit < 8U; bit++) {
      uint32_t high_start;
      uint32_t high_us;

      if (!wait_data_level(1, 80U)) {
        diag = "bit low timeout";
        result = DHT11_ERR_BIT_START;
        goto done;
      }

      high_start = cycles_now();

      if (!wait_data_level(0, 120U)) {
        diag = "bit high timeout";
        result = DHT11_ERR_BIT_END;
        goto done;
      }

      high_us = elapsed_us(high_start);
      data[byte] <<= 1;
      if (high_us > 50U) {
        data[byte] |= 1U;
      }
    }
  }

  if (data[4] != (uint8_t)(data[0] + data[1] + data[2] + data[3])) {
    result = DHT11_ERR_CHECKSUM;
  } else {
    *humidity_int = data[0];
    *temperature_int = data[2];
  }

done:
  data_input_pullup();
  irq_restore(primask);

  if (diag != 0) {
    print_diag(diag);
  }

  if (result == DHT11_ERR_CHECKSUM) {
    elog_e("DHT11 checksum error: %02X %02X %02X %02X %02X",
           (unsigned int)data[0],
           (unsigned int)data[1],
           (unsigned int)data[2],
           (unsigned int)data[3],
           (unsigned int)data[4]);
  }

  return result;
}
