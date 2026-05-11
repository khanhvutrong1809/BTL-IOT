#include "global.h"

// === NHIỆM VỤ 3: RTOS Queues cho dữ liệu cảm biến ===
// Kích thước queue: 5 phần tử, mỗi phần tử có thể lưu một float/bool
QueueHandle_t xQueueTemperature = NULL;
QueueHandle_t xQueueHumidity = NULL;
QueueHandle_t xQueueLight = NULL;
QueueHandle_t xQueuePIR = NULL;

// Nhiệm vụ 3: Semaphores trạng thái hệ thống
SemaphoreHandle_t xSemaphoreStateNormal = NULL;
SemaphoreHandle_t xSemaphoreStateWarning = NULL;
SemaphoreHandle_t xSemaphoreStateCritical = NULL;
SystemState_t currentSystemState = SYSTEM_STATE_NORMAL;

// === NHIỆM VỤ 3: ĐÃ LOẠI BỎ TẤT CẢ BIẾN TOÀN CỤC DỮ LIỆU CẢM BIẾN ===
// glob_temperature, glob_humidity, glob_light_lux, glob_pir_detected đã bị XÓA
// Tất cả dữ liệu hiện được truyền qua RTOS Queues

const char* serverBase = "http://10.255.194.234:3000";
String ssid = "ESP32-YOUR NETWORK HERE!!!";
String password = "12345678";
String wifi_ssid = "COOLER";
String wifi_password = "11111111";
boolean isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();
bool led1_state = false;
bool led2_state = false;

unsigned long connect_start_ms = 0;
bool connecting = false;
