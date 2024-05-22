/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "led.h"
#include "wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include <stdio.h>
#include <string.h>

#define LOGGER_TAG "wifi_api"

#define WIFI_CONNECTION_RETRY_NB   10
#define WIFI_CONNECTION_DELAY_SEC  30
#define WIFI_PROV_RETRY_NB          3
#define WIFI_PROV_SECURITY          WIFI_PROV_SECURITY_1

#ifndef WIFI_PROV_SSID
#define WIFI_PROV_SSID      "UPnP Remote"
#endif

#ifndef WIFI_PROV_PASS
#define WIFI_PROV_PASS      "P@ssw0rd"
#endif

#ifndef WIFI_PROV_PROOF
#define WIFI_PROV_PROOF     "abcd1234"
#endif

// Add MAC address at the end of the SSID.
static void wifi_ssid_suffix_mac(char *ssid, size_t max_size);
// WiFi event handler.
static void wifi_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
// WiFi provisioning event handler.
static void wifi_provisioning_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
// Check if WiFi is provisioned.
static bool wifi_is_provisioned(void);
// Initialise provisioning manager.
static void wifi_provisioning_init(void);
// Reset provisioning manager.
static void wifi_provisioning_deinit(void);
// Start provisioning process.
static void wifi_provisioning(void);
// Start connection process.
static void wifi_start(void);
// Trigger a connection.
static void wifi_connect(void);

static void wifi_ssid_suffix_mac(char *ssid, size_t max_size)
{
    // Get end position of user SSID.
    size_t end = strlen(ssid);
    // Get default base MAC address (UUID).
    uint8_t mac[8];
    ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
    // Add 3 LSB MAC address to SSID.
    snprintf(&ssid[end], max_size, " %02X%02X%02X", mac[3], mac[4], mac[5]);
}

static void wifi_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void) arg;
    (void) event_data;
    static size_t connection_retry = 0;
    if (event_base != WIFI_EVENT)
        return;
    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(LOGGER_TAG, "WiFi start");
            connection_retry = 0;
            wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(LOGGER_TAG, "WiFi connected");
            connection_retry = 0;
            led_wifi_set(WIFI_CONNECTED);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(LOGGER_TAG, "WiFi disconnected");
            led_wifi_set(WIFI_NOT_CONNECTED);
            // Check for reconnection or provisioning.
            connection_retry++;
            if (connection_retry < WIFI_CONNECTION_RETRY_NB)
            {
                // Wait before retry.
                vTaskDelay(
                    (WIFI_CONNECTION_DELAY_SEC * 1000) / portTICK_PERIOD_MS);
                // Retry connection.
                wifi_connect();
            }
            else
            {
                // Stop connection retry, start provisioning.
                connection_retry = 0;
                wifi_provisioning_init();
                wifi_prov_mgr_reset_sm_state_for_reprovision();
                wifi_provisioning();
            }
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(LOGGER_TAG, "SoftAP connected");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(LOGGER_TAG, "SoftAP disconnected");
            break;
        default:
            // Nothing to do.
            break;
    }
}

static void wifi_ip_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base != IP_EVENT)
        return;
    switch (event_id)
    {
        case IP_EVENT_STA_GOT_IP:
            const ip_event_got_ip_t * const ip =
                (ip_event_got_ip_t*) event_data;
            ESP_LOGI(LOGGER_TAG, "IP: " IPSTR, IP2STR(&ip->ip_info.ip));
            break;
        default:
            // Nothing to do.
            break;
    }
}

static void wifi_provisioning_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void) arg;
    static size_t fail_retry = 0;
    if (event_base != WIFI_PROV_EVENT)
        return;
    switch (event_id)
    {
        case WIFI_PROV_INIT:
            ESP_LOGD(LOGGER_TAG, "Provisioning initialised");
            break;
        case WIFI_PROV_START:
            ESP_LOGI(LOGGER_TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV:
            const wifi_sta_config_t * const wifi_sta_cfg =
                (wifi_sta_config_t *) event_data;
            ESP_LOGI(LOGGER_TAG,
                "Credentials received for ssid='%s'",
                wifi_sta_cfg->ssid);
            break;
        case WIFI_PROV_CRED_FAIL:
            const wifi_prov_sta_fail_reason_t * const reason =
                (wifi_prov_sta_fail_reason_t *) event_data;
            ESP_LOGE(LOGGER_TAG,
                "Provisioning failed reason=%s",
                (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                    "auth_failed" : "ap_not_found");
            // Wait number of retry to reset provisioning state.
            fail_retry++;
            if (fail_retry >= WIFI_PROV_RETRY_NB)
            {
                ESP_LOGI(LOGGER_TAG, "Retry limit reached, reseting provisioning");
                wifi_prov_mgr_reset_sm_state_on_failure();
                fail_retry = 0;
            }
            break;
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(LOGGER_TAG, "Provisioning successful");
            fail_retry = 0;
            break;
        case WIFI_PROV_END:
            ESP_LOGI(LOGGER_TAG, "Provisioning ended");
            wifi_provisioning_deinit();
            break;
        case WIFI_PROV_DEINIT:
            ESP_LOGD(LOGGER_TAG, "Provisioning de-initialised");
            break;
        default:
            // Nothing to do.
            break;
    }
}

static bool wifi_is_provisioned(void)
{
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    return provisioned;
}

static void wifi_provisioning_init(void)
{
    const wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
}

static void wifi_provisioning_deinit(void)
{
    wifi_prov_mgr_deinit();
}

static void wifi_provisioning(void)
{
    char ssid[32] = WIFI_PROV_SSID;
    wifi_ssid_suffix_mac(ssid, sizeof(ssid));
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
        WIFI_PROV_SECURITY, WIFI_PROV_PROOF, ssid, WIFI_PROV_PASS));
}

static void wifi_start(void)
{
    // Start WiFi in station mode.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_connect(void)
{
    esp_wifi_connect();
}

void wifi_init(void)
{
    // Register event handlers.
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &wifi_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_provisioning_event_handler,
        NULL));
    // Initialise WiFi component.
    const wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    wifi_provisioning_init();
    // Check credentials status.
    if (!wifi_is_provisioned())
    {
        ESP_LOGI(LOGGER_TAG, "Credentials not available");
        // No credentials, start provisioning process.
        wifi_provisioning();
    }
    else
    {
        ESP_LOGI(LOGGER_TAG, "Credentials available");
        // Credentials are present, start connection.
        wifi_provisioning_deinit();
        wifi_start();
    }
}
