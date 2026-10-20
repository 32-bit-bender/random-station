/**
 * @file    exti.c
 * @brief   TODO: one-line description
 * @author  32-bit-bender
 * @date    2026-10-12
 */

#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_exti.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_system.h>
#include <stm32f4xx_ll_tim.h>

#include "exti.h"

static LL_EXTI_InitTypeDef pa7_exti_init_s = {0};
static volatile uint32_t pulse_resp_delay = 0;
static volatile uint32_t echo_start;

void exti_init(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_7, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_INPUT);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE7);

    pa7_exti_init_s.Line_0_31 = LL_EXTI_LINE_7;
    pa7_exti_init_s.LineCommand = ENABLE;
    pa7_exti_init_s.Mode = LL_EXTI_MODE_IT;
    pa7_exti_init_s.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;;
    LL_EXTI_Init(&pa7_exti_init_s);

    /* EXTI interrupt init*/
    NVIC_SetPriority(EXTI9_5_IRQn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_7);
}

uint32_t exti_get_last_pulse_response_delay(void)
{
    return pulse_resp_delay;
}

void EXTI9_5_IRQHandler(void)
{
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_7) != RESET)
  {
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_7))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_7);
        uint32_t now = LL_TIM_GetCounter(TIM3);

        if (LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_7))
        {
            echo_start = now;                      /* rising edge */
        }
        else
        {
            /* falling edge: width in us, handling a counter wrap (period = 50000) */
            pulse_resp_delay = (now >= echo_start)
                             ? (now - echo_start)
                             : (now + 50000u - echo_start);
        }
    }
  }
}
