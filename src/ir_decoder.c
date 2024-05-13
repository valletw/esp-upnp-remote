/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "command.h"
#include "ir_decoder.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>

#define LOGGER_TAG "ir_decoder"

#define IR_DECODER_TASK_STACK_SIZE       (4u * configMINIMAL_STACK_SIZE)
#define IR_DECODER_TASK_PRIORITY         tskIDLE_PRIORITY
#define IR_DECODER_QUEUE_NB              1u
#define IR_DECODER_RAW_SYMBOLS_NB        64u
#define IR_DECODER_RESOLUTION_HZ         1000000u   // 1us / tick.
#define IR_DECODER_THRESHOLD_MIN_NS      1250u
#define IR_DECODER_THRESHOLD_MAX_NS      12000000u

// IR decoder parser selector.
typedef enum
{
    IR_DECODER_PARSER_NEC = 0,      // NEC protocol.
} ir_decoder_parser_t;

// IR decoder codeset configuration.
typedef struct
{
    uint8_t parser;
    uint16_t codeset[COMMAND_NB_MAX];
} ir_decoder_codeset_t;

// IR decoder handle.
typedef struct
{
    const ir_decoder_codeset_t *codeset;
    rmt_channel_handle_t rmt_handle;
    StaticTask_t task;
    StaticQueue_t queue;
    StackType_t task_stack[IR_DECODER_TASK_STACK_SIZE];
    rmt_rx_done_event_data_t queue_buffer[IR_DECODER_QUEUE_NB];
    rmt_symbol_word_t raw_symbols[IR_DECODER_RAW_SYMBOLS_NB];
} ir_decoder_handle_t;


static ir_decoder_handle_t ir_decoder_handle;

static const ir_decoder_codeset_t ir_decoder_codeset[] = {
    // Parser              ,  Play/Pause, Previous, Next  , Mute  , Volume+, Volume-
    { IR_DECODER_PARSER_NEC, { 0xF20D   , 0xE31C  , 0xE718, 0xFB04, 0xF30C , 0xEF10 }}
};
static const size_t ir_decoder_codeset_nb =
    sizeof(ir_decoder_codeset) / sizeof(ir_decoder_codeset_t);

// Parse codeset configuration to find command key.
static bool ir_decoder_parse_codeset(
    const ir_decoder_codeset_t * const codeset, uint16_t ir_cmd,
    command_t * const cmd)
{
    assert(codeset);
    assert(cmd);
    for (size_t i = 0; i < COMMAND_NB_MAX; i++)
    {
        if (codeset->codeset[i] == ir_cmd)
        {
            *cmd = (command_t) i;
            return true;
        }
    }
    return false;
}

// Manage NEC protocol.
static void ir_decoder_parser_nec(
    ir_decoder_handle_t * const handle,
    const rmt_rx_done_event_data_t * const event)
{
    uint16_t ir_command;
    if (ir_decoder_format_nec(event, NULL, &ir_command))
    {
        command_t command;
        // Convert command if not a repeat and push it.
        if (ir_command != 0u)
        {
            if (ir_decoder_parse_codeset(
                    handle->codeset, ir_command, &command))
            {
                ESP_LOGD(LOGGER_TAG, "Command found");
                if (!command_push(command))
                    ESP_LOGE(LOGGER_TAG, "Push command failed");
            }
            else
                ESP_LOGW(LOGGER_TAG, "Command unsupported cmd=0x%04x",
                    ir_command);
        }
        else
            ESP_LOGD(LOGGER_TAG, "Command ignored");
    }
    else
        ESP_LOGW(LOGGER_TAG, "NEC formatter failed");
}

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
    assert(handle->codeset);
    // Trigger first reception.
    ir_decoder_receive(handle);
    while (true)
    {
        // Wait event from RMT callback.
        if (pdPASS == xQueueReceive(
                (QueueHandle_t) &handle->queue, &event, pdMS_TO_TICKS(1000)))
        {
            ESP_LOGD(LOGGER_TAG, "IR event detected nb=%d", event.num_symbols);
            for (int i = 0; i < event.num_symbols; i++)
            {
                ESP_LOGV(LOGGER_TAG, "event %3d: {%d, %5d} {%d, %5d}",
                    i,
                    event.received_symbols[i].level0,
                    event.received_symbols[i].duration0,
                    event.received_symbols[i].level1,
                    event.received_symbols[i].duration1);
            }
            // Send to parsing method.
            switch (handle->codeset->parser)
            {
                case IR_DECODER_PARSER_NEC:
                    ir_decoder_parser_nec(handle, &event);
                    break;
                default:
                    ESP_LOGW(LOGGER_TAG, "IR parser unsupported");
                    break;
            }
            // Trigger next reception.
            ir_decoder_receive(handle);
        }
    }
}

void ir_decoder_init(uint8_t gpio_num, uint8_t codeset)
{
    assert(codeset < ir_decoder_codeset_nb);
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
    // Register codeset.
    ESP_LOGI(LOGGER_TAG, "codeset=%d", codeset);
    ir_decoder_handle.codeset = &ir_decoder_codeset[codeset];
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
