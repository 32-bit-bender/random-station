/**
 * @file    uart2.h
 * @brief   Usart2 driver
 * @author  32-bit-bender
 * @date    2026-10-08
 */

#ifndef UART2_H
#define UART2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void uart2_init(void);
void usart2_start_dma_tx(const uint8_t* const buff,const size_t size);


/* DMA1 STREAM 6 Transmission Complete Callback */
void dma1_s6_usart2_tc_callback(void);
/* DMA1 STREAM 6 Transmission Complete Callback */
void dma1_s6_usart2_te_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* UART2_H */
