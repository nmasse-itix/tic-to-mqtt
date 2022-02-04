#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "tic.h"
#include "wifi.h"

void app_main(void) {
    // NVS is used to store wifi credentials. So, we need to initialize it first.
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();
    wifi_wait_for_online();
    tic_uart_init();
}
