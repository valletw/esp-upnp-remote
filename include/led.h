/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#ifndef LED_H_
#define LED_H_

// WiFi LED state.
typedef enum
{
    WIFI_NOT_CONNECTED = 0,
    WIFI_CONNECTED
} led_wifi_t;

// Bluetooth LED state.
typedef enum
{
    BT_NOT_CONNECTED = 0,
    BT_CONNECTED
} led_bt_t;

// Software LED state.
typedef enum
{
    SOFT_OFF = 0,
    SOFT_ON
} led_soft_t;

// Initialise all LEDs.
extern void led_init(void);
// Control WiFi LED state.
extern void led_wifi_set(led_wifi_t value);
// Control bluetooth LED state.
extern void led_bt_set(led_bt_t value);
// Control software LED state.
extern void led_soft_set(led_soft_t value);

#endif  // LED_H_
