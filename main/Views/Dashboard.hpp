#pragma once

#include<vector>
#include<map>
#include<string>
#include"lvgl.h"

namespace Dashboard
{
	struct DisplayData
	{
		lv_obj_t *labelName;

		lv_obj_t *scaleTemp;
		lv_style_t tempBarStyle;
		int scaleTempMin;
		int scaleTempMax;
		std::map<std::string, lv_obj_t*> temperatures;

		lv_style_t usageBarStyle;
		std::map<std::string, lv_obj_t*> usages;
	};

	void Init();
}
