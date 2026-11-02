#pragma once

#include "driver/gpio.h"

/* I2S0 pins shared by the ES7210 capture and the ES8311 playback. */
#define BOARD_I2S_MCLK_GPIO GPIO_NUM_19
#define BOARD_I2S_BCLK_GPIO GPIO_NUM_20
#define BOARD_I2S_WS_GPIO GPIO_NUM_22
#define BOARD_I2S_DIN_GPIO GPIO_NUM_21
#define BOARD_I2S_DOUT_GPIO GPIO_NUM_23
