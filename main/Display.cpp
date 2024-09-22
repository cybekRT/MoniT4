#include"Display.hpp"

#include <esp_random.h>

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
	unsigned displayW = Display::GetWidth();

	// memset(revbuf, 0, AMOLED_WIDTH * AMOLED_HEIGHT * 2);
	//
	// unsigned col = esp_random();
	// uint16_t *buf = (uint16_t*)revbuf;
	// for(unsigned x = 0; x < AMOLED_WIDTH; x++)
	// {
	// 	buf[0 * AMOLED_WIDTH + x] = col;
	// 	buf[(AMOLED_HEIGHT - 1) * AMOLED_WIDTH + x] = col;
	// }
	//
	// for(unsigned y = 0; y < AMOLED_HEIGHT; y++)
	// {
	// 	buf[y * AMOLED_WIDTH + 0] = col;
	// 	buf[y * AMOLED_WIDTH + AMOLED_WIDTH - 1] = col;
	// }
	//
	// amoled_set_window(0, 0, AMOLED_WIDTH - 1, AMOLED_HEIGHT - 1);
	// amoled_push_buffer((uint16_t*)revbuf, AMOLED_WIDTH * AMOLED_HEIGHT);
	// lv_disp_flush_ready(drv);
	// return;

	auto rotation = lv_display_get_rotation(lv_display_get_default());
	if (rotation == LV_DISPLAY_ROTATION_0)
	{
		unsigned sx = (area->x1 % 2) ? area->x1 - 1 : area->x1;
		unsigned sy = (area->y1 % 2) ? area->y1 - 1 : area->y1;

		unsigned ex = (area->x2 % 2) ? area->x2 : area->x2 + 1;
		unsigned ey = (area->y2 % 2) ? area->y2 : area->y2 + 1;

		unsigned srcW = ex - sx + 1;
		unsigned srcH = ey - sy + 1;

		unsigned offsetSrc;
		unsigned offsetDst;
		for (unsigned y = sy; y <= ey; y++)
		{
			for (unsigned x = sx; x <= ex; x++)
			{
				offsetSrc = y * displayW + x;

				unsigned dx = x - sx;
				unsigned dy = y - sy;
				offsetDst = dy * srcW + dx;
				revbuf[offsetDst * 2 + 0] = color_map[offsetSrc * 2 + 1];
				revbuf[offsetDst * 2 + 1] = color_map[offsetSrc * 2 + 0];
			}
		}

		amoled_set_window(sx, sy, ex, ey);
		amoled_push_buffer((uint16_t*)revbuf, srcW * srcH);
	}
	else if (rotation == LV_DISPLAY_ROTATION_180)
	{
		unsigned sx = (area->x1 % 2) ? area->x1 - 1 : area->x1;
		unsigned sy = (area->y1 % 2) ? area->y1 - 1 : area->y1;

		unsigned ex = (area->x2 % 2) ? area->x2 : area->x2 + 1;
		unsigned ey = (area->y2 % 2) ? area->y2 : area->y2 + 1;

		unsigned srcW = ex - sx + 1;
		unsigned srcH = ey - sy + 1;

		unsigned offsetSrc;
		unsigned offsetDst;
		for (unsigned y = sy; y <= ey; y++)
		{
			for (unsigned x = sx; x <= ex; x++)
			{
				offsetSrc = y * displayW + x;

				unsigned dx = srcW - 1 - (x - sx);
				unsigned dy = srcH - 1 - (y - sy);
				offsetDst = dy * srcW + dx;
				revbuf[offsetDst * 2 + 0] = color_map[offsetSrc * 2 + 1];
				revbuf[offsetDst * 2 + 1] = color_map[offsetSrc * 2 + 0];
			}
		}

		amoled_set_window(AMOLED_WIDTH - 1 - ex, AMOLED_HEIGHT - 1 - ey, AMOLED_WIDTH - 1 - sx, AMOLED_HEIGHT - 1 - sy);
		amoled_push_buffer((uint16_t*)revbuf, srcW * srcH);
	}
	else
	{
		unsigned sx = (area->x1 % 2) ? area->x1 - 1 : area->x1;
		unsigned sy = (area->y1 % 2) ? area->y1 - 1 : area->y1;

		unsigned ex = (area->x2 % 2) ? area->x2 : area->x2 + 1;
		unsigned ey = (area->y2 % 2) ? area->y2 : area->y2 + 1;

		unsigned srcW = ex - sx + 1;
		unsigned srcH = ey - sy + 1;

		unsigned offsetSrc;
		unsigned offsetDst;

		unsigned dx, dy;

		for (unsigned y = sy; y <= ey; y++)
		{
			for (unsigned x = sx; x <= ex; x++)
			{
				offsetSrc = y * displayW + x;

				if (rotation == LV_DISPLAY_ROTATION_90)
				{
					dx = y - sy;
					dy = srcW - 1 - (x - sx);
				}
				else // if (rotation == LV_DISPLAY_ROTATION_270)
				{
					dx = srcH - 1 - (y - sy);
					dy = x - sx;
				}

				offsetDst = dy * srcH + dx;
				revbuf[offsetDst * 2 + 0] = color_map[offsetSrc * 2 + 1];
				revbuf[offsetDst * 2 + 1] = color_map[offsetSrc * 2 + 0];
			}
		}

		if (rotation == LV_DISPLAY_ROTATION_90)
			amoled_set_window(sy, AMOLED_HEIGHT - 1 - ex, ey, AMOLED_HEIGHT - 1 - sx);
		else // if (rotation == LV_DISPLAY_ROTATION_270)
			amoled_set_window(AMOLED_WIDTH - 1 - ey, sx, AMOLED_WIDTH - 1 - sy, ex);

		amoled_push_buffer((uint16_t*)revbuf, srcW * srcH);
	}

	lv_disp_flush_ready(drv);
}


static void example_lvgl_touch_cb(lv_indev_t *drv, lv_indev_data_t *data)
{
	int16_t touchpad_x[1] = { 0 };
	int16_t touchpad_y[1] = { 0 };
	uint8_t touchpad_cnt = 0;

	touchpad_cnt = touch_get_data(touchpad_x, touchpad_y, 1);

	if (touchpad_cnt > 0)
	{
		data->point.x = touchpad_x[0];
		data->point.y = touchpad_y[0];
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
