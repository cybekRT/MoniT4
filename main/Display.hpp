#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

namespace Display
{
	constexpr unsigned PHYS_WIDTH = 450;
	constexpr unsigned PHYS_HEIGHT = 600;

	enum Mode { Disconnected = 0, Connected = 1, Count };

	bool Init();

	bool Lock(int timeout_ms);
	void Unlock();

	int GetWidth();
	int GetHeight();

	void SetMode(Mode mode);
	Mode GetCurrentMode();
	lv_obj_t* GetRoot(Mode mode);
}
