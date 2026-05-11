#ifndef __PIR_SENSOR_H__
#define __PIR_SENSOR_H__

#include <Arduino.h>
#include "global.h"

// Pin definition for AS312 PIR sensor
#define PIR_SENSOR_PIN 6  // GPIO 6 for AS312 PIR motion sensor

// FreeRTOS task function
void pir_sensor_monitor(void *pvParameters);

#endif
