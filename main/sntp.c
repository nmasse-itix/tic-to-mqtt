#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sntp.h"
#include "sntp.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_sntp_event_group;

/* The event group allows multiple bits for each event, but we only care about one event:
 * - time is synchronized
 */
#define TIME_SYNC_BIT BIT0

static const char *SNTP_LOGGER = "sntp";

void sntp_callback(struct timeval *tv) {
    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_IMMED) {
        time_t now;
        time(&now);
        struct tm now_tm;
        localtime_r(&now, &now_tm);
        char strftime_buf[64];
        if (strftime(strftime_buf, sizeof(strftime_buf), "%c", &now_tm)) {
            ESP_LOGI(SNTP_LOGGER, "Time synchronized: %s", strftime_buf);
        }
        sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
        xEventGroupSetBits(s_sntp_event_group, TIME_SYNC_BIT);
    }
}

void sntp_start() {
    s_sntp_event_group = xEventGroupCreate();
    ESP_LOGI(SNTP_LOGGER, "Initializing SNTP...");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_callback);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_init();
}

void sntp_wait_for_readiness(void) {
    /* Waiting until the connection is established (MQTT_CONNECTED_BIT).
     * The bits are set by mqtt_event_handler_cb() (see above)
     */
    EventBits_t bits = xEventGroupWaitBits(s_sntp_event_group,
            TIME_SYNC_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & TIME_SYNC_BIT) {
        ESP_LOGI(SNTP_LOGGER, "Time is synchronized!");
    } else {
        ESP_LOGE(SNTP_LOGGER, "UNEXPECTED EVENT");
    }

    //vEventGroupDelete(s_sntp_event_group);
}