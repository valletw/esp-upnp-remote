/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "board.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include <stdio.h>
#include <stdint.h>

void app_main(void)
{
    board_initialise();
    printf("*** ESP UPnP remote ***\r\n");
    // Get chip information.
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);
    printf(
        "Chip: %s [%d] (%d core(s)) rev %d.%d\r\n",
        CONFIG_IDF_TARGET, chip_info.model, chip_info.cores,
        chip_info.revision / 100, chip_info.revision % 100);
    printf(
        "Flash: %lu MB (%s)\r\n",
        flash_size / (1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    printf(
        "PSRAM: %s\r\n",
        (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "Embedded" : "None");
    printf(
        "Features:%s%s%s%s\r\n",
        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? " WiFi" : "",
        (chip_info.features & CHIP_FEATURE_BT) ? " BT" : "",
        (chip_info.features & CHIP_FEATURE_BLE) ? " BLE" : "",
        (chip_info.features & CHIP_FEATURE_IEEE802154) ? " IEEE-802.15.4" : "");
    // Process.
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf(".");
    }
}
