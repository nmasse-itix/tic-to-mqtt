#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
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

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_mqtt_event_group;
static esp_mqtt_client_config_t mqtt_cfg;
static esp_mqtt_client_handle_t client;

/* The event group allows multiple bits for each event, but we only care about one event:
 * - we are connected to the MQTT server
 */
#define MQTT_CONNECTED_BIT BIT0

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
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_LOGGER, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
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
    s_mqtt_event_group = xEventGroupCreate();

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

void mqtt_wait_for_readiness(void) {
    /* Waiting until the connection is established (MQTT_CONNECTED_BIT).
     * The bits are set by mqtt_event_handler_cb() (see above)
     */
    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
            MQTT_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & MQTT_CONNECTED_BIT) {
        ESP_LOGI(MQTT_LOGGER, "Connected to the MQTT server!");
    } else {
        ESP_LOGE(MQTT_LOGGER, "UNEXPECTED EVENT");
    }

    //vEventGroupDelete(s_mqtt_event_group);
}
