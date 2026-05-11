#ifndef __LED_BLINKY__
#define __LED_BLINKY__
#include <Arduino.h>
#include "global.h"
#define LED_GPIO 48
#define LED_PIN 48

// Semaphore để báo hiệu cập nhật nhiệt độ
extern SemaphoreHandle_t xSemaphoreTempUpdate;

void led_blinky(void *pvParameters);
void turnLedOn();
void turnLedOff();

#endif