#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "tic.h"
#include "wifi.h"
#include "mqtt.h"

// Embedded via component.mk
extern const uint8_t cacert_pem_start[]   asm("_binary_cacert_pem_start");
extern const uint8_t cacert_pem_end[]   asm("_binary_cacert_pem_end");

void app_main(void) {
    // NVS is used to store wifi credentials. So, we need to initialize it first.
    ESP_ERROR_CHECK(nvs_flash_init());

    // Inject the Let's Encrypt Root CA certificate in the global CA store
    ESP_ERROR_CHECK(esp_tls_init_global_ca_store());
    ESP_ERROR_CHECK(esp_tls_set_global_ca_store((const unsigned char *) cacert_pem_start, cacert_pem_end - cacert_pem_start));

    wifi_init_sta();
    wifi_wait_for_online();
    mqtt_init();
    mqtt_wait_for_readiness();
    tic_uart_init();
}
