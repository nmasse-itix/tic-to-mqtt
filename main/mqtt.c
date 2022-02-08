#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "common.h"

static esp_mqtt_client_config_t mqtt_cfg;
static esp_mqtt_client_handle_t client;

static const char *MQTT_LOGGER = "mqtt";

void mqtt_publish_data(char* key, char* value) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "home/power/tic/%s", key);
    int qos = 0;
    int retain = 0;
    if (strcmp(key, "BASE") == 0 || strcmp(key, "HCHP") == 0 ||
        strcmp(key, "HCHC") == 0 || strcmp(key, "PTEC") == 0) {
        qos = 1;
        retain = 1;
    }
    int ret = esp_mqtt_client_publish(client, buffer, value, 0, qos, retain);
    if (ret == -1) {
        ESP_LOGD(MQTT_LOGGER, "MQTT Message discarded!");
    }
}

void mqtt_publish_alert(uint8_t value) {
    char payload[2] = {'0' + value, 0};
    int ret = esp_mqtt_client_publish(client, "home/power/tic/ADPS", payload, 0, 1, 0);
    if (ret == -1) {
        ESP_LOGD(MQTT_LOGGER, "MQTT Message discarded!");
    }
}

esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_LOGGER, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(services_event_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_LOGGER, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(services_event_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(MQTT_LOGGER, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(MQTT_LOGGER, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGI(MQTT_LOGGER, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(MQTT_LOGGER, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGI(MQTT_LOGGER, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                                                                strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(MQTT_LOGGER, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(MQTT_LOGGER, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGD(MQTT_LOGGER, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(MQTT_LOGGER, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

char* get_nvs_string(nvs_handle_t nvs, char* key) {
    size_t required_size;
    esp_err_t err = nvs_get_str(nvs, key, NULL, &required_size);
    if (err != ESP_OK) {
        return NULL;
    }
    char* value = malloc(required_size);
    if (!value) {
        return NULL;
    }
    err = nvs_get_str(nvs, key, value, &required_size);
    if (err != ESP_OK) {
        free(value);
        return NULL;
    }
    return value;
}

void mqtt_init(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("mqtt", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    }

    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    mqtt_cfg.uri = get_nvs_string(nvs, "url");
    mqtt_cfg.use_global_ca_store = true;
    mqtt_cfg.username = get_nvs_string(nvs, "username");
    mqtt_cfg.password = get_nvs_string(nvs, "password");

    nvs_close(nvs);

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}
