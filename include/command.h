/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#ifndef COMMAND_H_
#define COMMAND_H_

#include <stdbool.h>

// Control commands supported.
typedef enum
{
    COMMAND_PLAY_PAUSE = 0,
    COMMAND_PREVIOUS,
    COMMAND_NEXT,
    COMMAND_MUTE,
    COMMAND_VOLUME_UP,
    COMMAND_VOLUME_DOWN,
    COMMAND_NB_MAX
} command_t;

// Initialise command process.
extern void command_init(void);
// Push command for processing task.
// Return true on success, false on error.
extern bool command_push(command_t cmd);

#endif  // COMMAND_H_
