#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "tic.h"
#include "libteleinfo.h"
#include "mqtt.h"

static const char *TIC_LOGGER = "tic";

void tic_data_callback(time_t ts, uint8_t flags, char * name, char * value) {
    ESP_LOGD(TIC_LOGGER, "%s=%s", name, value);
    mqtt_publish_data(name, value);
}

static void tic_uart_read(void *pvParameters) {
    uint8_t* buffer = (uint8_t*) malloc(CONFIG_TIC_UART_READ_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TIC_LOGGER, "malloc(CONFIG_TIC_UART_READ_BUFFER_SIZE) failed!");
        return;
    }

    for (;;) {
        int len = uart_read_bytes(CONFIG_TIC_UART_PORT_NUM, buffer, CONFIG_TIC_UART_READ_BUFFER_SIZE, 20 / portTICK_RATE_MS);
        if (len == -1) {
            ESP_LOGE(TIC_LOGGER, "uart_read_bytes failed!");
            break;
        }
        if (len == 0) {
            continue;
        }
        ESP_LOGD(TIC_LOGGER, "uart_read_bytes read %d bytes", len);

        libteleinfo_process(buffer, len);
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
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_TIC_UART_PORT_NUM, CONFIG_TIC_UART_BUFFER_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_TIC_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CONFIG_TIC_UART_PORT_NUM, UART_PIN_NO_CHANGE, CONFIG_TIC_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    libteleinfo_init(tic_data_callback, NULL);

    // Create a task to handler UART event from ISR
    BaseType_t xReturned;
    xReturned = xTaskCreate(tic_uart_read,
                            "tic_uart_read",
                            CONFIG_TIC_UART_STACK_SIZE,  /* Stack size in words, not bytes. */
                            NULL,                        /* Parameter passed into the task. */
                            tskIDLE_PRIORITY + 12,
                            NULL);
    if (xReturned != pdPASS) {
        ESP_LOGE(TIC_LOGGER, "xTaskCreate('tic_uart_read'): %d", xReturned);
        abort();
    }
}