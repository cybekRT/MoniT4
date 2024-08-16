#include"Dashboard.hpp"

#include <lwip/sockets.h>

#include"Display.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lvgl.h"
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

void DisplayLocalIP(lv_event_t* e)
{
	char tmp[32];
	esp_netif_ip_info_t ipInfo;
	esp_netif_get_ip_info(esp_netif_get_default_netif(), &ipInfo);

	esp_ip4addr_ntoa(&ipInfo.ip, tmp, 32);
	lv_label_set_text(lIP, tmp);
}

bool ParseJson(char* jsonBuffer)
{
	auto root = cJSON_Parse(jsonBuffer);
	if(!root)
		return false;

	Display::Lock(-1);
	cJSON *currentElem;
	cJSON_ArrayForEach(currentElem, root)
	{
		ESP_LOGI(TAG, "Element: %s", currentElem->string);
		if(strcmp(currentElem->string, "name") == 0)
		{
			lv_label_set_text(lName, currentElem->valuestring);
		}
		else if(strcmp(currentElem->string, "uptime") == 0)
		{
			lv_label_set_text_fmt(lUptime, "%d", (int)currentElem->valuedouble);
		}
		else if(strcmp(currentElem->string, "cpu") == 0)
		{
			lv_bar_set_value(barCpu, (int)currentElem->valuedouble, LV_ANIM_ON);
		}
		else if(strcmp(currentElem->string, "ram") == 0)
		{
			lv_bar_set_value(barRam, (int)currentElem->valuedouble, LV_ANIM_ON);
		}
		else if(strcmp(currentElem->string, "swap") == 0)
		{
			lv_bar_set_value(barSwap, (int)currentElem->valuedouble, LV_ANIM_ON);
		}
		else
		{
			ESP_LOGI(TAG, "Invalid tag: %s", currentElem->string);
		}
	}

	Display::Unlock();

	cJSON_Delete(root);
	return true;
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
		lv_label_set_text(lName, "MoniT4");
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
			ESP_LOGI(TAG, "locked...");
			lv_label_set_text(lIP, clientIP);
			ESP_LOGI(TAG, "set...");
			Display::Unlock();
			ESP_LOGI(TAG, "unlocked...");
		}
		else
		{
			ESP_LOGI(TAG, "Couldn't lock for 3s...");
		}

		for(;;)
		{
			// char buf[32];
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

			if(!ParseJson(jsonBuffer))
			{
				ESP_LOGI(TAG, "Invalid json...");
				// break;
				continue;
			}

			jsonBufferLen = 0;
		}

		// ESP_LOGI(TAG, "Sleeping for 2s...");
		// vTaskDelay(pdMS_TO_TICKS(2000));

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
	lv_obj_add_event_cb(root, DisplayLocalIP, LV_EVENT_SCREEN_LOADED, nullptr);

	ESP_LOGI(TAG, "Dashboard 1");
	imgBg = lv_image_create(root);
	lv_image_set_src(imgBg, &dashboardDsc);
	lv_obj_align(imgBg, LV_ALIGN_TOP_LEFT, 0, 0);

	ESP_LOGI(TAG, "Dashboard 2");
	lName = lv_label_create(root);
	lv_label_set_text(lName, "MoniT4");
	lv_obj_align(lName, LV_ALIGN_TOP_LEFT, 361, 29);

	lUptime = lv_label_create(root);
	lv_label_set_text(lUptime, "");
	lv_obj_align(lUptime, LV_ALIGN_TOP_LEFT, 361, 66);

	ESP_LOGI(TAG, "Dashboard 3");
	lIP = lv_label_create(root);
	lv_label_set_text(lIP, "");
	lv_obj_align(lIP, LV_ALIGN_TOP_LEFT, 361, 103);

	ESP_LOGI(TAG, "Dashboard 4");
	barCpu = lv_bar_create(root);
	lv_bar_set_range(barCpu, 0, 100);
	lv_bar_set_value(barCpu, 0, LV_ANIM_OFF);
	lv_obj_set_size(barCpu, 51, 376);
	lv_obj_align(barCpu, LV_ALIGN_TOP_LEFT, 27, 27);

	barRam = lv_bar_create(root);
	lv_bar_set_range(barRam, 0, 100);
	lv_bar_set_value(barRam, 0, LV_ANIM_OFF);
	lv_obj_set_size(barRam, 51, 376);
	lv_obj_align(barRam, LV_ALIGN_TOP_LEFT, 92, 27);

	barSwap = lv_bar_create(root);
	lv_bar_set_range(barSwap, 0, 100);
	lv_bar_set_value(barSwap, 0, LV_ANIM_OFF);
	lv_obj_set_size(barSwap, 51, 376);
	lv_obj_align(barSwap, LV_ALIGN_TOP_LEFT, 157, 27);


	ESP_LOGI(TAG, "Dashboard 5");
	static lv_style_t style_indic;
	lv_style_init(&style_indic);
	lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
	lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
	lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
	lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_VER);

	lv_obj_set_style_radius(barCpu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_radius(barCpu, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

	lv_obj_set_style_radius(barRam, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_radius(barRam, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

	lv_obj_set_style_radius(barSwap, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_radius(barSwap, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

	lv_obj_add_style(barCpu, &style_indic, LV_PART_INDICATOR);
	lv_obj_add_style(barRam, &style_indic, LV_PART_INDICATOR);
	lv_obj_add_style(barSwap, &style_indic, LV_PART_INDICATOR);


	// ESP_LOGI(TAG, "Dashboard 6");
	// lv_anim_t a;
	// lv_anim_init(&a);
	// lv_anim_set_exec_cb(&a, [](void * bar, int32_t temp)
	// {
	// 	// ESP_LOGI(TAG, "cb");
	// 	lv_bar_set_value((lv_obj_t*)bar, temp, LV_ANIM_ON);
	// });
	// lv_anim_set_duration(&a, 3000);
	// lv_anim_set_playback_duration(&a, 3000);
	// lv_anim_set_var(&a, barCpu);
	// lv_anim_set_values(&a, 0, 100);
	// lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
	// lv_anim_start(&a);

	Display::Unlock();
}

void Dashboard::Init()
{
	InitViews();
	InitSocket();
}
