/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "command.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>

#define COMMAND_TASK_STACK_SIZE     (2u * configMINIMAL_STACK_SIZE)
#define COMMAND_TASK_PRIORITY       tskIDLE_PRIORITY
#define COMMAND_QUEUE_NB            16u

typedef struct
{
    StaticTask_t task;
    StaticQueue_t queue;
    StackType_t task_stack[COMMAND_TASK_STACK_SIZE];
    command_t queue_buffer[COMMAND_QUEUE_NB];
} command_handle_t;

static command_handle_t command_handle;

static char* command_debug_str[] = {
    [COMMAND_PLAY_PAUSE]  = "Play/Pause",
    [COMMAND_PREVIOUS]    = "Previous",
    [COMMAND_NEXT]        = "Next",
    [COMMAND_MUTE]        = "Mute",
    [COMMAND_VOLUME_UP]   = "Volume Up",
    [COMMAND_VOLUME_DOWN] = "Volume Down",
};

// Get last command from queue.
// Return true on success, false on error.
static bool command_pop(command_handle_t * const handle, command_t * const cmd)
{
    assert(handle);
    assert(cmd);
    return pdPASS == xQueueReceive(
        (QueueHandle_t) &handle->queue, cmd, pdMS_TO_TICKS(1000));
}

// Command task handler.
static void command_task_handler(void *context)
{
    assert(context);
    command_handle_t * const handle = (command_handle_t *) context;
    while (true)
    {
        command_t command;
        // Wait command from receiver process.
        if (command_pop(handle, &command))
        {
            // Check if received command is in range.
            if (command >= COMMAND_NB_MAX)
                continue;
            printf("Command received cmd='%s'\r\n", command_debug_str[command]);
        }
    }
}

void command_init(void)
{
    memset(&command_handle, 0, sizeof(command_handle_t));
    // Initialise command queue.
    xQueueCreateStatic(
        COMMAND_QUEUE_NB,
        sizeof(command_t),
        (uint8_t *) command_handle.queue_buffer,
        &command_handle.queue
    );
    // Create processing task.
    xTaskCreateStatic(
        &command_task_handler,
        "Command",
        COMMAND_TASK_STACK_SIZE,
        &command_handle,
        COMMAND_TASK_PRIORITY,
        command_handle.task_stack,
        &command_handle.task
    );
}

bool command_push(command_t cmd)
{
    assert(cmd < COMMAND_NB_MAX);
    return pdPASS == xQueueSend(
        (QueueHandle_t) &command_handle.queue, &cmd, pdMS_TO_TICKS(1000));
}
