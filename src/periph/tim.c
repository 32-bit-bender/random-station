/**
 * @file    tim.c
 * @brief   TIMER drivers
 * @author  32-bit-bender
 * @date    2026-10-12
 */
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_tim.h>
#include "tim.h"

LL_TIM_InitTypeDef tim2_init_struct = {0};
LL_TIM_InitTypeDef tim3_init_struct = {0};
LL_TIM_OC_InitTypeDef tim3_oc_init_struct = {0};

LL_GPIO_InitTypeDef tim3_pwm_gpio_struct = {0};

static void tim3_pwm_gpio_init(void);

void tim2_init(void)
{
    /* Peripheral clock enable */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

    tim2_init_struct.Prescaler = (SystemCoreClock / 1000000) - 1;
    tim2_init_struct.CounterMode = LL_TIM_COUNTERMODE_UP;
    tim2_init_struct.Autoreload = 0xFFFFFFFF;
    tim2_init_struct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    LL_TIM_Init(TIM2, &tim2_init_struct);
    LL_TIM_DisableARRPreload(TIM2);
    LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);
    LL_TIM_DisableMasterSlaveMode(TIM2);

    LL_TIM_EnableCounter(TIM2);
}

void tim3_init (void)
{
    tim3_pwm_gpio_init();
    /* Peripheral clock enable */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

    tim3_init_struct.Prescaler = (SystemCoreClock / 1000000) - 1;
    tim3_init_struct.CounterMode = LL_TIM_COUNTERMODE_UP;
    tim3_init_struct.Autoreload = 59999;
    tim3_init_struct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    LL_TIM_Init(TIM3, &tim3_init_struct);
    LL_TIM_DisableARRPreload(TIM3);
    LL_TIM_SetClockSource(TIM3, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH1);
    tim3_oc_init_struct.OCMode = LL_TIM_OCMODE_PWM1;
    tim3_oc_init_struct.OCState = LL_TIM_OCSTATE_DISABLE;
    tim3_oc_init_struct.OCNState = LL_TIM_OCSTATE_DISABLE;
    tim3_oc_init_struct.CompareValue = 10;
    tim3_oc_init_struct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
    LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH1, &tim3_oc_init_struct);
    LL_TIM_OC_DisableFast(TIM3, LL_TIM_CHANNEL_CH1);
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_RESET);
    LL_TIM_DisableMasterSlaveMode(TIM3);

    LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH1);
    LL_TIM_EnableCounter(TIM3);

}

uint32_t timebase_ms(void)
{
    return (LL_TIM_GetCounter(TIM2) / 1000);
}

void tim2_delay_ms(const uint32_t delay_ms)
{
    uint32_t start_tick = timebase_ms();
    while ((timebase_ms() - start_tick) < delay_ms)
    {
        // Wait until 'delay_ms' millisecond interrupts have occurred
    }
}

uint32_t timebase_us(void)
{
    return LL_TIM_GetCounter(TIM2);
}

/**TIM3 GPIO Configuration
 PA6   ------> TIM3_CH1
*/
static void tim3_pwm_gpio_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

    tim3_pwm_gpio_struct.Pin = LL_GPIO_PIN_6;
    tim3_pwm_gpio_struct.Mode = LL_GPIO_MODE_ALTERNATE;
    tim3_pwm_gpio_struct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    tim3_pwm_gpio_struct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    tim3_pwm_gpio_struct.Pull = LL_GPIO_PULL_NO;
    tim3_pwm_gpio_struct.Alternate = LL_GPIO_AF_2;
    LL_GPIO_Init(GPIOA, &tim3_pwm_gpio_struct);
}
