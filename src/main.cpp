#include "global.h"
#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "mainserver.h"
#include "light_sensor.h"
#include "pir_sensor.h"

void setup() {
  Serial.begin(115200);

  xQueueTemperature = xQueueCreate(1, sizeof(float));
  xQueueHumidity = xQueueCreate(1, sizeof(float));
  xQueueLight = xQueueCreate(5, sizeof(float));
  xQueuePIR = xQueueCreate(5, sizeof(bool));

  xSemaphoreStateNormal = xSemaphoreCreateBinary();
  xSemaphoreStateWarning = xSemaphoreCreateBinary();
  xSemaphoreStateCritical = xSemaphoreCreateBinary();

  xSemaphoreGive(xSemaphoreStateNormal);

  Serial.println("[BOOT] Queues and semaphores initialized");

  xTaskCreate(led_blinky, "Task LED Blink", 2048, NULL, 2, NULL);
  xTaskCreate(neo_blinky, "Task NEO Blink", 2048, NULL, 2, NULL);
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 4096, NULL, 2, NULL);
  xTaskCreate(light_sensor_monitor, "Task GL5528 Monitor", 4096, NULL, 2, NULL);
  // xTaskCreate(pir_sensor_monitor, "Task PIR AS312 Monitor", 2048, NULL, 2, NULL);
  xTaskCreate(main_server_task, "Task Main Server", 32768, NULL, 2, NULL);
}

void loop() {
}
