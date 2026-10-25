/**
 * @file    ili9341.c
 * @brief   TFT display driver, based on ILI9341
 * @author  32-bit-bender
 * @date    2026-10-22
 * SPI MISO -> PC2
 * SPI MOSI -> PC3
 * SPI SCK  -> PC7
 * SPI CS   -> PC9
 * D\C      -> PC14
 * RESET    -> PC15
 * 3V3      -> 3V3
 * LED      -> 3V3
 */

#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>

#include "driver/ili9341.h"
#include "periph/spi2.h"
#include "rtos/rtos.h"

#define ILI9341_D_C_PC10        LL_GPIO_PIN_10
#define ILI9341_RESET_PC12      LL_GPIO_PIN_12
#define ILI9341_CS_PC9          LL_GPIO_PIN_9
#define ILI9341_LCD_WIDTH       320
#define ILI9341_LCD_HEIGHT      240

static  LL_GPIO_InitTypeDef gpio_init_out_dc_reset =
{
    .Pin = (ILI9341_D_C_PC10 | ILI9341_RESET_PC12),
    .Mode = LL_GPIO_MODE_OUTPUT,
    .Speed = LL_GPIO_SPEED_FREQ_LOW,
    .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
    .Pull = LL_GPIO_PULL_NO,
    .Alternate = LL_GPIO_AF_0,
};

static LL_GPIO_InitTypeDef gpio_init_cs =
{
    .Pin = ILI9341_CS_PC9,
    .Mode = LL_GPIO_MODE_OUTPUT,
    .Speed = LL_GPIO_SPEED_FREQ_LOW,
    .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
    .Pull = LL_GPIO_PULL_UP,
    .Alternate = LL_GPIO_AF_0,
};

/* Static Function Declarations --------------------------------------------- */

static void ili9341_gpio_init(void);
static void ili9341_cs_low(void);
static void ili9341_cs_high(void);
static void ili9341_cmd_on_data_off(void);
static void ili9341_cmd_off_data_on(void);
static void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1,
                               uint16_t y1);

/* Function Definitions ----------------------------------------------------- */

void ili9341_reset(void)
{
    LL_GPIO_ResetOutputPin(GPIOC, ILI9341_RESET_PC12);
    vTaskDelay(pdMS_TO_TICKS(10));
    LL_GPIO_SetOutputPin(GPIOC, ILI9341_RESET_PC12);
    vTaskDelay(pdMS_TO_TICKS(120));
}

bool ili9341_write_cmd(uint8_t cmd, const uint8_t *data, size_t len)
{
    ili9341_cs_low();
    ili9341_cmd_on_data_off();
    bool ok = spi2_write(&cmd, 1);
    if (ok && len > 0U) {
        ili9341_cmd_off_data_on();
        ok = spi2_write(data, len);
    }
    ili9341_cs_high();
    return ok;
}

void ili9341_init(void)
{
    ili9341_gpio_init();
    ili9341_reset();

    ili9341_write_cmd(0x01, NULL, 0);            /* software reset */
    vTaskDelay(pdMS_TO_TICKS(120));
    ili9341_write_cmd(0x11, NULL, 0);            /* sleep out */
    vTaskDelay(pdMS_TO_TICKS(120));
    ili9341_write_cmd(0x3A, (uint8_t[]){0x55}, 1); /* RGB565 */
    ili9341_write_cmd(0x36, (uint8_t[]){0x48}, 1); /* portrait; flip BGR bit if colors swap */
    ili9341_write_cmd(0x29, NULL, 0);            /* display on */
}

void ili9341_fill(uint16_t color)
{
    static uint8_t line[240 * 2];
    for (int i = 0; i < 240; i++) {
        line[2 * i]     = color >> 8;
        line[2 * i + 1] = color & 0xFF;
    }

    ili9341_write_cmd(0x2A, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4); /* cols 0..239 */
    ili9341_write_cmd(0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0x3F}, 4); /* rows 0..319 */

    uint8_t ramwr = 0x2C;
    ili9341_cs_low();                  /* CS stays low for command + all pixels */
    ili9341_cmd_on_data_off();
    spi2_write(&ramwr, 1);
    ili9341_cmd_off_data_on();
    for (int y = 0; y < 320; y++) {
        spi2_write(line, sizeof line);
    }
    ili9341_cs_high();
}

void ili9341_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    static uint8_t line[ILI9341_LCD_WIDTH * 2];

    if (w <= 0 || h <= 0) return;
    if (x >= ILI9341_LCD_WIDTH || y >= ILI9341_LCD_HEIGHT) return;
    if (x + w <= 0 || y + h <= 0) return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ILI9341_LCD_WIDTH)  w = ILI9341_LCD_WIDTH  - x;
    if (y + h > ILI9341_LCD_HEIGHT) h = ILI9341_LCD_HEIGHT - y;

    for (int i = 0; i < w; i++) {
        line[2 * i]     = color >> 8;
        line[2 * i + 1] = color & 0xFF;
    }

    ili9341_set_window(x, y, x + w - 1, y + h - 1);

    uint8_t ramwr = 0x2C;
    ili9341_cs_low();                  /* CS stays low for command + all rows */
    ili9341_cmd_on_data_off();
    bool ok = spi2_write(&ramwr, 1);
    ili9341_cmd_off_data_on();
    for (int j = 0; ok && j < h; j++) {
        ok = spi2_write(line, (size_t)w * 2);
    }
    ili9341_cs_high();
}

void ili9341_fill_square(int16_t x, int16_t y, int16_t size, uint16_t color)
{
    ili9341_fill_rect(x, y, size, size, color);
}

/* Static Function Definitions ---------------------------------------------- */

static void ili9341_gpio_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);

    LL_GPIO_SetOutputPin(GPIOC, ILI9341_RESET_PC12);
    LL_GPIO_Init(GPIOC, &gpio_init_out_dc_reset);

    LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_9);
    LL_GPIO_Init(GPIOC, &gpio_init_cs);
}

static void ili9341_cs_low(void)
{
    LL_GPIO_ResetOutputPin(GPIOC, ILI9341_CS_PC9);
}

static void ili9341_cs_high(void)
{
    LL_GPIO_SetOutputPin(GPIOC, ILI9341_CS_PC9);
}

static void ili9341_cmd_on_data_off(void)
{
    LL_GPIO_ResetOutputPin(GPIOC, ILI9341_D_C_PC10);
}

static void ili9341_cmd_off_data_on(void)
{
    LL_GPIO_SetOutputPin(GPIOC, ILI9341_D_C_PC10);
}

static void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1,
                               uint16_t y1)
{
    const uint8_t col[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t row[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };

    ili9341_write_cmd(0x2A, col, 4);   /* columns = x */
    ili9341_write_cmd(0x2B, row, 4);   /* rows    = y */
}
