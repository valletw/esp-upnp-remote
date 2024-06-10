/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "ssdp.h"
#include "udp_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LOGGER_TAG  "ssdp"

#define SSDP_IP             "239.255.255.250"
#define SSDP_PORT           1900
#define SSDP_USER_AGENT_F   "FreeRTOS/%d.%d.%d UPnP/1.1 ESP-UPnP-Remote/1.0"
#define SSDP_USER_AGENT_D   tskKERNEL_VERSION_MAJOR, tskKERNEL_VERSION_MINOR, \
                            tskKERNEL_VERSION_BUILD

#define SSDP_MSEARCH_ALL        "ssdp:all"
#define SSDP_MSEARCH_ROOT       "upnp:rootdevice"
#define SSDP_MSEARCH_DEVICE     "urn:schemas-upnp-org:device:"
#define SSDP_MSEARCH_SERVICE    "urn:schemas-upnp-org:service:"
#define SSDP_MSEARCH_RENDERER   (SSDP_MSEARCH_SERVICE "RenderingControl:2")

#define SSDP_MSEARCH_DEFAULT_TIME   3

#define SSDP_MSEARCH_RESPONSE   "HTTP/1.1 200 OK\r\n"

// Prepare M-SEARCH request with target and response time.
// Return request size on success, -1 on error.
static int ssdp_msearch_prepare(
    char * const request, size_t size, const char * const target, int time_s);
// Parse M-SEARCH response and verify content.
static void ssdp_msearch_response_check(const char * const response, size_t size);

static int ssdp_msearch_prepare(
    char * const request, size_t size, const char * const target, int time_s)
{
    int count = snprintf(
        request, size,
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: %s:%d\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "ST: %s\r\n"
        "MX: %d\r\n"
        "USER-AGENT: " SSDP_USER_AGENT_F "\r\n\r\n",
        SSDP_IP, SSDP_PORT, target, time_s, SSDP_USER_AGENT_D);
    return (count <= size) ? count : -1;
}

static void ssdp_msearch_response_check(const char * const response, size_t size)
{
    if (response == NULL || size == 0)
        return;
    if (0 == strncmp(
            response, SSDP_MSEARCH_RESPONSE,
            sizeof(SSDP_MSEARCH_RESPONSE) - 1u))
    {
        ESP_LOGI(LOGGER_TAG, "Valid search response found");
        ESP_LOGD(LOGGER_TAG, "response=%s", response);
    }
}

void ssdp_discovery_dump(void)
{
    char msearch_request[256];
    char msearch_response[512];
    // Prepare request.
    int msearch_request_count = ssdp_msearch_prepare(
            msearch_request, sizeof(msearch_request), SSDP_MSEARCH_RENDERER,
            SSDP_MSEARCH_DEFAULT_TIME);
    if (msearch_request_count < 0)
    {
        ESP_LOGE(LOGGER_TAG, "Request preparation failed");
        return;
    }
    // Open UDP connection.
    uintptr_t udp = udp_client_open(SSDP_IP, SSDP_PORT, true);
    if (udp == 0)
    {
        ESP_LOGE(LOGGER_TAG, "UDP initialisation failed");
        return;
    }
    struct timeval udp_timeout = { 2, 0 };
    udp_client_set_timeout(udp, &udp_timeout);
    // Send SSDP search request.
    ESP_LOGI(LOGGER_TAG, "Send search request");
    if (0 > udp_client_write(
            udp, (uint8_t *) msearch_request, msearch_request_count))
    {
        ESP_LOGE(LOGGER_TAG, "UDP write failed");
        // Close UDP connection.
        udp_client_close(udp);
        return;
    }
    // Wait for response.
    ESP_LOGI(LOGGER_TAG, "Waiting search response");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    while (0 < udp_client_poll(udp))
    {
        // Get response.
        int count = udp_client_read(
            udp, (uint8_t *) msearch_response, sizeof(msearch_response), NULL);
        if (count <= 0)
        {
            ESP_LOGE(LOGGER_TAG, "UDP read failed");
            break;
        }
        // Parse response.
        ssdp_msearch_response_check(msearch_response, count);
    }
    ESP_LOGI(LOGGER_TAG, "All search responses received");
    // Close UDP connection.
    udp_client_close(udp);
}
