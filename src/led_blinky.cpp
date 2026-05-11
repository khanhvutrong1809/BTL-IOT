#include "led_blinky.h"

// Semaphore để báo hiệu cập nhật nhiệt độ
SemaphoreHandle_t xSemaphoreTempUpdate = NULL;

/**
 * Nhiệm vụ 1: LED Nhấp Nháy Theo Nhiệt Độ
 *
 * Phạm vi nhiệt độ và hành vi nhấp nháy tương ứng:
 * - < 20°C  : Nháy CHẬM   (2000ms BẬT / 2000ms TẮT) - Môi trường lạnh
 * - 20-30°C : Nháy THƯỜNG (1000ms BẬT / 1000ms TẮT) - Nhiệt độ thoải mái
 * - > 30°C  : Nháy NHANH  (500ms BẬT / 500ms TẮT)   - Môi trường nóng
 *
 * Đồng bộ hóa:
 * - Sử dụng semaphore (xSemaphoreTempUpdate) để đồng bộ với task temp_humi_monitor
 * - Đọc giá trị nhiệt độ toàn cục khi nhận được semaphore
 */
void led_blinky(void *pvParameters){
    pinMode(LED_GPIO, OUTPUT);

    // Tạo binary semaphore cho cập nhật nhiệt độ
    xSemaphoreTempUpdate = xSemaphoreCreateBinary();

    // Độ trễ nháy mặc định (bình thường)
    uint16_t blink_delay_ms = 1000;

    while(1) {
        // Chờ tín hiệu cập nhật nhiệt độ (kiểm tra không chặn)
        if (xSemaphoreTake(xSemaphoreTempUpdate, 0) == pdTRUE) {
            // Nhiệt độ đã cập nhật, xác định tốc độ nháy mới
            float temp = 25.0; // Giá trị mặc định

            // === NHIỆM VỤ 3: CHỈ đọc từ queue (đã xóa biến toàn cục) ===
            if (xQueueTemperature != NULL) {
                xQueuePeek(xQueueTemperature, &temp, 0);
            }

            if (temp < 20.0) {
                // LẠNH: Nháy chậm (2 giây)
                blink_delay_ms = 2000;
                Serial.println("[LED] Chế độ LẠNH: Nháy chậm (2s)");
            }
            else if (temp >= 20.0 && temp <= 30.0) {
                // BÌNH THƯỜNG: Nháy thường (1 giây)
                blink_delay_ms = 1000;
                Serial.println("[LED] Chế độ BÌNH THƯỜNG: Nháy thường (1s)");
            }
            else {
                // NÓNG: Nháy nhanh (0.5 giây)
                blink_delay_ms = 500;
                Serial.println("[LED] Chế độ NÓNG: Nháy nhanh (0.5s)");
            }
        }

        // Nháy LED với cài đặt độ trễ hiện tại
        digitalWrite(LED_GPIO, HIGH);  // bật LED
        vTaskDelay(blink_delay_ms / portTICK_PERIOD_MS);
        digitalWrite(LED_GPIO, LOW);   // tắt LED
        vTaskDelay(blink_delay_ms / portTICK_PERIOD_MS);
    }
}

void turnLedOn() {
    digitalWrite(LED_GPIO, HIGH);
}

void turnLedOff() {
    digitalWrite(LED_GPIO, LOW);
}