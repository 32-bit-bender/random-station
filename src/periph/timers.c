/**
 * @file    timers.c
 * @brief   Timers configs, inits and interrupts
 * @author  32-bit-bender
 * @date    2026-10-08
 */

#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_tim.h>
#include "timers.h"

LL_TIM_InitTypeDef tim6_init_struct = {0};
static volatile uint32_t ms_ticks;

void tim6_init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6);

    NVIC_SetPriority(TIM6_DAC_IRQn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    tim6_init_struct.Prescaler = 79;
    tim6_init_struct.CounterMode = LL_TIM_COUNTERMODE_UP;
    tim6_init_struct.Autoreload = 999;
    LL_TIM_Init(TIM6, &tim6_init_struct);
    LL_TIM_DisableARRPreload(TIM6);
    LL_TIM_SetTriggerOutput(TIM6, LL_TIM_TRGO_RESET);
    LL_TIM_DisableMasterSlaveMode(TIM6);

    LL_TIM_EnableIT_UPDATE(TIM6);
    LL_TIM_ClearFlag_UPDATE(TIM6);

    LL_TIM_EnableCounter(TIM6);
}

uint32_t timebase_ms(void)
{
    return ms_ticks; /* aligned 32-bit read is atomic on Cortex-M */
}

void tim6_delay_ms(const uint32_t delay_ms)
{
    uint32_t start_tick = timebase_ms();
    while ((timebase_ms() - start_tick) < delay_ms)
    {
        // Wait until 'delay_ms' millisecond interrupts have occurred
    }
}

/* TIM6 and the DAC share one interrupt vector on this chip. */
void TIM6_DAC_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM6) != 0U) {
        LL_TIM_ClearFlag_UPDATE(TIM6);
        ms_ticks++;
    }
}
