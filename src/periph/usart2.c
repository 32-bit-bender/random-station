/**
 * @file    uart2.c
 * @brief   Usart2 driver
 * @author  32-bit-bender
 * @date    2026-10-08
 */

#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_usart.h>

#include "periph/dma.h"
#include "periph/usart2.h"

#define DMA_BUFF_SIZE       100

LL_USART_InitTypeDef uart2_init_struct = {0};
LL_GPIO_InitTypeDef uart2_gpio_init_struct = {0};

void uart2_init(void)
{
  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  /**USART2 GPIO Configuration
  PA2   ------> USART2_TX
  PA3   ------> USART2_RX
  */
  uart2_gpio_init_struct.Pin = LL_GPIO_PIN_2|LL_GPIO_PIN_3;
  uart2_gpio_init_struct.Mode = LL_GPIO_MODE_ALTERNATE;
  uart2_gpio_init_struct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  uart2_gpio_init_struct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  uart2_gpio_init_struct.Pull = LL_GPIO_PULL_NO;
  uart2_gpio_init_struct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOA, &uart2_gpio_init_struct);


  uart2_init_struct.BaudRate = 921600;
  uart2_init_struct.DataWidth = LL_USART_DATAWIDTH_8B;
  uart2_init_struct.StopBits = LL_USART_STOPBITS_1;
  uart2_init_struct.Parity = LL_USART_PARITY_NONE;
  uart2_init_struct.TransferDirection = LL_USART_DIRECTION_TX;
  uart2_init_struct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  uart2_init_struct.OverSampling = LL_USART_OVERSAMPLING_16;

  LL_USART_Init(USART2, &uart2_init_struct);
  LL_USART_ConfigAsyncMode(USART2);
  LL_USART_EnableDMAReq_TX(USART2);
  LL_USART_Enable(USART2);
}

void usart2_start_dma_tx(const uint8_t* const buff, const size_t size)
{
  dma1_s6_usart2_start_tx(buff, size);
}
