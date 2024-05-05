/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#ifndef IR_DECODER_H_
#define IR_DECODER_H_

#include <stdint.h>

// Initialise IR decoder (RMT driver and parsing task).
extern void ir_decoder_init(uint8_t gpio_num);

#endif  // IR_DECODER_H_
