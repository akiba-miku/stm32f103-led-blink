#include "uart.h"

#define PERIPH_BASE 0x40000000UL
#define APB2PERIPH_BASE (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE (PERIPH_BASE + 0x00020000UL)

#define GPIOA_BASE (APB2PERIPH_BASE + 0x00000800UL)
#define RCC_BASE (AHBPERIPH_BASE + 0x00001000UL)
#define USART1_BASE (APB2PERIPH_BASE + 0x00003800UL)
#define DMA1_BASE (AHBPERIPH_BASE + 0x00000000UL)

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_APB2ENR REG32(RCC_BASE + 0x18UL)
#define RCC_AHBENR REG32(RCC_BASE + 0x14UL)

#define GPIOA_CRH REG32(GPIOA_BASE + 0x04UL)

#define USART1_SR REG32(USART1_BASE + 0x00UL)
#define USART1_DR REG32(USART1_BASE + 0x04UL)
#define USART1_BRR REG32(USART1_BASE + 0x08UL)
#define USART1_CR1 REG32(USART1_BASE + 0x0CUL)
#define USART1_CR3 REG32(USART1_BASE + 0x14UL)

#define DMA1_ISR REG32(DMA1_BASE + 0x00UL)
#define DMA1_IFCR REG32(DMA1_BASE + 0x04UL)

#define DMA1_CH5_CCR REG32(DMA1_BASE + 0x58UL)
#define DMA1_CH5_CNDTR REG32(DMA1_BASE + 0x5CUL)
#define DMA1_CH5_CPAR REG32(DMA1_BASE + 0x60UL)
#define DMA1_CH5_CMAR REG32(DMA1_BASE + 0x64UL)

#define NVIC_ISER0 REG32(0xE000E100UL)
#define NVIC_ISER1 REG32(0xE000E104UL)

#define RCC_APB2ENR_IOPAEN (1UL << 2)
#define RCC_APB2ENR_USART1EN (1UL << 14)
#define RCC_AHBENR_DMA1EN (1UL << 0)

#define USART_SR_IDLE (1UL << 4)
#define USART_SR_TXE (1UL << 7)

#define USART_CR1_IDLEIE (1UL << 4)
#define USART_CR1_RE (1UL << 2)
#define USART_CR1_TE (1UL << 3)
#define USART_CR1_UE (1UL << 13)

#define USART_CR3_DMAR (1UL << 6)

#define DMA_CCR_EN (1UL << 0)
#define DMA_CCR_TCIE (1UL << 1)
#define DMA_CCR_MINC (1UL << 7)
#define DMA_CCR_PL_HIGH (1UL << 12)

#define USART1_IRQN (37UL)
#define DMA1_CHANNEL5_IRQN (15UL)

#define UART1_RX_FRAME_LEN 10U

#define DMA1_IFCR_CGIF5 (1UL << 16)
#define DMA1_IFCR_CTCIF5 (1UL << 17)
#define DMA1_IFCR_CHTIF5 (1UL << 18)
#define DMA1_IFCR_CTEIF5 (1UL << 19)

static volatile uint8_t rx_dma_buffer[UART1_RX_FRAME_LEN];
static volatile uint8_t rx_frame[UART1_RX_FRAME_LEN];
static volatile uint32_t rx_frame_len;
static volatile uint32_t rx_frame_ready;

static uint32_t uart1_brr_from_baud(uint32_t baud)
{
  if (baud == 9600U) {
    return 0x0341UL;
  }

  return 0x0045UL; /* 115200 baud at 8 MHz PCLK2. */
}

static void uart1_enter_critical(void)
{
  __asm volatile("cpsid i" ::: "memory");
}

static void uart1_exit_critical(void)
{
  __asm volatile("cpsie i" ::: "memory");
}

static void uart1_enable_nvic(void)
{
  NVIC_ISER0 = (1UL << DMA1_CHANNEL5_IRQN);
  NVIC_ISER1 = (1UL << (USART1_IRQN - 32UL));
}

static void uart1_clear_dma_flags(void)
{
  DMA1_IFCR = DMA1_IFCR_CGIF5 | DMA1_IFCR_CTCIF5 | DMA1_IFCR_CHTIF5 | DMA1_IFCR_CTEIF5;
}

static void uart1_rearm_dma_rx(void)
{
  DMA1_CH5_CCR &= ~DMA_CCR_EN;
  uart1_clear_dma_flags();
  DMA1_CH5_CMAR = (uint32_t)(uintptr_t)rx_dma_buffer;
  DMA1_CH5_CNDTR = UART1_RX_FRAME_LEN;
  DMA1_CH5_CCR |= DMA_CCR_EN;
}

static void uart1_finish_rx_frame(uint32_t received_len)
{
  uint32_t i;

  if (received_len > UART1_RX_FRAME_LEN) {
    received_len = UART1_RX_FRAME_LEN;
  }

  for (i = 0U; i < received_len; i++) {
    rx_frame[i] = rx_dma_buffer[i];
  }

  rx_frame_len = received_len;
  rx_frame_ready = 1U;
  DMA1_CH5_CCR &= ~DMA_CCR_EN;
  uart1_clear_dma_flags();
}

void uart1_init(void)
{
  RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
  RCC_AHBENR |= RCC_AHBENR_DMA1EN;

  /*
   * PA9  = USART1_TX: alternate-function push-pull, 50 MHz.
   * PA10 = USART1_RX: floating input.
   */
  GPIOA_CRH &= ~((0xFUL << 4) | (0xFUL << 8));
  GPIOA_CRH |= (0xBUL << 4) | (0x4UL << 8);

  USART1_BRR = uart1_brr_from_baud(115200U);
  USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;

  DMA1_CH5_CCR &= ~DMA_CCR_EN;
  DMA1_CH5_CPAR = (uint32_t)(uintptr_t)&USART1_DR;
  DMA1_CH5_CMAR = (uint32_t)(uintptr_t)rx_dma_buffer;
  DMA1_CH5_CNDTR = UART1_RX_FRAME_LEN;
  DMA1_CH5_CCR = DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_PL_HIGH;

  USART1_CR3 |= USART_CR3_DMAR;
  uart1_enable_nvic();
  uart1_rearm_dma_rx();
}

void uart1_set_baud(uint32_t baud)
{
  USART1_BRR = uart1_brr_from_baud(baud);
}

void uart1_write_char(char ch)
{
  uart1_write_byte((uint8_t)ch);
}

void uart1_write_byte(uint8_t byte)
{
  while ((USART1_SR & USART_SR_TXE) == 0U) {
  }

  USART1_DR = (uint32_t)byte;
}

void uart1_write_bytes(const uint8_t *data, uint32_t len)
{
  while (len > 0U) {
    uart1_write_byte(*data);
    data++;
    len--;
  }
}

void uart1_write_string(const char *text)
{
  while (*text != '\0') {
    uart1_write_char(*text);
    text++;
  }
}

int uart1_take_rx_frame(char *out, uint32_t len, uint32_t *out_len)
{
  uint32_t i;
  uint32_t frame_len;

  if (len < UART1_RX_FRAME_LEN) {
    return 0;
  }

  uart1_enter_critical();

  if (rx_frame_ready == 0U) {
    uart1_exit_critical();
    return 0;
  }

  frame_len = rx_frame_len;
  for (i = 0U; i < frame_len; i++) {
    out[i] = (char)rx_frame[i];
  }

  rx_frame_ready = 0U;
  rx_frame_len = 0U;
  uart1_rearm_dma_rx();
  uart1_exit_critical();

  if (out_len != 0) {
    *out_len = frame_len;
  }
  return 1;
}

int _write(int file, char *ptr, int len)
{
  int i;

  (void)file;

  for (i = 0; i < len; i++) {
    uart1_write_char(ptr[i]);
  }

  return len;
}

void DMA1_Channel5_IRQHandler(void)
{
  if ((DMA1_ISR & (1UL << 17)) == 0U) {
    return;
  }

  uart1_finish_rx_frame(UART1_RX_FRAME_LEN);
}

void USART1_IRQHandler(void)
{
  uint32_t sr;
  uint32_t received_len;

  sr = USART1_SR;
  if ((sr & USART_SR_IDLE) == 0U) {
    return;
  }

  /*
   * Clear IDLE by reading SR then DR.
   * The DR read also drains the last DMA-visible byte from the peripheral path.
   */
  (void)USART1_SR;
  (void)USART1_DR;

  if (rx_frame_ready != 0U) {
    return;
  }

  received_len = UART1_RX_FRAME_LEN - DMA1_CH5_CNDTR;
  if (received_len == 0U) {
    return;
  }

  uart1_finish_rx_frame(received_len);
}
