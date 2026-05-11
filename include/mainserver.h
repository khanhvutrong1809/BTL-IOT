#ifndef ___MAIN_SERVER__
#define ___MAIN_SERVER__
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include "global.h"
#include "neo_blinky.h"
#include <HTTPClient.h>
#include <Preferences.h>

#define LED1_PIN 48
#define LED2_PIN 45
#define BOOT_PIN 0
#define MAX_CUSTOM_DEVICES 10

// Cấu trúc lưu thông tin thiết bị
struct CustomDevice {
  bool active;
  char name[32];
  char type[16];
  uint8_t gpio;
  bool state;
};

// Biến global cho custom devices
extern CustomDevice customDevices[MAX_CUSTOM_DEVICES];
extern int customDeviceCount;
extern Preferences preferences;

String htmlHeader(const String& pageTitle, const String& activePage);
String htmlFooter();
String mainPage();
String devicesPage();
String infoPage();
String settingsPage();

void startAP();
void setupServer();
void connectToWiFi();
void setupOTA();
void loadCustomDevices();
void saveCustomDevices();

void main_server_task(void *pvParameters);

#endif