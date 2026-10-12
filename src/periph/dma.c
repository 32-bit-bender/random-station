/**
 * @file    dma.h
 * @brief   DMA init, config, trasmit and interrupt handler
 * @author  32-bit-bender
 * @date    2026-10-10
 */

/**
 * DMA Allocation:
 *  DMA Stream 6 -> USART2 TX
 *  ..
 */

#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_dma.h>

#include "dma.h"

void dma_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

    /* DMA interrupt init */
    /* DMA1_Stream6_IRQn interrupt configuration */
    NVIC_SetPriority(DMA1_Stream6_IRQn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_STREAM_6);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_STREAM_6);

    /* USART2_TX Init */
    LL_DMA_SetChannelSelection(DMA1, LL_DMA_STREAM_6, LL_DMA_CHANNEL_4);
    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_6,
                                    LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_6, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(DMA1, LL_DMA_STREAM_6, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_6, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_6, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_6, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_6, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_DisableFifoMode(DMA1, LL_DMA_STREAM_6);
}

void dma1_s6_usart2_start_tx(const uint8_t* const buff, const size_t size)
{
    LL_DMA_SetPeriphAddress(DMA1,LL_DMA_STREAM_6, (uint32_t)&USART2->DR);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_6, (uint32_t)buff);
    LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_6, size);

    LL_DMA_ClearFlag_TC6(DMA1);
    LL_DMA_ClearFlag_HT6(DMA1);
    LL_DMA_ClearFlag_TE6(DMA1);
    LL_DMA_ClearFlag_DME6(DMA1);
    LL_DMA_ClearFlag_FE6(DMA1);

    LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_6);
}

/* DMA1 STREAM 6 Transmission Complete Callback */
__attribute__((weak)) void dma1_s6_usart2_tc_callback(void);

/* DMA1 STREAM 6 Transmission Complete Callback */
__attribute__((weak)) void dma1_s6_usart2_te_callback(void);

void DMA1_Stream6_IRQHandler(void) {
    // Check if the Transfer Complete interrupt flag for Stream 6 is set
    if (LL_DMA_IsActiveFlag_TC6(DMA1)) {
        // 1. Clear the Transfer Complete flag
        LL_DMA_ClearFlag_TC6(DMA1);

        // 2. Disable the DMA stream to reset for the next transfer
        LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_6);

        dma1_s6_usart2_tc_callback();

    }

    // Optional: Handle DMA Transfer Error interrupts if enabled
    if (LL_DMA_IsActiveFlag_TE6(DMA1)) {
        LL_DMA_ClearFlag_TE6(DMA1);

        dma1_s6_usart2_te_callback();
    }
}
