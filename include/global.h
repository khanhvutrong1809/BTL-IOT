#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

// === NHIỆM VỤ 3: Thay thế biến toàn cục bằng RTOS Queues ===
// Queues để truyền dữ liệu cảm biến giữa các tasks
extern QueueHandle_t xQueueTemperature;
extern QueueHandle_t xQueueHumidity;
extern QueueHandle_t xQueueLight;
extern QueueHandle_t xQueuePIR;

// Nhiệm vụ 3: Semaphores trạng thái hệ thống (Bình thường/Cảnh báo/Nghiêm trọng)
// Dựa trên nhiệt độ (20-30°C) và độ ẩm (40-70%)
typedef enum {
    SYSTEM_STATE_NORMAL,    // Cả nhiệt độ và độ ẩm trong phạm vi
    SYSTEM_STATE_WARNING,   // Một tham số ngoài phạm vi
    SYSTEM_STATE_CRITICAL   // Cả hai tham số ngoài phạm vi
} SystemState_t;

extern SemaphoreHandle_t xSemaphoreStateNormal;
extern SemaphoreHandle_t xSemaphoreStateWarning;
extern SemaphoreHandle_t xSemaphoreStateCritical;
extern SystemState_t currentSystemState;

// === NHIỆM VỤ 3: ĐÃ LOẠI BỎ TẤT CẢ BIẾN TOÀN CỤC DỮ LIỆU CẢM BIẾN ===
// Tất cả dữ liệu cảm biến hiện được truyền qua RTOS Queues
// glob_temperature, glob_humidity, glob_light_lux, glob_pir_detected đã bị XÓA

extern String ssid;
extern String password;
extern String wifi_ssid;
extern String wifi_password;
extern boolean isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;
extern const char* serverBase;
extern bool led1_state;
extern bool led2_state;
extern unsigned long connect_start_ms;
extern bool connecting;


#endif