#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// AI-VOX3 Board (ESP32-S3R8 + ES8311 + NS4150B + ST7789 240x240)
// Schematic: AI_VOX_V3.kicad_sch Rev V0.5, sheet 4/4 (interface)

#include <driver/gpio.h>

// Audio: ES8311 codec + NS4150B speaker amp
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_11   // IO11 → ES8311 MCLK (pin 2)
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_10   // IO10 → ES8311 SCLK (pin 6)
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_8    // IO8  → ES8311 LRCK (pin 9)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7    // IO7  → ES8311 DSDIN (pin 10), MCU → codec
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_9    // IO9  → ES8311 ASDOUT (pin 8), codec → MCU

#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_13  // IO13 → ES8311 CDATA (pin 19)
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_12  // IO12 → ES8311 CCLK (pin 1)
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC  // NS4150B CTRL tied high via R30 (always on)

// Buttons
#define BOOT_BUTTON_GPIO        GPIO_NUM_0    // IO0, boot button
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_46   // IO46, button A
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_45   // IO45, button B

// WS2812 RGB LED
#define BUILTIN_LED_GPIO        GPIO_NUM_41   // IO41

// Display: ST7789 240x240 SPI LCD
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_SPI_MOSI_PIN  GPIO_NUM_21     // IO21 → LCD SDA (pin 4)
#define DISPLAY_SPI_DC_PIN    GPIO_NUM_14     // IO14 → LCD DC  (pin 5)
#define DISPLAY_SPI_SCLK_PIN  GPIO_NUM_17     // IO17 → LCD SCL (pin 6)
#define DISPLAY_SPI_CS_PIN    GPIO_NUM_15     // IO15 → LCD CS  (pin 7)
#define DISPLAY_SPI_RESET_PIN GPIO_NUM_NC     // EN → LCD RESET (pin 8)
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_16     // IO16 → LCD backlight
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// Battery monitoring
#define BATTERY_ADC_PIN         GPIO_NUM_18   // IO18, battery voltage divider
#define CHARGE_DETECT_PIN       GPIO_NUM_47   // IO47, charge status from FM5327

#endif // _BOARD_CONFIG_H_
