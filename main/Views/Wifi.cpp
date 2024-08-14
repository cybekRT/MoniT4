#include"Wifi.hpp"
#include"Display.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lvgl.h"

static const char *TAG = "main";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_RETRY_BIT     BIT2

static int s_retry_num = 0;

char wifiSsid[32] = "";
char wifiPwd[64] = "";
bool wifiChanged = false;

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < 3)
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_RETRY_BIT);
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		else
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			esp_wifi_stop();
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		auto event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init_sta()
{
	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&event_handler,
														NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&event_handler,
														NULL,
														&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.threshold = { .authmode = WIFI_AUTH_WPA2_PSK },
			.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
			.sae_h2e_identifier = "",
		},
	};

	strcpy((char*)wifi_config.sta.ssid, wifiSsid);
	strcpy((char*)wifi_config.sta.password, wifiPwd);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void WifiTask(void* vLabel)
{
	auto label = (lv_obj_t*)vLabel;

	for(;;)
	{
		ESP_LOGI(TAG, "Wait for status");
		/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
				* number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
		EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
				WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_RETRY_BIT,
				pdFALSE,
				pdFALSE,
				portMAX_DELAY);
		xEventGroupClearBits(s_wifi_event_group, bits);

		ESP_LOGI(TAG, "Wait for lock");
		Display::Lock(-1);
		ESP_LOGI(TAG, "Locked~!");
		/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
		 * happened. */
		if (bits & WIFI_CONNECTED_BIT)
		{
			ESP_LOGI(TAG, "Connected~!");
			lv_label_set_text(label, "Connected~");
			ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifiSsid, wifiPwd);

			if(wifiChanged)
			{
				ESP_LOGI(TAG, "Updating credentials in flash...");

				nvs_handle_t nvsHandle;
				if(nvs_open("wifi", NVS_READWRITE, &nvsHandle) == ESP_OK)
				{
					nvs_set_str(nvsHandle, "ssid", wifiSsid);
					nvs_set_str(nvsHandle, "pwd", wifiPwd);
				}
			}
			else
				ESP_LOGI(TAG, "Credentials are up to date");

			ESP_LOGI(TAG, "Changing mode...");
			Display::SetMode(Display::Mode::Connected);
			ESP_LOGI(TAG, "Changed mode~!");
		}
		else if (bits & WIFI_FAIL_BIT)
		{
			lv_label_set_text(label, "Failed~");
			ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifiSsid, wifiPwd);
		}
		else if (bits & WIFI_RETRY_BIT)
		{
			lv_label_set_text(label, "Retrying...");
			ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifiSsid, wifiPwd);
		}
		else
		{
			lv_label_set_text(label, "ERROR~!");
			ESP_LOGE(TAG, "UNEXPECTED EVENT");
		}

		ESP_LOGI(TAG, "Unlocking");
		Display::Unlock();
		ESP_LOGI(TAG, "Unlocked~!");
	}
}

void Wifi::Init()
{
	s_wifi_event_group = xEventGroupCreate();

	// Init wifi
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// Init lvgl
	ESP_LOGI(TAG, "Display LVGL");
	// Lock the mutex due to the LVGL APIs are not thread-safe
	ESP_LOGI(TAG, "Wait for lock...");
	Display::Lock(-1);
	ESP_LOGI(TAG, "Acquired~!");

	nvs_handle_t nvsHandle;
	if(nvs_open("wifi", NVS_READONLY, &nvsHandle) == ESP_OK)
	{
		size_t wifiSsidLen = sizeof(wifiSsid) - 1;
		nvs_get_str(nvsHandle, "ssid", wifiSsid, &wifiSsidLen);

		size_t wifiPwdLen = sizeof(wifiPwd) - 1;
		nvs_get_str(nvsHandle, "pwd", wifiPwd, &wifiPwdLen);
	}

	auto root = Display::GetRoot(Display::Mode::Disconnected);
	printf("Root: %p (%ldx%ld)\n", root, lv_obj_get_width(root), lv_obj_get_height(root));
	lv_obj_set_style_bg_color(root, lv_color_make(0, 0, 0), 0);

	auto kbd = lv_keyboard_create(root);
	lv_obj_set_width(kbd, Display::GetWidth() - 50);
	lv_obj_align(kbd, LV_ALIGN_BOTTOM_LEFT, 0, 0);

	auto SelectTextArea = [](lv_event_t *e)
	{
		auto target = (lv_obj_t*)lv_event_get_target(e);
		lv_keyboard_set_textarea((lv_obj_t*)e->user_data, target);
	};

	auto CopyTextFromTextArea = [](lv_event_t *e)
	{
		auto target = (lv_obj_t*)lv_event_get_target(e);
		auto dst = (char*)e->user_data;

		wifiChanged = true;
		strcpy(dst, lv_textarea_get_text(target));
	};

	auto ConnectToWifi = [](lv_event_t *e)
	{
		auto label = (lv_obj_t*)e->user_data;
		lv_label_set_text(label, "Connecting...");
		wifi_init_sta();
	};

	auto taSsid = lv_textarea_create(root);
	lv_textarea_set_one_line(taSsid, true);
	lv_textarea_set_max_length(taSsid, sizeof(wifiSsid) - 1);
	lv_textarea_set_text(taSsid, wifiSsid);
	lv_obj_set_width(taSsid, Display::GetWidth() / 2 - 20);
	lv_obj_align(taSsid, LV_ALIGN_TOP_LEFT, 10, 20);
	lv_obj_add_event_cb(taSsid, SelectTextArea, LV_EVENT_FOCUSED, kbd);
	lv_obj_add_event_cb(taSsid, CopyTextFromTextArea, LV_EVENT_VALUE_CHANGED, wifiSsid);

	auto taPwd = lv_textarea_create(root);
	lv_textarea_set_one_line(taPwd, true);
	lv_textarea_set_password_mode(taPwd, true);
	lv_textarea_set_max_length(taPwd, sizeof(wifiPwd) - 1);
	lv_textarea_set_text(taPwd, wifiPwd);
	lv_obj_set_width(taPwd, Display::GetWidth() / 2 - 20);
	lv_obj_align(taPwd, LV_ALIGN_TOP_RIGHT, -10, 20);
	lv_obj_add_event_cb(taPwd, SelectTextArea, LV_EVENT_FOCUSED, kbd);
	lv_obj_add_event_cb(taPwd, CopyTextFromTextArea, LV_EVENT_VALUE_CHANGED, wifiPwd);

	auto bConnect = lv_button_create(root);
	auto lConnect = lv_label_create(bConnect);
	lv_obj_center(lConnect);
	lv_label_set_text(lConnect, "Connect~");
	lv_obj_set_width(bConnect, Display::GetWidth() / 2);
	auto bConnectYOff = lv_obj_get_y(taSsid) + lv_obj_get_height(taSsid) + 10;
	lv_obj_align(bConnect, LV_ALIGN_TOP_MID, 0, bConnectYOff);
	lv_obj_add_event_cb(bConnect, ConnectToWifi, LV_EVENT_CLICKED, lConnect);

	// Release the mutex
	Display::Unlock();

	xTaskCreate(WifiTask, "LVGL Wifi", 4096, lConnect, tskIDLE_PRIORITY, nullptr);
}
