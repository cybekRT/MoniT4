#pragma once

#include<vector>
#include<map>
#include<string>
#include"lvgl.h"

namespace Dashboard
{
	struct DisplayData
	{
		lv_obj_t *scaleTemp;
		int scaleTempMin;
		int scaleTempMax;
		std::map<std::string, lv_obj_t*> temperatures;
	};

	void Init();
}
