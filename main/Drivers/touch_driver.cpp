/**
 * @file      touch_driver.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2024  Shenzhen Xinyuan Electronic Technology Co., Ltd
 * @date      2024-01-07
 *
 */

#define CONFIG_LILYGO_T4_S3_241 1

#include <sdkconfig.h>
#include "esp_err.h"
#include "esp_log.h"
#include "product_pins.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "touch";

#include "REG/CSTxxxConstants.h"
#include "touch/TouchClassCST226.h"
TouchClassCST226 touch;
#define TOUCH_ADDRESS   CST226SE_SLAVE_ADDRESS

static bool _init_success = false;

#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000      /*!< I2C master clock frequency */
#define I2C_MASTER_SDA_IO           (gpio_num_t) BOARD_I2C_SDA
#define I2C_MASTER_SCL_IO           (gpio_num_t) BOARD_I2C_SCL

i2c_master_bus_handle_t bus_handle;

/**
 * @brief i2c master initialization
 */
esp_err_t i2c_driver_init(void)
{
    i2c_master_bus_config_t i2c_bus_config;
    memset(&i2c_bus_config, 0, sizeof(i2c_bus_config));
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = I2C_MASTER_NUM;
    i2c_bus_config.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_bus_config.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_bus_config.glitch_ignore_cnt = 7;
    return i2c_new_master_bus(&i2c_bus_config, &bus_handle);
}

void touch_home_button_callback(void *args)
{
    ESP_LOGI(TAG, "Pressed Home button");
}

bool touch_init()
{
    ESP_LOGI(TAG, "Initialize Touchpanle");

    touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_IRQ);

    if (!touch.begin(bus_handle, TOUCH_ADDRESS)) {
        ESP_LOGE(TAG, "Touch init failed!");
        return false;
    }

    touch.setMaxCoordinates(AMOLED_HEIGHT, AMOLED_WIDTH);

    _init_success = true;
    return true;
}

uint8_t touch_get_data(int16_t *x, int16_t *y, uint8_t point_num)
{
    uint8_t touched = 0;

    if (!_init_success)return 0;
    touched =  touch.getPoint(x, y, point_num);
    if (touched) {
        ESP_LOGI(TAG, "X:%d Y:%d touched:%d\n", *x, *y, touched);
    }

    return touched;
}
