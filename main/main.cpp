#include <cstdio>
#include <nvs_flash.h>

#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl.h"
#include "Drivers/touch_driver.h"
#include "Drivers/amoled_driver.h"
#include "Drivers/power_driver.h"

#include"Display.hpp"
#include"Views/Wifi.hpp"
#include"Views/Dashboard.hpp"

static const char *TAG = "main";

extern "C" void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	i2c_driver_init();
	if (!power_driver_init())
	{
		ESP_LOGE(TAG, "ERROR :No find PMU ....");
	}

	touch_init();
	display_init();
	Display::Init();

	Wifi::Init();
	Dashboard::Init();
}
