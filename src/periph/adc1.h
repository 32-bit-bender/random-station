/**
 * @file    adc1.h
 * @brief   ADC1 driver
 * @author  32-bit-bender
 * @date    2026-10-10
 */

#ifndef ADC1_H
#define ADC1_H

#ifdef __cplusplus
extern "C" {
#endif

void adc1_init(void);
void adc1_start_and_get_conv(uint32_t* const values);

#ifdef __cplusplus
}
#endif

#endif /* ADC1_H */
