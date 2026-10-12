/**
 * @file    system.c
 * @brief   TODO: one-line description
 * @author  32-bit-bender
 * @date    2026-10-08
 */

#include <stm32f4xx.h>
#include "system/system.h"

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void error_handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
