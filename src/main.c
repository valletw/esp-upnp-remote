/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "board.h"
#include "board_cfg.h"
#include "command.h"
#include "ir_decoder.h"
#include "led.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>

#define LOGGER_TAG      "main"

static void display_chip_information(void)
{
    // Get chip information.
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(LOGGER_TAG,
        "Chip: %s [%d] (%d core(s)) rev %d.%d",
        CONFIG_IDF_TARGET, chip_info.model, chip_info.cores,
        chip_info.revision / 100, chip_info.revision % 100);
    ESP_LOGI(LOGGER_TAG,
        "Flash: %lu MB (%s)",
        flash_size / (1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI(LOGGER_TAG,
        "PSRAM: %s",
        (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "Embedded" : "None");
    ESP_LOGI(LOGGER_TAG,
        "Features:%s%s%s%s",
        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? " WiFi" : "",
        (chip_info.features & CHIP_FEATURE_BT) ? " BT" : "",
        (chip_info.features & CHIP_FEATURE_BLE) ? " BLE" : "",
        (chip_info.features & CHIP_FEATURE_IEEE802154) ? " IEEE-802.15.4" : "");
}

void app_main(void)
{
    board_initialise();
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(LOGGER_TAG, "*** ESP UPnP remote ***");
    display_chip_information();
    // Initialise command processing.
    command_init();
    // IR decoder configuration.
    ir_decoder_init(BOARD_IO_IR_RX, IR_CODESET_CFG);
    // Process.
    led_soft_t led_soft = SOFT_ON;
    while (1)
    {
        led_soft_set(led_soft);
        led_soft = (led_soft == SOFT_ON) ? SOFT_OFF : SOFT_ON;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
