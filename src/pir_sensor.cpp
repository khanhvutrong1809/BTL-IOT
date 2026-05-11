#include "pir_sensor.h"

void pir_sensor_monitor(void *pvParameters) {
    Serial.begin(115200);

    // Khởi tạo chân cảm biến PIR là INPUT
    pinMode(PIR_SENSOR_PIN, INPUT);

    Serial.println("Cảm biến PIR AS312 đã khởi tạo trên GPIO 6");

    while (1) {
        // Đọc tín hiệu digital từ cảm biến PIR
        int pirState = digitalRead(PIR_SENSOR_PIN);

        bool pir_detected = (pirState == HIGH);

        // === NHIỆM VỤ 3: Gửi dữ liệu qua queue (ĐÃ XÓA biến toàn cục) ===
        if (xQueuePIR != NULL) {
            xQueueSend(xQueuePIR, &pir_detected, 0);
        }

        // In trạng thái cảm biến
        // if (glob_pir_detected) {
        //     Serial.println("Phát hiện chuyển động!");
        // } else {
        //     Serial.println("Không có chuyển động");
        // }

        // Delay 500ms (phản hồi nhanh hơn các cảm biến khác)
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
