#ifndef __COMMON_H__
#define __COMMON_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1
#define TIME_SYNC_BIT BIT2

extern volatile EventGroupHandle_t services_event_group;

#define WAIT_FOR(flags) while ((xEventGroupWaitBits(services_event_group, flags, pdFALSE, pdTRUE, portMAX_DELAY) & (flags)) != (flags)) {}

#endif
