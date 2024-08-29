#pragma once

#include<vector>
#include<map>
#include<string>
#include"lvgl.h"

namespace Dashboard
{
	struct DisplayData
	{
		char jsonBufferTmp[128];
		std::vector<char> jsonBuffer;

		lv_obj_t *root;
		lv_obj_t *labelName;

		lv_obj_t *scaleTemp;
		lv_style_t tempBarStyle;
		int scaleTempMin;
		int scaleTempMax;
		std::map<std::string, lv_obj_t*> temperatures;

		lv_style_t usageBarStyle;
		std::map<std::string, lv_obj_t*> usages;

		std::map<std::string, lv_obj_t*> storages;
		std::map<std::string, lv_obj_t*> storagesValues;

		std::map<std::string, lv_obj_t*> networks;
	};

	void Init();
}
