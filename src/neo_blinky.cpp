#include "neo_blinky.h"
#include "global.h"

// Giữ strip riêng tư cho file này
static Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

// Semaphore để báo hiệu cập nhật độ ẩm
SemaphoreHandle_t xSemaphoreHumidityUpdate = NULL;

// Current color and brightness
static uint8_t current_r = 0;
static uint8_t current_g = 200;
static uint8_t current_b = 120;
static uint8_t current_brightness = 128;

// Global state variables for scene capture
bool neoBlinkState = false;
uint8_t neoR = 0;
uint8_t neoG = 200;
uint8_t neoB = 120;
uint8_t neoBrightness = 128;

static void _write_color(uint8_t r, uint8_t g, uint8_t b){
    // Apply brightness scaling
    uint8_t br = (r * current_brightness) / 255;
    uint8_t bg = (g * current_brightness) / 255;
    uint8_t bb = (b * current_brightness) / 255;

    strip.setPixelColor(0, strip.Color(br, bg, bb));
    strip.show();
}

void neo_set_on(bool on){
    if (on) {
        _write_color(current_r, current_g, current_b);
    } else {
        _write_color(0, 0, 0);
    }
}

void neo_set_color(uint8_t r, uint8_t g, uint8_t b){
    current_r = r;
    current_g = g;
    current_b = b;
    _write_color(r, g, b);
}

void neo_set_brightness(uint8_t brightness){
    current_brightness = brightness;
    // Re-apply current color with new brightness
    _write_color(current_r, current_g, current_b);
}

/**
 * Nhiệm vụ 2: Điều Khiển NeoPixel LED Theo Độ Ẩm
 *
 * Phạm vi độ ẩm và màu RGB tương ứng:
 * - < 40%   : ĐỎ (255, 0, 0)      - Môi trường khô
 * - 40-70%  : XANH LÁ (0, 255, 0) - Độ ẩm thoải mái
 * - > 70%   : XANH DƯƠNG (0, 0, 255) - Môi trường ẩm ướt
 *
 * Đồng bộ hóa:
 * - Sử dụng semaphore (xSemaphoreHumidityUpdate) để đồng bộ với task temp_humi_monitor
 * - Đọc giá trị độ ẩm toàn cục khi nhận được semaphore
 */
void neo_blinky(void *pvParameters){
    strip.begin();
    strip.clear();
    strip.show();

    // Tạo binary semaphore cho cập nhật độ ẩm
    xSemaphoreHumidityUpdate = xSemaphoreCreateBinary();

    while(1){
        // Chờ tín hiệu cập nhật độ ẩm (chờ chặn với timeout)
        if (xSemaphoreTake(xSemaphoreHumidityUpdate, portMAX_DELAY) == pdTRUE) {
            // Độ ẩm đã cập nhật, xác định màu mới
            float humidity = 55.0; // Giá trị mặc định

            // === NHIỆM VỤ 3: CHỈ đọc từ queue (đã xóa biến toàn cục) ===
            if (xQueueHumidity != NULL) {
                xQueuePeek(xQueueHumidity, &humidity, 0);
            }

            if (humidity < 40.0) {
                // KHÔ: Màu đỏ
                _write_color(255, 0, 0);
                Serial.println("[NeoPixel] Chế độ KHÔ: ĐỎ (Độ ẩm < 40%)");
            }
            else if (humidity >= 40.0 && humidity <= 70.0) {
                // THOẢI MÁI: Màu xanh lá
                _write_color(0, 255, 0);
                Serial.println("[NeoPixel] Chế độ THOẢI MÁI: XANH LÁ (Độ ẩm 40-70%)");
            }
            else {
                // ẨM ƯỚT: Màu xanh dương
                _write_color(0, 0, 255);
                Serial.println("[NeoPixel] Chế độ ẨM ƯỚT: XANH DƯƠNG (Độ ẩm > 70%)");
            }
        }
    }
}

// New functions for scene management
void setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b) {
    neoR = r;
    neoG = g;
    neoB = b;
    neo_set_color(r, g, b);
    neoBlinkState = true;
}

void setNeoPixelBrightness(uint8_t brightness) {
    neoBrightness = brightness;
    neo_set_brightness(brightness);
}

void turnNeoPixelOff() {
    neo_set_on(false);
    neoBlinkState = false;
}

void turnNeoPixelOn() {
    neo_set_on(true);
    neoBlinkState = true;
}