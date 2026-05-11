#include "global.h"
#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "mainserver.h"
#include "light_sensor.h"
#include "pir_sensor.h"






void setup() {
  Serial.begin(115200);

  // === NHIỆM VỤ 3: Khởi tạo RTOS Queues cho dữ liệu cảm biến ===
  // Tạo queues với 1 slot cho Temperature và Humidity (dùng xQueueOverwrite)
  xQueueTemperature = xQueueCreate(1, sizeof(float));
  xQueueHumidity = xQueueCreate(1, sizeof(float));
  xQueueLight = xQueueCreate(5, sizeof(float));
  xQueuePIR = xQueueCreate(5, sizeof(bool));

  // Tạo binary semaphores cho các trạng thái hệ thống
  xSemaphoreStateNormal = xSemaphoreCreateBinary();
  xSemaphoreStateWarning = xSemaphoreCreateBinary();
  xSemaphoreStateCritical = xSemaphoreCreateBinary();

  // Khởi tạo với trạng thái BÌNH THƯỜNG
  xSemaphoreGive(xSemaphoreStateNormal);

  Serial.println("[KHỞI TẠO] Queues và semaphores đã tạo thành công");

  // Khởi tạo điều khiển thiết bị (Quạt & Đèn) - TÙY CHỌN, có thể vô hiệu hóa
  // initDeviceControl();

  // === NHIỆM VỤ 1-4: Chức năng cốt lõi ===
  xTaskCreate(led_blinky, "Task LED Blink" ,2048  ,NULL  ,2 , NULL);              // Nhiệm vụ 1
  xTaskCreate(neo_blinky, "Task NEO Blink" ,2048  ,NULL  ,2 , NULL);              // Nhiệm vụ 2
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor" ,2048  ,NULL  ,2 , NULL); // Nhiệm vụ 3
  xTaskCreate(light_sensor_monitor, "Task GL5528 Monitor" ,4096  ,NULL  ,2 , NULL); // Nhiệm vụ 3
  // xTaskCreate(pir_sensor_monitor, "Task PIR AS312 Monitor" ,2048  ,NULL  ,2 , NULL); // Nhiệm vụ 3

  xTaskCreate(main_server_task, "Task Main Server" ,32768  ,NULL  ,2 , NULL);      // Nhiệm vụ 4

}

void loop() {
  
}