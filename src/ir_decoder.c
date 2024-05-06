/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "ir_decoder.h"
#include "driver/rmt_rx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>

#define IR_DECODER_TASK_STACK_SIZE       (4u * configMINIMAL_STACK_SIZE)
#define IR_DECODER_TASK_PRIORITY         tskIDLE_PRIORITY
#define IR_DECODER_QUEUE_NB              1u
#define IR_DECODER_RAW_SYMBOLS_NB        64u
#define IR_DECODER_RESOLUTION_HZ         1000000u   // 1us / tick.
#define IR_DECODER_THRESHOLD_MIN_NS      1250u
#define IR_DECODER_THRESHOLD_MAX_NS      12000000u

typedef struct
{
    rmt_channel_handle_t rmt_handle;
    StaticTask_t task;
    StaticQueue_t queue;
    StackType_t task_stack[IR_DECODER_TASK_STACK_SIZE];
    rmt_rx_done_event_data_t queue_buffer[IR_DECODER_QUEUE_NB];
    rmt_symbol_word_t raw_symbols[IR_DECODER_RAW_SYMBOLS_NB];
} ir_decoder_handle_t;

static ir_decoder_handle_t ir_decoder_handle;

// Start RMT reception for specific decoder.
static void ir_decoder_receive(ir_decoder_handle_t * const handle)
{
    assert(handle);
    const rmt_receive_config_t rmt_rx_cfg = {
        .signal_range_min_ns = IR_DECODER_THRESHOLD_MIN_NS,
        .signal_range_max_ns = IR_DECODER_THRESHOLD_MAX_NS
    };
    ESP_ERROR_CHECK(rmt_receive(
        handle->rmt_handle,
        handle->raw_symbols,
        sizeof(handle->raw_symbols),
        &rmt_rx_cfg
    ));
}

// RMT event callback.
static bool ir_decoder_rmt_handler(
    rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *data,
    void *context)
{
    (void) channel;
    BaseType_t task_wakeup = false;
    QueueHandle_t queue = (QueueHandle_t) context;
    // Send data to parsing process.
    xQueueSendFromISR(queue, data, &task_wakeup);
    return task_wakeup;
}

// IR decoder task handler.
static void ir_decoder_task_handler(void *context)
{
    assert(context);
    rmt_rx_done_event_data_t event;
    ir_decoder_handle_t * const handle = (ir_decoder_handle_t *) context;
    // Trigger first reception.
    ir_decoder_receive(handle);
    while (true)
    {
        // Wait event from RMT callback.
        if (pdPASS == xQueueReceive(
                (QueueHandle_t) &handle->queue, &event, pdMS_TO_TICKS(1000)))
        {
            // Send to parsing method.
            uint16_t address;
            uint16_t command;
            if (ir_decoder_format_nec(&event, &address, &command))
            {
                printf("IR decoder [NEC]: address=%04x command=%04x\r\n",
                    address, command);
            }
            else
                printf("IR decoder failed\r\n");
            // Trigger next reception.
            ir_decoder_receive(handle);
        }
    }
}

void ir_decoder_init(uint8_t gpio_num)
{
    memset(&ir_decoder_handle, 0, sizeof(ir_decoder_handle_t));
    const rmt_rx_channel_config_t rmt_cfg = {
        .gpio_num = gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_DECODER_RESOLUTION_HZ,
        .mem_block_symbols = IR_DECODER_RAW_SYMBOLS_NB
    };
    const rmt_rx_event_callbacks_t rmt_cbs = {
        .on_recv_done = ir_decoder_rmt_handler
    };
    // Initialise RX channel.
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rmt_cfg, &ir_decoder_handle.rmt_handle));
    // Initialise RX queue and register handler.
    xQueueCreateStatic(
        IR_DECODER_QUEUE_NB,
        sizeof(rmt_rx_done_event_data_t),
        (uint8_t *) ir_decoder_handle.queue_buffer,
        &ir_decoder_handle.queue
    );
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(
        ir_decoder_handle.rmt_handle,
        &rmt_cbs,
        (QueueHandle_t) &ir_decoder_handle.queue
    ));
    // Enable processing.
    ESP_ERROR_CHECK(rmt_enable(ir_decoder_handle.rmt_handle));
    // Create parsing task.
    xTaskCreateStatic(
        &ir_decoder_task_handler,
        "IR decoder",
        IR_DECODER_TASK_STACK_SIZE,
        &ir_decoder_handle,
        IR_DECODER_TASK_PRIORITY,
        ir_decoder_handle.task_stack,
        &ir_decoder_handle.task
    );
}
