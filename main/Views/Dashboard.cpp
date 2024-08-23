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

#define TAG "dashboard"

int serverSock;
lv_obj_t *tabView;

// char jsonBuffer[4096];
// unsigned jsonBufferLen = 0;
// char jsonBufferTmp[128];
// std::vector<char> jsonBuffer;

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

	// auto root = Display::GetRoot(Display::Mode::Connected);

	auto root = displayData->root = lv_tabview_add_tab(tabView, displayInitData.name.c_str());
	lv_tabview_set_active(tabView, lv_tabview_get_tab_count(tabView) - 1, LV_ANIM_OFF);
	lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
	// lv_obj_set_style_border_width(tabView, 0, LV_PART_MAIN);
	lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

	auto rootWidth = lv_obj_get_content_width(root);
	auto rootHeight = lv_obj_get_content_height(root);

	ESP_LOGI(TAG, "Root size: %ldx%ld", lv_obj_get_width(root), lv_obj_get_height(root));
	ESP_LOGI(TAG, "Root2 size: %ldx%ld", lv_obj_get_content_width(root), lv_obj_get_content_height(root));

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

	lv_obj_set_size(scaleTemp, rootWidth - 40, 50);
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
	int usageBarHeight = rootHeight - barOffsetY;

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

		lv_obj_set_pos(label, usageBarOffsetX + usageBarWidth / 2 - labelWidth / 2, rootHeight - labelHeight);

		auto bar = displayData->usages[sensorName] = lv_bar_create(root);
		lv_bar_set_range(bar, 0, 100);
		lv_bar_set_value(bar, 0, LV_ANIM_OFF);

		lv_obj_set_size(bar, usageBarWidth, usageBarHeight - labelHeight - 5);
		lv_obj_set_pos(bar, usageBarOffsetX, usageBarOffsetY);
		lv_obj_set_style_anim_duration(bar, 500, LV_PART_MAIN);
		lv_obj_add_style(bar, usageBarStyle, LV_PART_INDICATOR);

		usageBarOffsetX += usageBarWidth + 5;
	}

	/****************************** STORAGE ******************************/

	int storageBarOffsetX = usageBarOffsetX + 5;
	int storageBarOffsetY = barOffsetY + 5;
	int storageBarWidth = rootWidth - storageBarOffsetX - 5;
	int storageBarHeight = 25;
	for(auto& sensorName : displayInitData.storage)
	{
		auto font = &lv_font_montserrat_20;

		auto bar = displayData->storages[sensorName] = lv_bar_create(root);
		lv_bar_set_range(bar, 0, 100);
		lv_obj_set_size(bar, storageBarWidth, storageBarHeight);
		lv_obj_set_pos(bar, storageBarOffsetX, storageBarOffsetY);

		auto labelName = lv_label_create(bar);
		lv_label_set_text(labelName, sensorName.c_str());
		lv_obj_set_style_text_font(labelName, font, LV_PART_MAIN);
		lv_obj_set_style_text_color(labelName, lv_color_make(255, 255, 255), LV_PART_MAIN);
		lv_obj_align(labelName, LV_ALIGN_LEFT_MID, 10, 0);

		auto labelValue = displayData->storagesValues[sensorName] = lv_label_create(bar);
		lv_label_set_text(labelValue, "");
		lv_obj_set_style_text_font(labelValue, font, LV_PART_MAIN);
		lv_obj_set_style_text_color(labelValue, lv_color_make(255, 255, 255), LV_PART_MAIN);
		lv_obj_align(labelValue, LV_ALIGN_RIGHT_MID, -10, 0);

		storageBarOffsetY += storageBarHeight + 5;
	}

	/****************************** NETWORK ******************************/
	auto netFont = &lv_font_montserrat_32;
	int netBarOffsetX = usageBarOffsetX + 5;//storageBarOffsetX;
	int netBarOffsetY = storageBarOffsetY + 5;
	int netContainerOffsetY = 5;
	int netBarWidth = rootWidth - netBarOffsetX - 5;
	int netBarHeight = lv_font_get_line_height(netFont);// 25;

	auto netContainer = lv_obj_create(root);
	lv_obj_set_size(netContainer, netBarWidth, 5 + 5 + (netBarHeight + 5) * displayInitData.network.size());
	lv_obj_set_pos(netContainer, netBarOffsetX, netBarOffsetY);
	lv_obj_set_style_bg_color(netContainer, lv_color_make(32, 32, 32), LV_PART_MAIN);
	lv_obj_set_style_pad_all(netContainer, 0, LV_PART_MAIN);
	// lv_obj_set_style_pad_all(netContainer, 0, LV_PART_ANY);
	// lv_obj_set_style_border_width(netContainer, 0, LV_PART_MAIN);
	// lv_obj_set_style_border_width(netContainer, 0, LV_PART_ANY);

	for(auto& sensorName : displayInitData.network)
	{
		auto labelName = lv_label_create(netContainer);
		lv_label_set_text(labelName, sensorName.c_str());
		lv_obj_set_style_text_font(labelName, netFont, LV_PART_MAIN);
		lv_obj_set_style_text_color(labelName, lv_color_make(255, 255, 255), LV_PART_MAIN);
		lv_obj_align(labelName, LV_ALIGN_TOP_LEFT, 10, netContainerOffsetY);

		auto labelValue = displayData->networks[sensorName] = lv_label_create(netContainer);
		lv_label_set_text(labelValue, "---");
		lv_obj_set_style_text_font(labelValue, netFont, LV_PART_MAIN);
		lv_obj_set_style_text_color(labelValue, lv_color_make(255, 255, 255), LV_PART_MAIN);
		lv_obj_align(labelValue, LV_ALIGN_TOP_RIGHT, -10, netContainerOffsetY);

		netBarOffsetY += netBarHeight + 10;
		netContainerOffsetY += netBarHeight + 5;
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

void UpdateStorage(Dashboard::DisplayData *displayData, std::string name, int value)
{
	if(!displayData)
	{
		ESP_LOGE(TAG, "JSON data without init");
		return;
	}

	if(!displayData->storages.contains(name) || !displayData->storagesValues.contains(name))
	{
		ESP_LOGE(TAG, "Invalid storage \"%s\" = %d", name.c_str(), value);
		return;
	}

	if(value < 0 || value > 100)
	{
		ESP_LOGE(TAG, "Invalid storage value: %d", value);
		return;
	}

	auto storageBar = displayData->storages[name];
	auto storageLabel = displayData->storagesValues[name];

	lv_bar_set_value(storageBar, value, LV_ANIM_ON);
	lv_label_set_text_fmt(storageLabel, "%d%%", value);
}

void UpdateNetwork(Dashboard::DisplayData *displayData, std::string name, int value)
{
	if(!displayData)
	{
		ESP_LOGE(TAG, "JSON data without init");
		return;
	}

	if(!displayData->networks.contains(name))
	{
		ESP_LOGE(TAG, "Invalid network \"%s\" = %d", name.c_str(), value);
		return;
	}

	auto networkLabel = displayData->networks[name];

	if(value < 0)
	{
		lv_label_set_text(networkLabel, "Timeout");
		lv_obj_set_style_text_color(networkLabel, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
	}
	else
	{
		lv_label_set_text_fmt(networkLabel, "%dms", value);
		lv_obj_set_style_text_color(networkLabel, lv_color_make(255, 255, 255), LV_PART_MAIN);
	}
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
	auto root = cJSON_ParseWithLengthOpts(displayData->jsonBuffer.data(),
		displayData->jsonBuffer.size(), &jsonRemaining, false);
	if(!root)
		return 0;

	Display::Lock(-1);
	cJSON *currentElem;
	cJSON_ArrayForEach(currentElem, root)
	{
		ESP_LOGI(TAG, "Element: %s", currentElem->string);
		if(strcmp(currentElem->string, "name") == 0)
		{
			lv_label_set_text(displayData->labelName, currentElem->valuestring);
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
		else if(strcmp(currentElem->string, "storage") == 0)
		{
			ESP_LOGI(TAG, "Is \"usage\" an array: %d", cJSON_IsArray(currentElem));
			ESP_LOGI(TAG, "Is \"usage\" an object: %d", cJSON_IsObject(currentElem));
			cJSON *currentStorage;
			cJSON_ArrayForEach(currentStorage, currentElem)
			{
				ESP_LOGI(TAG, "Iterate for: %s", currentStorage->string);
				UpdateStorage(displayData, currentStorage->string, (int)currentStorage->valuedouble);
			}
		}
		else if(strcmp(currentElem->string, "network") == 0)
		{
			ESP_LOGI(TAG, "Is \"usage\" an array: %d", cJSON_IsArray(currentElem));
			ESP_LOGI(TAG, "Is \"usage\" an object: %d", cJSON_IsObject(currentElem));
			cJSON *currentNetwork;
			cJSON_ArrayForEach(currentNetwork, currentElem)
			{
				ESP_LOGI(TAG, "Iterate for: %s", currentNetwork->string);
				UpdateNetwork(displayData, currentNetwork->string, (int)currentNetwork->valuedouble);
			}
		}
		else
		{
			ESP_LOGI(TAG, "Invalid tag: %s", currentElem->string);
		}
	}

	Display::Unlock();

	cJSON_Delete(root);
	return jsonRemaining - displayData->jsonBuffer.data();
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
		// DisplayLocalIP(nullptr);
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
		// auto root = Display::GetRoot(Display::Mode::Connected);
		// for(auto id = 0; id < lv_obj_get_child_count(root); id++)
		// {
		// 	auto child = lv_obj_get_child(root, id);
		// 	lv_obj_delete(child);
		// }

		auto displayData = new Dashboard::DisplayData();
		auto displayDataInit = DisplayInitData();

		displayDataInit.name = "MoniT4";

		displayDataInit.temperature.push_back("cpu");
		displayDataInit.temperature.push_back("gpu");
		displayDataInit.temperature.push_back("psu");

		displayDataInit.usage.push_back("cpu");
		displayDataInit.usage.push_back("ram");
		displayDataInit.usage.push_back("swap");

		displayDataInit.storage.push_back("/");
		displayDataInit.storage.push_back("/boot");
		displayDataInit.storage.push_back("/home");

		displayDataInit.network.push_back("8.8.8.8");
		displayDataInit.network.push_back("10.0.2.3");
		displayDataInit.network.push_back("192.168.100.1");
		displayDataInit.network.push_back("bing.com");

		CreateContainer(displayData, displayDataInit);

		Display::Unlock();

		bool validConnection = true;
		while(validConnection)
		{
			auto bufLen = recv(clientSock, displayData->jsonBufferTmp, sizeof(displayData->jsonBufferTmp), 0);
			if(bufLen == 0)// || bufLen + jsonBufferLen >= sizeof(jsonBuffer))
			{
				ESP_LOGI(TAG, "Invalid data or disconnected... %d", bufLen);
				break;
			}

			displayData->jsonBuffer.insert(displayData->jsonBuffer.end(),
				displayData->jsonBufferTmp, displayData->jsonBufferTmp + bufLen);

			ESP_LOGI(TAG, "Recv: %.*s", displayData->jsonBuffer.size(), displayData->jsonBuffer.data());

			for(;;) // Parse all jsons in buffer
			{
				auto parsedLen = ParseJson(displayData);
				if(parsedLen == 0)
				{
					if(displayData->jsonBuffer.size() > 8192)
					{
						ESP_LOGE(TAG, "Too much non-parseable data in buffer: %d", displayData->jsonBuffer.size());
						validConnection = false;
						break;
					}
					else
						ESP_LOGI(TAG, "Need more data...");
					break;
				}

				// memcpy(jsonBuffer, jsonBuffer + parsedLen, jsonBufferLen - parsedLen);
				// jsonBufferLen -= parsedLen;
				displayData->jsonBuffer.erase(displayData->jsonBuffer.begin(), displayData->jsonBuffer.begin() + parsedLen);
				ESP_LOGI(TAG, "Parsed: %u, Remaining: %u", parsedLen, displayData->jsonBuffer.size());
			}
		}

		Display::Lock(-1);
		lv_obj_delete(displayData->root);
		Display::Unlock();
		delete displayData;

		// jsonBufferLen = 0;
		// displayData->jsonBuffer.clear();
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

void DisplayLocalIP(lv_event_t* e)
{
	char tmp[32];
	esp_netif_ip_info_t ipInfo;
	esp_netif_get_ip_info(esp_netif_get_default_netif(), &ipInfo);

	esp_ip4addr_ntoa(&ipInfo.ip, tmp, 32);
	lv_label_set_text_fmt((lv_obj_t*)e->user_data, "Waiting for client...\nAddress: %s", tmp);
}

void InitViews()
{
	auto root = Display::GetRoot(Display::Mode::Connected);

	tabView = lv_tabview_create(root);
	lv_obj_set_pos(tabView, 0, 0);
	lv_obj_set_size(tabView, Display::GetWidth(), Display::GetHeight());
	lv_obj_set_style_pad_all(tabView, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(tabView, 0, LV_PART_MAIN);

	auto tabViewBar = lv_tabview_get_tab_bar(tabView);
	lv_obj_add_flag(tabViewBar, LV_OBJ_FLAG_HIDDEN);

	auto tabMain = lv_tabview_add_tab(tabView, "Disconnected");
	lv_obj_set_style_pad_all(tabMain, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(tabMain, 0, LV_PART_MAIN);

	auto label = lv_label_create(tabMain);
	lv_label_set_text(label, "Waiting for client...");
	lv_obj_center(label);

	lv_obj_add_event_cb(root, DisplayLocalIP, LV_EVENT_SCREEN_LOADED, label);
}

void Dashboard::Init()
{
	InitViews();
	InitSocket();
}
