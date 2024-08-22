#include"Dashboard.hpp"
#include"Display.hpp"

#include"freertos/FreeRTOS.h"
#include"freertos/task.h"
#include"freertos/event_groups.h"
#include"esp_log.h"
#include"esp_system.h"
#include"esp_wifi.h"
#include"esp_event.h"
#include"lwip/err.h"
#include"lwip/sys.h"
#include"lwip/sockets.h"
#include"lvgl.h"
#include"cJSON.h"

// LV_IMAGE_DECLARE(dashboardDsc);
#define TAG "dashboard"

// lv_obj_t *imgBg;
// lv_obj_t *lName;
// lv_obj_t *lUptime;
// lv_obj_t *lIP;
// lv_obj_t *barCpu;
// lv_obj_t *barRam;
// lv_obj_t *barSwap;

int serverSock;

char jsonBuffer[4096];
unsigned jsonBufferLen = 0;

// Dashboard::DisplayData displayData;

struct DisplayInitData
{
	std::string name;
	std::vector<std::string> usage;
	std::vector<std::string> storage;
	std::vector<std::string> temperature;
	std::vector<std::string> network;
};

void CreateContainer(Dashboard::DisplayData *displayData, DisplayInitData &displayInitData)
{
	Display::Lock(-1);

	auto root = Display::GetRoot(Display::Mode::Connected);
	// lv_obj_add_event_cb(root, DisplayLocalIP, LV_EVENT_SCREEN_LOADED, nullptr);

	/****************************** NAME ******************************/
	auto labelNameFont = &lv_font_montserrat_14;
	auto labelName = displayData->labelName = lv_label_create(root);
	lv_label_set_text(labelName, displayInitData.name.c_str());
	lv_obj_set_style_text_font(labelName, labelNameFont, LV_PART_MAIN);
	lv_obj_align(labelName, LV_ALIGN_BOTTOM_MID, 0, 0);//-lv_font_get_line_height(labelNameFont));

	labelName = lv_label_create(root);
	lv_label_set_text(labelName, displayInitData.name.c_str());
	lv_obj_set_style_text_font(labelName, labelNameFont, LV_PART_MAIN);
	lv_obj_align(labelName, LV_ALIGN_TOP_MID, 0, 0);//-lv_font_get_line_height(labelNameFont));

	/****************************** TEMPERATURE ******************************/
	auto scaleTemp = displayData->scaleTemp = lv_scale_create(root);
	lv_scale_set_mode(scaleTemp, LV_SCALE_MODE_HORIZONTAL_TOP);
	lv_scale_set_label_show(scaleTemp, true);
	// lv_obj_set_style_length(scaleTemp, 10, LV_PART_ITEMS);
	lv_obj_set_style_length(scaleTemp, 5, LV_PART_INDICATOR);

	ESP_LOGE(TAG, "SCALE: %ld, %ld, %ld", lv_obj_get_height(scaleTemp), lv_obj_get_style_height(scaleTemp, 0), lv_obj_get_content_height(scaleTemp));

	lv_obj_set_size(scaleTemp, Display::GetWidth() - 40, 50);
	lv_obj_set_pos(scaleTemp, 20, 0);

	displayData->scaleTemp = scaleTemp;
	displayData->scaleTempMin = 999;
	displayData->scaleTempMax = -999;

	auto tempBarStyle = &displayData->tempBarStyle;
	lv_style_init(tempBarStyle);
	lv_style_set_bg_opa(tempBarStyle, LV_OPA_COVER);
	// lv_style_set_bg_color(tempBarStyle, lv_palette_main(LV_PALETTE_RED));
	// lv_style_set_bg_grad_color(tempBarStyle, lv_palette_main(LV_PALETTE_BLUE));
	lv_style_set_bg_grad_dir(tempBarStyle, LV_GRAD_DIR_HOR);

	int barOffsetY = lv_obj_get_style_y(scaleTemp, 0) + lv_obj_get_style_height(scaleTemp, 0);
	for(auto& sensorName : displayInitData.temperature)
	{
		auto temp = displayData->temperatures[sensorName] = lv_bar_create(root);
		lv_obj_set_size(temp, lv_obj_get_style_width(scaleTemp, 0), 30);
		lv_obj_set_pos(temp, lv_obj_get_style_x(scaleTemp, 0), barOffsetY);
		lv_bar_set_mode(temp, LV_BAR_MODE_RANGE);
		lv_bar_set_range(temp, displayData->scaleTempMin, displayData->scaleTempMax);
		lv_bar_set_value(temp, 0, LV_ANIM_OFF);
		lv_obj_set_style_anim_duration(temp, 500, LV_PART_MAIN);

		// lv_obj_set_style_bg_color(temp, lv_color_make(64, 0, 0), LV_PART_MAIN);
		// lv_obj_set_style_bg_color(temp, lv_color_make(255, 0, 255), LV_PART_INDICATOR);
		lv_obj_set_style_radius(temp, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_radius(temp, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
		lv_obj_add_style(temp, tempBarStyle, LV_PART_INDICATOR);

		// Add label
		for(auto&[name, parent] : displayData->temperatures)
		{
			auto upperName = name;
			for(auto& c : upperName)
				c = (char)toupper(c);

			auto label = lv_label_create(parent);
			lv_label_set_text(label, upperName.c_str());
			lv_obj_set_style_text_color(label, lv_color_make(255, 255, 255), LV_PART_MAIN);
			lv_obj_center(label);
			// lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
		}

		barOffsetY += 35;
	}

	/****************************** USAGE ******************************/
	int usageBarOffsetX = 0;
	int usageBarOffsetY = barOffsetY + 5;
	int usageBarWidth = 30;
	int usageBarHeight = Display::GetHeight() - barOffsetY;

	auto usageBarStyle = &displayData->usageBarStyle;
	lv_style_init(usageBarStyle);
	lv_style_set_bg_opa(usageBarStyle, LV_OPA_COVER);
	lv_style_set_bg_color(usageBarStyle, lv_palette_main(LV_PALETTE_RED));
	lv_style_set_bg_grad_color(usageBarStyle, lv_palette_main(LV_PALETTE_BLUE));
	lv_style_set_bg_grad_dir(usageBarStyle, LV_GRAD_DIR_VER);

	for(auto& sensorName : displayInitData.usage)
	{
		auto font = &lv_font_montserrat_14;

		auto label = lv_label_create(root);
		lv_label_set_text(label, sensorName.c_str());
		lv_obj_set_style_text_font(label, font, LV_PART_MAIN);

		int labelWidth = 0;//lv_obj_get_width(label);
		for(auto& c : sensorName)
			labelWidth += lv_font_get_glyph_width(font, c, 0);
		int labelHeight = lv_font_get_line_height(font); //lv_obj_get_style_height(label, 0);
		ESP_LOGI(TAG, "Bar label size: %dx%d, %ld", labelWidth, labelHeight, lv_obj_get_height(label));

		lv_obj_set_pos(label, usageBarOffsetX + usageBarWidth / 2 - labelWidth / 2, Display::GetHeight() - labelHeight);

		auto bar = displayData->usages[sensorName] = lv_bar_create(root);
		lv_bar_set_range(bar, 0, 100);
		lv_bar_set_value(bar, 0, LV_ANIM_OFF);

		lv_obj_set_size(bar, usageBarWidth, usageBarHeight - labelHeight - 5);
		lv_obj_set_pos(bar, usageBarOffsetX, usageBarOffsetY);
		lv_obj_set_style_anim_duration(bar, 500, LV_PART_MAIN);
		lv_obj_add_style(bar, usageBarStyle, LV_PART_INDICATOR);

		usageBarOffsetX += usageBarWidth + 5;
	}

	Display::Unlock();
}

void UpdateTemperature(Dashboard::DisplayData *displayData, std::string name, int value)
{
	if(!displayData)
	{
		ESP_LOGE(TAG, "JSON data without init");
		return;
	}

	if(!displayData->temperatures.contains(name))
	{
		ESP_LOGI(TAG, "Invalid temperature: \"%s\" = %d", name.c_str(), value);
		return;
	}

	if(value > displayData->scaleTempMax)
		displayData->scaleTempMax = value;
	if(value < displayData->scaleTempMin)
		displayData->scaleTempMin = value;

	lv_scale_set_range(displayData->scaleTemp, displayData->scaleTempMin, displayData->scaleTempMax);
	ESP_LOGI(TAG, "Temp range: <%d, %d>", displayData->scaleTempMin, displayData->scaleTempMax);

	int minMajor = (displayData->scaleTempMin - 9) / 10;
	int maxMajor = (displayData->scaleTempMax + 9) / 10;
	lv_scale_set_total_tick_count(displayData->scaleTemp, (maxMajor - minMajor) * 10 + 1);
	ESP_LOGI(TAG, "Ticks: %d, %d, %d", minMajor, maxMajor, (maxMajor - minMajor) * 10);
	lv_scale_set_major_tick_every(displayData->scaleTemp, 10);

	for(auto& itr : displayData->temperatures)
	{
		lv_bar_set_range(itr.second, displayData->scaleTempMin, displayData->scaleTempMax);
		ESP_LOGI(TAG, "Range for \"%s\" = <%d, %d>", itr.first.c_str(), displayData->scaleTempMin, displayData->scaleTempMax);
	}

	lv_bar_set_value(displayData->temperatures[name], value, LV_ANIM_ON);
	ESP_LOGI(TAG, "Set value for \"%s\" = %d", name.c_str(), value);

	int minBlue = 255 - int(255.0 / 100.0 * displayData->scaleTempMin);
	int minRed = int(255.0 / 100.0 * displayData->scaleTempMin);
	int maxBlue = 255 - int(255.0 / 100.0 * displayData->scaleTempMax);
	int maxRed = int(255.0 / 100.0 * displayData->scaleTempMax);
	ESP_LOGI(TAG, "Min: %d, %d, Max: %d, %d", minBlue, minRed, maxBlue, maxRed);
	lv_style_set_bg_color(&displayData->tempBarStyle, lv_color_make(minRed, 0, minBlue));
	lv_style_set_bg_grad_color(&displayData->tempBarStyle, lv_color_make(maxRed, 0, maxBlue));
}

void UpdateUsage(Dashboard::DisplayData *displayData, std::string name, int value)
{
	if(!displayData)
	{
		ESP_LOGE(TAG, "JSON data without init");
		return;
	}

	if(!displayData->usages.contains(name))
	{
		ESP_LOGE(TAG, "Invalid usage: \"%s\" = %d", name.c_str(), value);
		return;
	}

	if(value < 0 || value > 100)
	{
		ESP_LOGE(TAG, "Invalid usage value: %d", value);
		return;
	}

	lv_bar_set_value(displayData->usages[name], value, LV_ANIM_ON);
	ESP_LOGI(TAG, "Set value for \"%s\" = %d", name.c_str(), value);
}



void DisplayLocalIP(lv_event_t* e)
{
	// char tmp[32];
	// esp_netif_ip_info_t ipInfo;
	// esp_netif_get_ip_info(esp_netif_get_default_netif(), &ipInfo);
	//
	// esp_ip4addr_ntoa(&ipInfo.ip, tmp, 32);
	// lv_label_set_text(lIP, tmp);
}

void ClearViews()
{
	// lv_label_set_text(lName, "MoniT4");
	// lv_label_set_text(lUptime, "");
	// DisplayLocalIP(nullptr);
	//
	// lv_bar_set_value(barCpu, 0, LV_ANIM_OFF);
	// lv_bar_set_value(barRam, 0, LV_ANIM_OFF);
	// lv_bar_set_value(barSwap, 0, LV_ANIM_OFF);
}

unsigned ParseJson(Dashboard::DisplayData *displayData)
{
	const char *jsonRemaining;
	auto root = cJSON_ParseWithLengthOpts(jsonBuffer, jsonBufferLen, &jsonRemaining, false);
	if(!root)
		return 0;

	Display::Lock(-1);
	cJSON *currentElem;
	cJSON_ArrayForEach(currentElem, root)
	{
		ESP_LOGI(TAG, "Element: %s", currentElem->string);
		if(strcmp(currentElem->string, "name") == 0)
		{
			// lv_label_set_text(lName, currentElem->valuestring);
		}
		else if(strcmp(currentElem->string, "temperature") == 0)
		{
			ESP_LOGI(TAG, "Is \"temperature\" an array: %d", cJSON_IsArray(currentElem));
			ESP_LOGI(TAG, "Is \"temperature\" an object: %d", cJSON_IsObject(currentElem));
			cJSON *currentTemp;
			cJSON_ArrayForEach(currentTemp, currentElem)
			{
				ESP_LOGI(TAG, "Iterate for: %s", currentTemp->string);
				UpdateTemperature(displayData, currentTemp->string, (int)currentTemp->valuedouble);
			}
		}
		else if(strcmp(currentElem->string, "usage") == 0)
		{
			ESP_LOGI(TAG, "Is \"usage\" an array: %d", cJSON_IsArray(currentElem));
			ESP_LOGI(TAG, "Is \"usage\" an object: %d", cJSON_IsObject(currentElem));
			cJSON *currentUsage;
			cJSON_ArrayForEach(currentUsage, currentElem)
			{
				ESP_LOGI(TAG, "Iterate for: %s", currentUsage->string);
				UpdateUsage(displayData, currentUsage->string, (int)currentUsage->valuedouble);
			}
		}
		// else if(strcmp(currentElem->string, "uptime") == 0)
		// {
		// 	unsigned days, hours, minutes;
		//
		// 	auto uptime = (int)currentElem->valuedouble / 60;
		// 	minutes = uptime % 60;
		// 	uptime /= 60;
		// 	hours = uptime % 24;
		// 	days = uptime / 24;
		//
		// 	// lv_label_set_text_fmt(lUptime, "%d", (int)currentElem->valuedouble);
		// 	lv_label_set_text_fmt(lUptime, "%lud %luh %lum", days, hours, minutes);
		// }
		// else if(strcmp(currentElem->string, "cpu") == 0)
		// {
		// 	lv_bar_set_value(barCpu, (int)currentElem->valuedouble, LV_ANIM_ON);
		// }
		// else if(strcmp(currentElem->string, "ram") == 0)
		// {
		// 	lv_bar_set_value(barRam, (int)currentElem->valuedouble, LV_ANIM_ON);
		// }
		// else if(strcmp(currentElem->string, "swap") == 0)
		// {
		// 	lv_bar_set_value(barSwap, (int)currentElem->valuedouble, LV_ANIM_ON);
		// }
		else
		{
			ESP_LOGI(TAG, "Invalid tag: %s", currentElem->string);
		}
	}

	Display::Unlock();

	cJSON_Delete(root);
	return jsonRemaining - jsonBuffer;
}

void ServerTask(void*)
{
	// FIXME: add static to free the stack
	sockaddr_in clientAddr;
	socklen_t clientAddrLen;
	int clientSock;
	char clientIP[32];

	int keepAlive = 1;
	int keepIdle = 300;
	int keepInterval = 300;
	int keepCount = 3;

	for(;;)
	{
		// Reset fields to defaults...
		Display::Lock(-1);
		// lv_label_set_text(lName, "MoniT4");
		DisplayLocalIP(nullptr);
		Display::Unlock();

		ESP_LOGI(TAG, "Waiting for client...");
		clientAddrLen = sizeof(clientAddr);
		clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientAddrLen);
		if(clientSock < 0)
		{
			ESP_LOGI(TAG, "Couldn't accept");
			continue;
		}
		else
			ESP_LOGI(TAG, "Accepted~!");

		setsockopt(clientSock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
		setsockopt(clientSock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
		setsockopt(clientSock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
		setsockopt(clientSock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));

		inet_ntoa_r(clientAddr.sin_addr, clientIP, sizeof(clientIP) - 1);
		ESP_LOGI(TAG, "Client: %s", clientIP);
		// if(Display::Lock(-1))
		// {
		// 	// lv_label_set_text(lIP, clientIP);
		// 	Display::Unlock();
		// }

		Display::Lock(-1);
		auto root = Display::GetRoot(Display::Mode::Connected);
		for(auto id = 0; id < lv_obj_get_child_count(root); id++)
		{
			auto child = lv_obj_get_child(root, id);
			lv_obj_delete(child);
		}

		auto displayData = new Dashboard::DisplayData();
		auto displayDataInit = DisplayInitData();

		displayDataInit.name = "MoniT4";

		displayDataInit.temperature.push_back("cpu");
		// displayDataInit.temperature.push_back("gpu");
		// displayDataInit.temperature.push_back("psu");

		displayDataInit.usage.push_back("cpu");
		displayDataInit.usage.push_back("ram");
		displayDataInit.usage.push_back("swap");

		CreateContainer(displayData, displayDataInit);

		Display::Unlock();

		for(;;)
		{
			// auto bufLen = recv(clientSock, buf, 31, 0); //MSG_WAITALL
			auto bufLen = recv(clientSock, jsonBuffer + jsonBufferLen, sizeof(jsonBuffer) - jsonBufferLen, 0);
			if(bufLen == 0 || bufLen + jsonBufferLen >= sizeof(jsonBuffer))
			{
				ESP_LOGI(TAG, "Invalid data or disconnected... %d + %d", bufLen, jsonBufferLen);
				break;
			}
			jsonBufferLen += bufLen;
			jsonBuffer[jsonBufferLen] = 0;
			ESP_LOGI(TAG, "Recv: %s", jsonBuffer);

			for(;;) // Parse all jsons in buffer
			{
				auto parsedLen = ParseJson(displayData);
				if(parsedLen == 0)
				{
					ESP_LOGI(TAG, "Need more data...");
					break;
				}

				memcpy(jsonBuffer, jsonBuffer + parsedLen, jsonBufferLen - parsedLen);
				jsonBufferLen -= parsedLen;
				ESP_LOGI(TAG, "Parsed: %u, Remaining: %u", parsedLen, jsonBufferLen);
			}
		}

		delete displayData;

		jsonBufferLen = 0;
		ESP_LOGI(TAG, "Clearing client socket");
		shutdown(clientSock, 0);
		close(clientSock);
	}
}

void InitSocket()
{
	sockaddr_in sockAddr = {
		.sin_len = sizeof(sockAddr),
		.sin_family = AF_INET,
		.sin_port = htons(8000),
		.sin_addr = {
			.s_addr = htonl(INADDR_ANY),
		}
	};

	serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	assert(serverSock >= 0);

	auto err = bind(serverSock, (sockaddr*)&sockAddr, sizeof(sockAddr));
	assert(err == 0);

	ESP_LOGI(TAG, "Started listening...");
	listen(serverSock, 1);

	xTaskCreate(ServerTask, "Server", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);
}

void InitViews()
{
	auto root = Display::GetRoot(Display::Mode::Connected);
	auto label = lv_label_create(root);
	lv_label_set_text(label, "Waiting for client...");
	lv_obj_center(label);
}

void Dashboard::Init()
{
	InitViews();
	InitSocket();
}
