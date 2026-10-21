/**
 * @file    tim.h
 * @brief   TIMER drivers
 * @author  32-bit-bender
 * @date    2026-10-12
 */

#ifndef TIM2_H
#define TIM2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void tim2_init(void);
void tim3_init (void);
uint32_t timebase_ms(void);
uint32_t timebase_us(void);
void tim2_delay_us(const uint32_t delay_us);
void tim2_delay_ms(const uint32_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* TIM2_H */
