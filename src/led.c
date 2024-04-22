/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "board_cfg.h"
#include "led.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include <stdint.h>

#define LED_SPEED           LEDC_LOW_SPEED_MODE
#define LED_TIMER           LEDC_TIMER_0
#define LED_DUTY_RES        LEDC_TIMER_13_BIT
#define LED_DUTY_RES_VAL    13u

static const ledc_timer_config_t led_timer_config[] = {
    {
        .speed_mode = LED_SPEED,
        .duty_resolution = LED_DUTY_RES,
        .timer_num = LED_TIMER,
        .freq_hz = BOARD_LED_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    }
};
static const size_t led_timer_config_nb =
    sizeof(led_timer_config) / sizeof(ledc_timer_config_t);

static const ledc_channel_config_t led_channel_config[] = {
    {
        .gpio_num = BOARD_IO_LED_WIFI,
        .speed_mode = LED_SPEED,
        .channel = BOARD_LED_CHANNEL_WIFI,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LED_TIMER,
        .duty = 0,
        .hpoint = 0
    },
    {
        .gpio_num = BOARD_IO_LED_BT,
        .speed_mode = LED_SPEED,
        .channel = BOARD_LED_CHANNEL_BT,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LED_TIMER,
        .duty = 0,
        .hpoint = 0
    },
    {
        .gpio_num = BOARD_IO_LED_SOFT,
        .speed_mode = LED_SPEED,
        .channel = BOARD_LED_CHANNEL_SOFT,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LED_TIMER,
        .duty = 0,
        .hpoint = 0
    }
};
static const size_t led_channel_config_nb =
    sizeof(led_channel_config) / sizeof(ledc_channel_config_t);

// Compute duty cycle value from percent.
static inline uint32_t led_duty_format(uint32_t percent)
{
    return (((1u << LED_DUTY_RES_VAL) - 1u) * 100u) / percent;
}

void led_init(void)
{
    // Timer configuration.
    for (size_t i = 0; i < led_timer_config_nb; i++)
        ESP_ERROR_CHECK(ledc_timer_config(&led_timer_config[i]));
    // Channels configuration.
    for (size_t i = 0; i < led_channel_config_nb; i++)
        ESP_ERROR_CHECK(ledc_channel_config(&led_channel_config[i]));
    // Initialise fade service.
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
}

void led_wifi_set(led_wifi_t value)
{
    switch (value)
    {
        case WIFI_NOT_CONNECTED:
            ledc_set_duty(LED_SPEED, BOARD_LED_CHANNEL_WIFI, 0);
            ledc_update_duty(LED_SPEED, BOARD_LED_CHANNEL_WIFI);
            break;
        case WIFI_CONNECTED:
            ledc_set_duty(
                LED_SPEED, BOARD_LED_CHANNEL_WIFI,
                led_duty_format(BOARD_LED_DUTY_CYCLE));
            ledc_update_duty(LED_SPEED, BOARD_LED_CHANNEL_WIFI);
            break;
        default:
            // Nothing to do.
            break;
    }
}

void led_bt_set(led_bt_t value)
{
    switch (value)
    {
        case BT_NOT_CONNECTED:
            ledc_set_duty(LED_SPEED, BOARD_LED_CHANNEL_BT, 0);
            ledc_update_duty(LED_SPEED, BOARD_LED_CHANNEL_BT);
            break;
        case BT_CONNECTED:
            ledc_set_duty(
                LED_SPEED, BOARD_LED_CHANNEL_BT,
                led_duty_format(BOARD_LED_DUTY_CYCLE));
            ledc_update_duty(LED_SPEED, BOARD_LED_CHANNEL_BT);
            break;
        default:
            // Nothing to do.
            break;
    }
}

void led_soft_set(led_soft_t value)
{
    switch (value)
    {
        case SOFT_OFF:
            ledc_set_duty(LED_SPEED, BOARD_LED_CHANNEL_SOFT, 0);
            ledc_update_duty(LED_SPEED, BOARD_LED_CHANNEL_SOFT);
            break;
        case SOFT_ON:
            ledc_set_duty(
                LED_SPEED, BOARD_LED_CHANNEL_SOFT,
                led_duty_format(BOARD_LED_DUTY_CYCLE));
            ledc_update_duty(LED_SPEED, BOARD_LED_CHANNEL_SOFT);
            break;
        default:
            // Nothing to do.
            break;
    }
}
