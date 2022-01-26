#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "tic.h"

static const char *TIC_LOGGER = "tic";
static QueueHandle_t uart_queue;

uint8_t tic_checksum(uint8_t* buffer, size_t start, size_t end) {
    uint16_t checksum = 0;
    for (size_t i = start; i <= end; i++) {
        checksum += buffer[i];
    }
    checksum = (checksum & 0x3F) + 0x20;
    return (uint8_t)checksum;
}

static void tic_uart_events(void *pvParameters) {
    uart_event_t event;
    size_t buffered_size;
    uint8_t* buffer = (uint8_t*) malloc(TIC_READ_BUFFER_SIZE);
    uint8_t methods[2] = {0, 0};
    for(;;) {
        // Waiting for UART event.
        if(xQueueReceive(uart_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            switch(event.type) {
                case UART_FIFO_OVF:
                case UART_BUFFER_FULL:
                case UART_PARITY_ERR:
                case UART_FRAME_ERR:
                    ESP_LOGI(TIC_LOGGER, "error: event type %d. Discarding existing data...", event.type);
                    uart_flush_input(TIC_UART_NUM);
                    xQueueReset(uart_queue);
                    break;
                case UART_PATTERN_DET:
                    bzero(buffer, TIC_READ_BUFFER_SIZE);
                    uart_get_buffered_data_len(TIC_UART_NUM, &buffered_size);
                    int pos = uart_pattern_pop_pos(TIC_UART_NUM);
                    if (pos == -1) {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(TIC_UART_NUM);
                    } else {
                        uint8_t* scratch = buffer;
                        uart_read_bytes(TIC_UART_NUM, scratch, pos + 1, 100 / portTICK_PERIOD_MS);
                        scratch[pos] = '\0';
                        ESP_LOGD(TIC_LOGGER, "read data: '%s', separator at %d", scratch, pos);

                        // '\n' + tag + sep + value + sep + checksum = at least 6 characters
                        if (pos < 6) {
                            ESP_LOGI(TIC_LOGGER, "read string is too short: %d", pos);
                            break;
                        }

                        // during manual tests, there is no '\n' at the start of the string
                        // but according to Enedis, there is one in the actual data sent.
                        if (scratch[0] == '\n') {
                            scratch++;
                            pos--;
                        }

                        if (methods[0] >= TIC_CHECKSUM_THRESHOLD || methods[1] >= TIC_CHECKSUM_THRESHOLD) {
                            if (methods[0] >= TIC_CHECKSUM_THRESHOLD && tic_checksum(scratch, 0, pos - 3) == scratch[pos - 1]) {
                                ESP_LOGD(TIC_LOGGER, "validated with method1: %s", scratch);
                            } else if (methods[1] >= TIC_CHECKSUM_THRESHOLD && tic_checksum(scratch, 0, pos - 2) == scratch[pos - 1]) {
                                ESP_LOGD(TIC_LOGGER, "validated with method2: %s", scratch);
                            } else {
                                ESP_LOGI(TIC_LOGGER, "wrong checksum: %s", scratch);
                                break;
                            }
                        } else {
                            if (tic_checksum(scratch, 0, pos - 3) == scratch[pos - 1]) {
                                methods[0]++;
                                ESP_LOGD(TIC_LOGGER, "validated with method 1 while learning: %s", scratch);
                            } else if (tic_checksum(scratch, 0, pos - 2) == scratch[pos - 1]) {
                                methods[1]++;
                                ESP_LOGD(TIC_LOGGER, "validated with method 2 while learning: %s", scratch);
                            } else {
                                ESP_LOGI(TIC_LOGGER, "wrong checksum: %s", scratch);
                                break;
                            }
                        }

                        // trim the checksum
                        scratch[pos - 2] = '\0';
                        ESP_LOGD(TIC_LOGGER, "validated: '%s'", scratch);
                    }
                    break;
                //Others
                default:
                    break;
            }
        }
    }
    free(buffer);
    buffer = NULL;
    vTaskDelete(NULL);
}

void tic_uart_init() {
    // Eventually, the log level can be reduced here
    //ESP_ERROR_CHECK(esp_log_level_set(TIC_LOGGER, ESP_LOG_INFO));

#if CONFIG_TIC_MODE == 0
    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_7_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
#endif
#if CONFIG_TIC_MODE == 1
    uart_config_t uart_config = {
        .baud_rate = 9600,
        // TODO: validate other parameters
        .data_bits = UART_DATA_7_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
#endif
    // Setup the UART
    ESP_ERROR_CHECK(uart_driver_install(TIC_UART_NUM, TIC_BUFFER_SIZE, TIC_BUFFER_SIZE, 20, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(TIC_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(TIC_UART_NUM, UART_PIN_NO_CHANGE, CONFIG_TIC_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(TIC_UART_NUM, '\r', 1, 9, 0, 0));
    ESP_ERROR_CHECK(uart_pattern_queue_reset(TIC_UART_NUM, 20));

    // Create a task to handler UART event from ISR
    BaseType_t xReturned;
    xReturned = xTaskCreate(tic_uart_events,
                            "tic_uart_events",
                            CONFIG_TIC_UART_STACK_SIZE,  /* Stack size in words, not bytes. */
                            NULL,                        /* Parameter passed into the task. */
                            tskIDLE_PRIORITY + 12,
                            NULL);
    if (xReturned != pdPASS) {
        ESP_LOGE(TIC_LOGGER, "xTaskCreate('tic_uart_events'): %d", xReturned);
        abort();
    }
}