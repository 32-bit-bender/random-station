/**
 * @file    exti.h
 * @brief   External Interrupt Event Controller driver
 * @author  32-bit-bender
 * @date    2026-10-12
 */

#ifndef EXTI_H
#define EXTI_H

#ifdef __cplusplus
extern "C" {
#endif

void exti_init(void);
uint32_t exti_get_last_pulse_response_delay(void);

#ifdef __cplusplus
}
#endif

#endif /* EXTI_H */
