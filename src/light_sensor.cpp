
#include "light_sensor.h"

void light_sensor_monitor(void *pvParameters) {

    Serial.begin(115200);

    LMV358Light lightSensor(LIGHT_SENSOR_PIN, 3.3, 1.0);
    lightSensor.begin();

    Serial.println("Cảm biến ánh sáng GL5528 đã khởi tạo");

    while (1) {
        // Đọc phần trăm cảm biến ánh sáng
        float light_percent = lightSensor.readPercent();

        // === NHIỆM VỤ 3: Gửi dữ liệu qua queue (ĐÃ XÓA biến toàn cục) ===
        if (xQueueLight != NULL) {
            xQueueSend(xQueueLight, &light_percent, 0);
        }

        // In dữ liệu cảm biến
        // printf("Mức ánh sáng: %.2f %% (Điện áp: %.2f V, Raw: %d)\n",
        //        light_percent,
        //        lightSensor.readVoltage(),
        //        lightSensor.readRaw());

        // Delay 5 giây
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
