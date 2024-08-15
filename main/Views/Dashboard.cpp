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

LV_IMAGE_DECLARE(dashboardDsc);
#define TAG "dashboard"

lv_obj_t *imgBg;
lv_obj_t *lName;
lv_obj_t *lIP;
lv_obj_t *barCpu;

int serverSock;

void DisplayLocalIP(lv_event_t* e)
{
	char tmp[32];
	esp_netif_ip_info_t ipInfo;
	esp_netif_get_ip_info(esp_netif_get_default_netif(), &ipInfo);

	esp_ip4addr_ntoa(&ipInfo.ip, tmp, 32);
	lv_label_set_text(lIP, tmp);
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
			char buf[32];
			auto bufLen = recv(clientSock, buf, 31, 0); //MSG_WAITALL
			if(bufLen == 0)
			{
				ESP_LOGI(TAG, "Disconnected...");
				break;
			}
			buf[bufLen] = 0;
			ESP_LOGI(TAG, "Recv: %s", buf);
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

	ESP_LOGI(TAG, "Dashboard 3");
	lIP = lv_label_create(root);
	lv_label_set_text(lIP, "---");
	lv_obj_align(lIP, LV_ALIGN_TOP_LEFT, 361, 103);

	ESP_LOGI(TAG, "Dashboard 4");
	barCpu = lv_bar_create(root);
	lv_bar_set_range(barCpu, 0, 100);
	lv_bar_set_value(barCpu, 50, LV_ANIM_ON);
	lv_obj_set_size(barCpu, 51, 376);
	lv_obj_align(barCpu, LV_ALIGN_TOP_LEFT, 27, 27);


	ESP_LOGI(TAG, "Dashboard 5");
	static lv_style_t style_indic;
	lv_style_init(&style_indic);
	lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
	lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
	lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
	lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_VER);

	lv_obj_set_style_radius(barCpu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_radius(barCpu, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

	lv_obj_add_style(barCpu, &style_indic, LV_PART_INDICATOR);


	ESP_LOGI(TAG, "Dashboard 6");
	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_exec_cb(&a, [](void * bar, int32_t temp)
	{
		// ESP_LOGI(TAG, "cb");
		lv_bar_set_value((lv_obj_t*)bar, temp, LV_ANIM_ON);
	});
	lv_anim_set_duration(&a, 3000);
	lv_anim_set_playback_duration(&a, 3000);
	lv_anim_set_var(&a, barCpu);
	lv_anim_set_values(&a, 0, 100);
	lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&a);

	Display::Unlock();
}

void Dashboard::Init()
{
	InitViews();
	InitSocket();
}
