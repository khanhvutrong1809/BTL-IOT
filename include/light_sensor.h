#ifndef __LIGHT_SENSOR__
#define __LIGHT_SENSOR__
#include <Arduino.h>
#include "global.h"
#include "lightSensor.cpp"
// Pin definition for GL5528 light sensor
#define LIGHT_SENSOR_PIN 1  // ADC pin for GL5528

void light_sensor_monitor(void *pvParameters);

#endif
