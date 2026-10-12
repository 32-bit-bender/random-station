/**
 * @file    dma.h
 * @brief   DMA init, config, trasmit and interrupt handler
 * @author  32-bit-bender
 * @date    2026-10-10
 */

#ifndef DMA_H
#define DMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void dma_init(void);
void dma1_s6_usart2_start_tx(const uint8_t* const buff, const size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DMA_H */
