#include"Display.hpp"

#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "Drivers/amoled_driver.h"
#include "Drivers/touch_driver.h"
#include "Drivers/product_pins.h"

static const char *TAG = "Display";

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE (8 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY 2

lv_obj_t* modesScreen[Display::Mode::Count];
Display::Mode currentMode = Display::Mode::Disconnected;

static SemaphoreHandle_t lvgl_mux = NULL;

// static lv_buffdisp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
lv_display_t *disp_drv; // contains callback functions

uint8_t *revbuf;

// typedef void (*lv_display_flush_cb_t)(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
static void example_lvgl_flush_cb(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
	uint32_t w = (area->x2 - area->x1 + 1);
	uint32_t h = (area->y2 - area->y1 + 1);

	// printf("Refr: %ld x %ld\n", w, h);

	auto rotation = lv_display_get_rotation(lv_display_get_default());
	if (rotation == LV_DISPLAY_ROTATION_0)
	{
		for (unsigned a = 0; a < w * h; a++)
		{
			revbuf[a * 2 + 0] = color_map[a * 2 + 1];
			revbuf[a * 2 + 1] = color_map[a * 2 + 0];
		}
	}
	else if (rotation == LV_DISPLAY_ROTATION_180)
	{
		unsigned offset1, offset2;
		for (unsigned y = 0; y < AMOLED_HEIGHT; y++)
		{
			for (unsigned x = 0; x < AMOLED_WIDTH; x++)
			{
				offset1 = y * AMOLED_WIDTH + x;
				offset2 = (AMOLED_HEIGHT - 1 - y) * AMOLED_WIDTH + (AMOLED_WIDTH - 1 - x);
				revbuf[offset1 * 2 + 0] = color_map[offset2 * 2 + 1];
				revbuf[offset1 * 2 + 1] = color_map[offset2 * 2 + 0];
			}
		}
	}
	else
	{
		unsigned offset1;
		unsigned offset2;
		for (unsigned y = 0; y < AMOLED_HEIGHT; y++)
		{
			for (unsigned x = 0; x < AMOLED_WIDTH; x++)
			{
				if (rotation == LV_DISPLAY_ROTATION_90)
					offset1 = (AMOLED_HEIGHT - 1 - y) * AMOLED_WIDTH + x;
				else
					offset1 = y * AMOLED_WIDTH + (AMOLED_WIDTH - 1 - x);

				offset2 = x * AMOLED_HEIGHT + y;
				revbuf[offset1 * 2 + 0] = color_map[offset2 * 2 + 1];
				revbuf[offset1 * 2 + 1] = color_map[offset2 * 2 + 0];
			}
		}
	}

	display_push_colors(0, 0, AMOLED_WIDTH, AMOLED_HEIGHT, (uint16_t *)revbuf);
	lv_disp_flush_ready(drv);
}


static void example_lvgl_touch_cb(lv_indev_t *drv, lv_indev_data_t *data)
{
	int16_t touchpad_x[1] = { 0 };
	int16_t touchpad_y[1] = { 0 };
	uint8_t touchpad_cnt = 0;

	/* Get coordinates */
	touchpad_cnt = touch_get_data(touchpad_x, touchpad_y, 1);

	if (touchpad_cnt > 0)
	{
		// if(lv_display_get_rotation(lv_display_get_default()) == LV_DISPLAY_ROTATION_0)
		// {
		printf("<touch> rot 0\n");
		data->point.x = touchpad_x[0];
		data->point.y = touchpad_y[0];
		// }
		// else
		// {
		//     printf("<touch> rot 90\n");
		//     data->point.x = AMOLED_WIDTH - 1 - touchpad_y[0];
		//     data->point.y = AMOLED_HEIGHT - 1 - touchpad_x[0];
		// }
		printf("<touch> [%ld x %ld]\n", data->point.x, data->point.y);
		data->state = LV_INDEV_STATE_PRESSED;
	}
	else
	{
		data->state = LV_INDEV_STATE_RELEASED;
	}
}

static void example_increase_lvgl_tick(void *arg)
{
	/* Tell LVGL how many milliseconds has elapsed */
	lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_port_task(void *arg)
{
	ESP_LOGI(TAG, "Starting LVGL task");
	uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
	while (1)
	{
		// ESP_LOGI(TAG, "LVGL Lock?...");
		// Lock the mutex due to the LVGL APIs are not thread-safe
		if (Display::Lock(-1))
		{
			// ESP_LOGI(TAG, "LVGL Locked~");
			task_delay_ms = lv_timer_handler();
			// Release the mutex
			// ESP_LOGI(TAG, "LVGL Unlocking...");
			Display::Unlock();
			// ESP_LOGI(TAG, "LVGL Unlocked~");
		}
		if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
		{
			task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
		}
		else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
		{
			task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
		}
		// ESP_LOGI(TAG, "LVGL sleep: %ld", task_delay_ms);
		vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
	}
}

bool Display::Init()
{
	lv_init();

	lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISPLAY_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
	assert(buf1);

	revbuf = (uint8_t *)heap_caps_malloc(DISPLAY_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
	assert(revbuf);

	disp_drv = lv_display_create(AMOLED_WIDTH, AMOLED_HEIGHT);
	lv_display_set_flush_cb(disp_drv, example_lvgl_flush_cb);
	lv_display_set_buffers(disp_drv, buf1, nullptr, DISPLAY_BUFFER_SIZE * sizeof(lv_color_t),
						   LV_DISPLAY_RENDER_MODE_DIRECT);
	lv_display_set_rotation(disp_drv, LV_DISPLAY_ROTATION_90);

	ESP_LOGI(TAG, "Install LVGL tick timer");
	// Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
	const esp_timer_create_args_t lvgl_tick_timer_args = {
		.callback = &example_increase_lvgl_tick,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "lvgl_tick",
		.skip_unhandled_events = false
	};
	esp_timer_handle_t lvgl_tick_timer = NULL;
	ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

	auto indev_drv = lv_indev_create();
	lv_indev_set_type(indev_drv, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(indev_drv, example_lvgl_touch_cb);
	lv_indev_set_display(indev_drv, disp_drv);

	lvgl_mux = xSemaphoreCreateRecursiveMutex();
	assert(lvgl_mux);

	// Modes screens
	modesScreen[0] = lv_display_get_screen_active(lv_display_get_default());
	for(unsigned a = 1; a < Display::Mode::Count; a++)
	{
		modesScreen[a] = lv_obj_create(nullptr);
		lv_obj_set_size(modesScreen[a], GetWidth(), GetHeight());
	}

	printf("Active: %p\n", lv_display_get_screen_active(lv_display_get_default()));
	for(unsigned a = 0; a < Display::Mode::Count; a++)
	{
		lv_obj_set_style_bg_color(modesScreen[a], lv_color_make(0, 0, 0), 0);
		printf("[%d] = %p\n", a, modesScreen[a]);
	}

	lv_timer_handler();
	xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

	return true;
}

bool Display::Lock(int timeout_ms)
{
	// Convert timeout in milliseconds to FreeRTOS ticks
	// If `timeout_ms` is set to -1, the program will block until the condition is met
	const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
	return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void Display::Unlock()
{
	xSemaphoreGiveRecursive(lvgl_mux);
}

int Display::GetWidth()
{
	switch(lv_display_get_rotation(lv_display_get_default()))
	{
		case LV_DISPLAY_ROTATION_0:
		case LV_DISPLAY_ROTATION_180:
			return PHYS_WIDTH;
		case LV_DISPLAY_ROTATION_90:
		case LV_DISPLAY_ROTATION_270:
			return PHYS_HEIGHT;
	}

	__unreachable();
}

int Display::GetHeight()
{
	switch(lv_display_get_rotation(lv_display_get_default()))
	{
		case LV_DISPLAY_ROTATION_0:
		case LV_DISPLAY_ROTATION_180:
			return PHYS_HEIGHT;
		case LV_DISPLAY_ROTATION_90:
		case LV_DISPLAY_ROTATION_270:
			return PHYS_WIDTH;
	}

	__unreachable();
}

void Display::SetMode(Mode mode)
{
	lv_screen_load(modesScreen[mode]);
}

Display::Mode Display::GetCurrentMode()
{
	return currentMode;
}

lv_obj_t* Display::GetRoot(Mode mode)
{
	return modesScreen[mode];
}
