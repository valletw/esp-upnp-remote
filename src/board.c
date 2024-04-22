/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "board.h"
#include "board_cfg.h"
#include "esp_err.h"
#include "driver/gpio.h"

void board_initialise(void)
{
    // WiFi LED configuration.
    ESP_ERROR_CHECK(gpio_reset_pin(BOARD_IO_LED_WIFI));
    ESP_ERROR_CHECK(gpio_set_direction(BOARD_IO_LED_WIFI, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_IO_LED_WIFI, 0));
    // Bluetooth LED configuration.
    ESP_ERROR_CHECK(gpio_reset_pin(BOARD_IO_LED_BT));
    ESP_ERROR_CHECK(gpio_set_direction(BOARD_IO_LED_BT, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_IO_LED_BT, 0));
    // Software status LED configuration.
    ESP_ERROR_CHECK(gpio_reset_pin(BOARD_IO_LED_SOFT));
    ESP_ERROR_CHECK(gpio_set_direction(BOARD_IO_LED_SOFT, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_IO_LED_SOFT, 0));
    // IR Rx configuration.
    ESP_ERROR_CHECK(gpio_reset_pin(BOARD_IO_IR_RX));
    ESP_ERROR_CHECK(gpio_set_direction(BOARD_IO_IR_RX, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(BOARD_IO_IR_RX, GPIO_PULLUP_ONLY));
    ESP_ERROR_CHECK(gpio_set_intr_type(BOARD_IO_IR_RX, GPIO_INTR_NEGEDGE));
    ESP_ERROR_CHECK(gpio_intr_enable(BOARD_IO_IR_RX));
}
