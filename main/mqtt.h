#ifndef __MQTT_H__
#define __MQTT_H__

void mqtt_init();
void mqtt_wait_for_readiness(void);
void mqtt_publish_data(char* key, char* value);
void mqtt_publish_alert(uint8_t value);

#endif
