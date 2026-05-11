#ifndef __NEO_BLINKY__
#define __NEO_BLINKY__
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define NEO_PIN 45
#define LED_COUNT 1

// Semaphore để báo hiệu cập nhật độ ẩm
extern SemaphoreHandle_t xSemaphoreHumidityUpdate;

// Global state variables for scene capture
extern bool neoBlinkState;
extern uint8_t neoR, neoG, neoB, neoBrightness;

void neo_blinky(void *pvParameters);

// API an toàn cho các module khác để điều khiển NeoPixel mà không truy cập strip trực tiếp.
void neo_set_on(bool on);
void neo_set_color(uint8_t r, uint8_t g, uint8_t b);
void neo_set_brightness(uint8_t brightness);

// New functions for scene management
void setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b);
void setNeoPixelBrightness(uint8_t brightness);
void turnNeoPixelOff();
void turnNeoPixelOn();

#endif