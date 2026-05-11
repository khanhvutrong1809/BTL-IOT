#include <Arduino.h>


class LMV358Light {
private:
uint8_t pin;
float vref;
float scale;


public:
// pin: chân ADC đọc tín hiệu từ LMV358
// vref: điện áp tham chiếu ADC (thường 5.0V hoặc 3.3V)
// scale: hệ số khuếch đại hoặc tỉ lệ cảm biến nếu có
LMV358Light(uint8_t pin, float vref = 3.3, float scale = 1.0) {
this->pin = pin;
this->vref = vref;
this->scale = scale;
}


void begin() {
pinMode(pin, INPUT);
}


// Đọc giá trị ADC (0 - 1023)
int readRaw() {
return analogRead(pin);
}


// Trả về điện áp sau LMV358
float readVoltage() {
int raw = analogRead(pin);
return (raw * vref / 4095.0);
}


// Quy đổi ra đơn vị ánh sáng tương đối (0-100%)
float readPercent() {
float v = readVoltage();
float percent = (v / vref) * 100.0 * scale;
if (percent > 100.0) percent = 100.0;
if (percent < 0.0) percent = 0.0;
return percent;
}
};

