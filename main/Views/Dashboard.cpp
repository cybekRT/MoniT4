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

LV_IMAGE_DECLARE(dashboardDsc);
#define TAG "dashboard"

lv_obj_t *imgBg;
lv_obj_t *lName;
lv_obj_t *lUptime;
lv_obj_t *lIP;
lv_obj_t *barCpu;
lv_obj_t *barRam;
lv_obj_t *barSwap;

int serverSock;

char jsonBuffer[4096];
unsigned jsonBufferLen = 0;

Dashboard::DisplayData displayData;


void UpdateTemperature(std::string name, int value)
{
	if(!displayData.temperatures.contains(name))
	{
		ESP_LOGI(TAG, "Invalid temperature: \"%s\" = %d", name.c_str(), value);
		return;
	}

	if(value > displayData.scaleTempMax)
		displayData.scaleTempMax = value;
	if(value < displayData.scaleTempMin)
		displayData.scaleTempMin = value;

	lv_scale_set_range(displayData.scaleTemp, displayData.scaleTempMin, displayData.scaleTempMax);
	ESP_LOGI(TAG, "Temp range: <%d, %d>", displayData.scaleTempMin, displayData.scaleTempMax);

	lv_bar_set_value(displayData.temperatures[name], value, LV_ANIM_ON);
	ESP_LOGI(TAG, "Set value for \"%s\" = %d", name.c_str(), value);

	for(auto& itr : displayData.temperatures)
	{
		lv_bar_set_range(itr.second, displayData.scaleTempMin, displayData.scaleTempMax);
		ESP_LOGI(TAG, "Range for \"%s\" = <%d, %d>", itr.first.c_str(), displayData.scaleTempMin, displayData.scaleTempMax);
	}
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

unsigned ParseJson()
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
				UpdateTemperature(currentTemp->string, (int)currentTemp->valuedouble);
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
		if(Display::Lock(-1))
		{
			// lv_label_set_text(lIP, clientIP);
			Display::Unlock();
		}

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

			auto parsedLen = ParseJson();
			if(parsedLen == 0)
			{
				ESP_LOGI(TAG, "Need more data...");
				// break;
				continue;
			}

			memcpy(jsonBuffer, jsonBuffer + parsedLen, jsonBufferLen - parsedLen);
			jsonBufferLen -= parsedLen;
			ESP_LOGI(TAG, "Parsed: %u, Remaining: %u", parsedLen, jsonBufferLen);
		}

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
	Display::Lock(-1);

	auto root = Display::GetRoot(Display::Mode::Connected);
	// lv_obj_add_event_cb(root, DisplayLocalIP, LV_EVENT_SCREEN_LOADED, nullptr);

	auto scaleTemp = lv_scale_create(root);
	lv_scale_set_mode(scaleTemp, LV_SCALE_MODE_HORIZONTAL_TOP);
	lv_scale_set_label_show(scaleTemp, true);
	lv_obj_set_size(scaleTemp, 400, 100);
	lv_obj_set_pos(scaleTemp, 30, 10);

	displayData.scaleTemp = scaleTemp;
	displayData.scaleTempMin = 40;
	displayData.scaleTempMax = 60;

	auto cpuTemp = lv_bar_create(root);
	displayData.temperatures["cpu"] = cpuTemp;
	ESP_LOGI(TAG, "x Size: %ldx%ld", lv_obj_get_style_width(scaleTemp, 0), lv_obj_get_style_height(scaleTemp, 0));

	ESP_LOGI(TAG, "x Pos: %ldx%ld", lv_obj_get_style_x(scaleTemp, 0), lv_obj_get_style_y(scaleTemp, 0));
	lv_obj_set_size(cpuTemp, lv_obj_get_style_width(scaleTemp, 0), 30);
	lv_obj_set_pos(cpuTemp, lv_obj_get_style_x(scaleTemp, 0), lv_obj_get_style_y(scaleTemp, 0) + lv_obj_get_style_height(scaleTemp, 0) + 30 * 0);
	lv_bar_set_mode(cpuTemp, LV_BAR_MODE_RANGE);
	lv_bar_set_range(cpuTemp, 0, 100);
	lv_bar_set_value(cpuTemp, 50, LV_ANIM_OFF);

	lv_obj_set_style_bg_color(cpuTemp, lv_color_make(64, 0, 0), LV_PART_MAIN);
	lv_obj_set_style_bg_color(cpuTemp, lv_color_make(255, 0, 255), LV_PART_INDICATOR);

	auto gpuTemp = lv_bar_create(root);
	displayData.temperatures["gpu"] = gpuTemp;
	lv_obj_set_size(gpuTemp, lv_obj_get_style_width(scaleTemp, 0), 30);
	lv_obj_set_pos(gpuTemp, lv_obj_get_style_x(scaleTemp, 0), lv_obj_get_style_y(scaleTemp, 0) + lv_obj_get_style_height(scaleTemp, 0) + 30 * 1);
	lv_bar_set_mode(gpuTemp, LV_BAR_MODE_RANGE);
	lv_bar_set_range(gpuTemp, 0, 100);
	lv_bar_set_value(gpuTemp, 50, LV_ANIM_OFF);

	for(auto& itr : displayData.temperatures)
	{
		auto name = itr.first;
		for(auto& c : name)
			c = toupper(c);

		auto label = lv_label_create(itr.second);
		lv_label_set_text(label, name.c_str());
		lv_obj_set_style_text_color(label, lv_color_make(255, 255, 255), LV_PART_MAIN);
		// lv_obj_center(label);
		lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
	}

	// imgBg = lv_image_create(root);
	// lv_image_set_src(imgBg, &dashboardDsc);
	// lv_obj_align(imgBg, LV_ALIGN_TOP_LEFT, 0, 0);
	//
	// lName = lv_label_create(root);
	// lv_label_set_text(lName, "MoniT4");
	// lv_obj_align(lName, LV_ALIGN_TOP_LEFT, 361, 29);
	//
	// lUptime = lv_label_create(root);
	// lv_label_set_text(lUptime, "");
	// lv_obj_align(lUptime, LV_ALIGN_TOP_LEFT, 361, 66);
	//
	// lIP = lv_label_create(root);
	// lv_label_set_text(lIP, "");
	// lv_obj_align(lIP, LV_ALIGN_TOP_LEFT, 361, 103);
	//
	// barCpu = lv_bar_create(root);
	// lv_bar_set_range(barCpu, 0, 100);
	// lv_bar_set_value(barCpu, 0, LV_ANIM_OFF);
	// lv_obj_set_size(barCpu, 51, 376);
	// lv_obj_align(barCpu, LV_ALIGN_TOP_LEFT, 27, 27);
	// lv_obj_set_style_anim_duration(barCpu, 500, LV_PART_MAIN);
	//
	// barRam = lv_bar_create(root);
	// lv_bar_set_range(barRam, 0, 100);
	// lv_bar_set_value(barRam, 0, LV_ANIM_OFF);
	// lv_obj_set_size(barRam, 51, 376);
	// lv_obj_align(barRam, LV_ALIGN_TOP_LEFT, 92, 27);
	// lv_obj_set_style_anim_duration(barRam, 500, LV_PART_MAIN);
	//
	// barSwap = lv_bar_create(root);
	// lv_bar_set_range(barSwap, 0, 100);
	// lv_bar_set_value(barSwap, 0, LV_ANIM_OFF);
	// lv_obj_set_size(barSwap, 51, 376);
	// lv_obj_align(barSwap, LV_ALIGN_TOP_LEFT, 157, 27);
	// lv_obj_set_style_anim_duration(barSwap, 500, LV_PART_MAIN);
	//
	//
	// static lv_style_t style_indic;
	// lv_style_init(&style_indic);
	// lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
	// lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
	// lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
	// lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_VER);
	//
	// lv_obj_set_style_radius(barCpu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	// lv_obj_set_style_radius(barCpu, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
	//
	// lv_obj_set_style_radius(barRam, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	// lv_obj_set_style_radius(barRam, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
	//
	// lv_obj_set_style_radius(barSwap, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	// lv_obj_set_style_radius(barSwap, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
	//
	// lv_obj_add_style(barCpu, &style_indic, LV_PART_INDICATOR);
	// lv_obj_add_style(barRam, &style_indic, LV_PART_INDICATOR);
	// lv_obj_add_style(barSwap, &style_indic, LV_PART_INDICATOR);

	Display::Unlock();
}

void Dashboard::Init()
{
	InitViews();
	InitSocket();
}
