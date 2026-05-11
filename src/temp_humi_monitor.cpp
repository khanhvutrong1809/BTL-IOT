#include "temp_humi_monitor.h"
#include "led_blinky.h"  // Cho xSemaphoreTempUpdate
#include "neo_blinky.h"  // Cho xSemaphoreHumidityUpdate
DHT20 dht20;

/**
 * Hàm hỗ trợ xác định trạng thái hệ thống dựa trên nhiệt độ và độ ẩm
 * Nhiệm vụ 3: 3 Trạng thái dựa trên giá trị cảm biến
 * - BÌNH THƯỜNG: Nhiệt độ 20-30°C VÀ Độ ẩm 40-70%
 * - CẢNH BÁO: Một tham số ngoài phạm vi
 * - NGHIÊM TRỌNG: Cả hai tham số ngoài phạm vi
 */
void updateSystemState(float temperature, float humidity) {
    bool tempInRange = (temperature >= 20.0 && temperature <= 30.0);
    bool humidityInRange = (humidity >= 40.0 && humidity <= 70.0);

    SystemState_t newState;

    if (tempInRange && humidityInRange) {
        newState = SYSTEM_STATE_NORMAL;
    } else if (!tempInRange && !humidityInRange) {
        newState = SYSTEM_STATE_CRITICAL;
    } else {
        newState = SYSTEM_STATE_WARNING;
    }

    // Cập nhật trạng thái nếu thay đổi
    if (newState != currentSystemState) {
        // Lấy tất cả semaphores trước để reset trạng thái
        xSemaphoreTake(xSemaphoreStateNormal, 0);
        xSemaphoreTake(xSemaphoreStateWarning, 0);
        xSemaphoreTake(xSemaphoreStateCritical, 0);

        // Phát semaphore phù hợp
        switch(newState) {
            case SYSTEM_STATE_NORMAL:
                xSemaphoreGive(xSemaphoreStateNormal);
                Serial.println("[TRẠNG THÁI HỆ THỐNG] BÌNH THƯỜNG - Nhiệt độ & Độ ẩm trong phạm vi");
                break;
            case SYSTEM_STATE_WARNING:
                xSemaphoreGive(xSemaphoreStateWarning);
                Serial.println("[TRẠNG THÁI HỆ THỐNG] CẢNH BÁO - Một tham số ngoài phạm vi");
                break;
            case SYSTEM_STATE_CRITICAL:
                xSemaphoreGive(xSemaphoreStateCritical);
                Serial.println("[TRẠNG THÁI HỆ THỐNG] NGHIÊM TRỌNG - Cả hai tham số ngoài phạm vi");
                break;
        }

        currentSystemState = newState;
    }
}



void temp_humi_monitor(void *pvParameters){

    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();

    while (1){
        dht20.read();
        // Đọc nhiệt độ theo độ C
        float temperature = dht20.getTemperature();
        // Đọc độ ẩm
        float humidity = dht20.getHumidity();



        // Kiểm tra nếu đọc thất bại và thoát sớm
        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("Lỗi: Không đọc được cảm biến DHT!");
            temperature = humidity =  -1;
        }

        // === NHIỆM VỤ 3: Gửi dữ liệu qua queues (ĐÃ XÓA biến toàn cục) ===
        // Sử dụng xQueueOverwrite để luôn cập nhật giá trị mới nhất
        if (xQueueTemperature != NULL) {
            xQueueOverwrite(xQueueTemperature, &temperature);
        }
        if (xQueueHumidity != NULL) {
            xQueueOverwrite(xQueueHumidity, &humidity);
        }

        // Nhiệm vụ 3: Cập nhật trạng thái hệ thống dựa trên nhiệt độ và độ ẩm
        updateSystemState(temperature, humidity);

        // Thông báo cho task LED blinky về cập nhật nhiệt độ qua semaphore
        if (xSemaphoreTempUpdate != NULL) {
            xSemaphoreGive(xSemaphoreTempUpdate);
        }

        // Thông báo cho task NeoPixel về cập nhật độ ẩm qua semaphore
        if (xSemaphoreHumidityUpdate != NULL) {
            xSemaphoreGive(xSemaphoreHumidityUpdate);
        }

        // Print the results

        // Serial.print("Humidity: ");
        // Serial.print(humidity);
        // Serial.print("%  Temperature: ");
        // Serial.print(temperature);
        // Serial.println("°C");

        vTaskDelay(5000);
    }

}