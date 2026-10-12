/**
 * @file    timers.h
 * @brief   Timers configs, inits and interrupts
 * @author  32-bit-bender
 * @date    2026-10-08
 */

#ifndef TIMERS_H
#define TIMERS_H

#ifdef __cplusplus
extern "C" {
#endif

void tim6_init(void);
uint32_t timebase_ms(void);
void tim6_delay_ms(const uint32_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* TIMERS_H */
