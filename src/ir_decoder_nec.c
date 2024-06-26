/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "ir_decoder.h"
#include "esp_log.h"
#include <stdio.h>

#define LOGGER_TAG "ir_decoder_nec"

#define NEC_FRAME_NORMAL                  34u
#define NEC_FRAME_REPEAT                   2u
#define NEC_RANGE_MARGIN                 150u
#define NEC_LEADING_CODE_DURATION_0     9000u
#define NEC_LEADING_CODE_DURATION_1     4500u
#define NEC_REPEAT_CODE_DURATION_0      9000u
#define NEC_REPEAT_CODE_DURATION_1      2250u
#define NEC_ZERO_DURATION_0              562u
#define NEC_ZERO_DURATION_1              562u
#define NEC_ONE_DURATION_0               562u
#define NEC_ONE_DURATION_1              1675u

static inline bool nec_check_range(uint32_t value, uint32_t expected)
{
    return ((expected - NEC_RANGE_MARGIN) < value)
        && (value < (expected + NEC_RANGE_MARGIN));
}

static bool nec_check_leading_code(
    const rmt_symbol_word_t * const symbol, bool variant)
{
    assert(symbol);
    if (variant)
        return nec_check_range(symbol->duration0, NEC_LEADING_CODE_DURATION_1)
            && nec_check_range(symbol->duration1, NEC_LEADING_CODE_DURATION_1);
    else
        return nec_check_range(symbol->duration0, NEC_LEADING_CODE_DURATION_0)
            && nec_check_range(symbol->duration1, NEC_LEADING_CODE_DURATION_1);
}

static bool nec_check_repeat_code(
    const rmt_symbol_word_t * const symbol, bool variant)
{
    assert(symbol);
    if (variant)
        return nec_check_range(symbol->duration0, NEC_REPEAT_CODE_DURATION_1)
            && nec_check_range(symbol->duration1, NEC_REPEAT_CODE_DURATION_1);
    else
        return nec_check_range(symbol->duration0, NEC_REPEAT_CODE_DURATION_0)
            && nec_check_range(symbol->duration1, NEC_REPEAT_CODE_DURATION_1);
}

static bool nec_check_zero(const rmt_symbol_word_t * const symbol)
{
    assert(symbol);
    return nec_check_range(symbol->duration0, NEC_ZERO_DURATION_0)
        && nec_check_range(symbol->duration1, NEC_ZERO_DURATION_1);
}

static bool nec_check_one(const rmt_symbol_word_t * const symbol)
{
    assert(symbol);
    return nec_check_range(symbol->duration0, NEC_ONE_DURATION_0)
        && nec_check_range(symbol->duration1, NEC_ONE_DURATION_1);
}

static bool nec_parse_normal(
    const rmt_symbol_word_t *symbols, uint16_t * const address,
    uint8_t * const command, bool variant)
{
    assert(symbols);
    // Normal frame is composed of leading code, address and command.
    // Check if leading code is valid.
    if (!nec_check_leading_code(symbols++, variant))
        return false;
    // Decode address if requested.
    if (address)
    {
        *address = 0u;
        for (uint32_t i = 0; i < 16u; i++)
        {
            if (nec_check_one(symbols))
                *address |= 1u << i;
            else if (nec_check_zero(symbols))
            {
                // Already at zero, nothing to do.
            }
            else
                return false;
            symbols++;
        }
    }
    else
        symbols += 16u;
    // Decode command if requested.
    if (command)
    {
        uint16_t command_raw = 0u;
        for (uint32_t i = 0; i < 16u; i++)
        {
            if (nec_check_one(symbols))
                command_raw |= 1u << i;
            else if (nec_check_zero(symbols))
            {
                // Already at zero, nothing to do.
            }
            else
                return false;
            symbols++;
        }
        // Check inversion format.
        if (((~command_raw & 0xFF00u) >> 8u) != (command_raw & 0xFFu))
            return false;
        *command = command_raw & 0xFFu;
    }
    ESP_LOGD(LOGGER_TAG,
        "Frame decoded address=0x%04x command=0x%02x",
        (address) ? *address : 0,
        (command) ? *command : 0);
    return true;
}

static bool nec_parse_repeat(
    const rmt_symbol_word_t * const symbols, uint16_t * const address,
    uint8_t * const command, bool variant)
{
    assert(symbols);
    // No information on this frame, use only first symbol.
    if (nec_check_repeat_code(&symbols[0], variant))
    {
        if (address)
            *address = 0u;
        if (command)
            *command = 0u;
        return true;
    }
    return false;
}

bool ir_decoder_format_nec(
    const rmt_rx_done_event_data_t * const event, uint16_t * const address,
    uint8_t * const command, bool variant)
{
    const rmt_symbol_word_t * const symbols = event->received_symbols;
    switch (event->num_symbols)
    {
        case NEC_FRAME_NORMAL:
            ESP_LOGD(LOGGER_TAG, "Normal frame");
            return nec_parse_normal(symbols, address, command, variant);
        case NEC_FRAME_REPEAT:
            ESP_LOGD(LOGGER_TAG, "Repeat frame");
            return nec_parse_repeat(symbols, NULL, NULL, variant);
        default:
            ESP_LOGW(LOGGER_TAG, "Frame unsupported");
            return false;
    }
}
