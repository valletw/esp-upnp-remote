/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#ifndef IR_DECODER_H_
#define IR_DECODER_H_

#include "driver/rmt_types.h"
#include <stdint.h>

// Initialise IR decoder (RMT driver and parsing task).
extern void ir_decoder_init(uint8_t gpio_num);
// Event parser for NEC protocol.
// Return true if parsing was successful, else false.
extern bool ir_decoder_format_nec(
    const rmt_rx_done_event_data_t * const event, uint16_t * const address,
    uint16_t * const command);

#endif  // IR_DECODER_H_
