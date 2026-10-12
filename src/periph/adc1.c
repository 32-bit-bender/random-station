/**
 * @file    adc1.c
 * @brief   ADC1 driver
 * @author  32-bit-bender
 * @date    2026-10-10
 */

#include <stm32f4xx_ll_adc.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_dma.h>
#include <stm32f4xx_ll_gpio.h>

#include "adc1.h"

#define ADC1_SAMPLING_CYCLES        LL_ADC_SAMPLINGTIME_112CYCLES

static LL_ADC_InitTypeDef adc_init_struct = {0};
static LL_ADC_REG_InitTypeDef adc_reg_init_struct = {0};
static LL_ADC_CommonInitTypeDef adc_common_init_struct = {0};
static LL_GPIO_InitTypeDef adc1_ch8_pb0_gpio = {0};

static uint32_t adc_values[3];

static void adc1_gpio_init(void);
static void adc1_set_channel(const uint32_t channel, const uint32_t rank);

void adc1_init(void)
{
    adc1_gpio_init();
    /* Peripheral clock enable */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

    adc_init_struct.Resolution = LL_ADC_RESOLUTION_12B;
    adc_init_struct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
    adc_init_struct.SequencersScanMode = LL_ADC_SEQ_SCAN_ENABLE;
    LL_ADC_Init(ADC1, &adc_init_struct);
    adc_reg_init_struct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
    adc_reg_init_struct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_3RANKS;
    adc_reg_init_struct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
    adc_reg_init_struct.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
    adc_reg_init_struct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_UNLIMITED;
    LL_ADC_REG_Init(ADC1, &adc_reg_init_struct);
    LL_ADC_REG_SetFlagEndOfConversion(ADC1, LL_ADC_REG_FLAG_EOC_UNITARY_CONV);
    adc_common_init_struct.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
    adc_common_init_struct.Multimode = LL_ADC_MULTI_INDEPENDENT;
    LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &adc_common_init_struct);
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
                                   LL_ADC_PATH_INTERNAL_VREFINT);


    adc1_set_channel(LL_ADC_CHANNEL_8, LL_ADC_REG_RANK_1);
    adc1_set_channel(LL_ADC_CHANNEL_TEMPSENSOR, LL_ADC_REG_RANK_2);
    adc1_set_channel(LL_ADC_PATH_INTERNAL_VREFINT, LL_ADC_REG_RANK_3);

    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
                                   LL_ADC_PATH_INTERNAL_TEMPSENSOR);
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
                                   LL_ADC_PATH_INTERNAL_VREFINT);

    LL_DMA_ConfigAddresses(DMA2, LL_DMA_STREAM_0,
                       LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
                       (uint32_t)adc_values,
                       LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_0, 3);
    LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_0);

    // Enable ADC and trigger conversion
    LL_ADC_Enable(ADC1);
    LL_ADC_REG_StartConversionSWStart(ADC1);
}


void adc1_start_and_get_conv(uint32_t* const values)
{
    LL_ADC_REG_StartConversionSWStart(ADC1);

    // 2. Loop through each of the 3 ranks
    for (int i = 0; i < 3; i++) {
        // Wait for the individual conversion to finish
        while (!LL_ADC_IsActiveFlag_EOCS(ADC1)) {
            // Wait for current rank conversion
        }

        // Read the data register (reading ADC_DR automatically clears the EOC flag)
        values[i] = LL_ADC_REG_ReadConversionData32(ADC1);
    }
}

/**ADC1 GPIO Configuration
 PB0   ------> ADC1_IN8
*/
static void adc1_gpio_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
    adc1_ch8_pb0_gpio.Pin = LL_GPIO_PIN_0;
    adc1_ch8_pb0_gpio.Mode = LL_GPIO_MODE_ANALOG;
    adc1_ch8_pb0_gpio.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOB, &adc1_ch8_pb0_gpio);
}

static void adc1_set_channel(const uint32_t channel, const uint32_t rank)
{
    LL_ADC_REG_SetSequencerRanks(ADC1, rank, channel);
    LL_ADC_SetChannelSamplingTime(ADC1, channel,
                                  ADC1_SAMPLING_CYCLES);
}
