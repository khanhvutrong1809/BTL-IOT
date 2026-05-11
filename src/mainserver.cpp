#include "mainserver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// === RTOS Synchronization Objects ===
static SemaphoreHandle_t xMutexLEDStates = NULL;
static SemaphoreHandle_t xMutexWiFi = NULL;
static SemaphoreHandle_t xMutexSerial = NULL;
static SemaphoreHandle_t xMutexOTA = NULL;
static SemaphoreHandle_t xMutexDevices = NULL;

// === Spinlock for critical sections ===
static portMUX_TYPE criticalMux = portMUX_INITIALIZER_UNLOCKED;

bool isAPMode = true;
WebServer server(80);
static volatile bool otaInProgress = false;

// === Custom Devices Management ===
CustomDevice customDevices[MAX_CUSTOM_DEVICES];
int customDeviceCount = 0;
Preferences preferences;

// Load custom devices from flash
void loadCustomDevices() {
  preferences.begin("devices", false);
  customDeviceCount = preferences.getInt("count", 0);

  if (customDeviceCount > MAX_CUSTOM_DEVICES) {
    customDeviceCount = 0;
  }

  for (int i = 0; i < customDeviceCount; i++) {
    String key = "dev" + String(i);
    size_t len = preferences.getBytesLength(key.c_str());
    if (len == sizeof(CustomDevice)) {
      preferences.getBytes(key.c_str(), &customDevices[i], sizeof(CustomDevice));

      // Initialize GPIO pin
      if (customDevices[i].active && customDevices[i].gpio < 40) {
        pinMode(customDevices[i].gpio, OUTPUT);
        digitalWrite(customDevices[i].gpio, customDevices[i].state ? HIGH : LOW);

        if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
          Serial.printf("Loaded device: %s (GPIO %d)\n", customDevices[i].name, customDevices[i].gpio);
          xSemaphoreGive(xMutexSerial);
        }
      }
    }
  }

  preferences.end();
}

// Save custom devices to flash
void saveCustomDevices() {
  preferences.begin("devices", false);
  preferences.putInt("count", customDeviceCount);

  for (int i = 0; i < customDeviceCount; i++) {
    String key = "dev" + String(i);
    preferences.putBytes(key.c_str(), &customDevices[i], sizeof(CustomDevice));
  }

  preferences.end();
}


// Common HTML header với navigation
String htmlHeader(const String& pageTitle, const String& activePage) {
  return R"rawliteral(
    <!DOCTYPE html>
    <html lang="vi">
    <head>
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <meta charset='utf-8'>
      <title>)rawliteral" + pageTitle + R"rawliteral( - YoloUNO</title>
      <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        :root {
          --primary: #3b82f6;
          --primary-dark: #2563eb;
          --success: #10b981;
          --danger: #ef4444;
          --warning: #f59e0b;
          --dark: #1e293b;
          --gray: #64748b;
          --light-gray: #f1f5f9;
          --card-bg: rgba(255, 255, 255, 0.95);
          --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
          --shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
          --sidebar-width: 240px;
        }

        body {
          font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          min-height: 100vh;
          color: var(--dark);
        }

        .layout {
          display: flex;
          min-height: 100vh;
        }

        /* Sidebar Navigation */
        .sidebar {
          width: var(--sidebar-width);
          background: var(--card-bg);
          box-shadow: var(--shadow-lg);
          position: fixed;
          height: 100vh;
          overflow-y: auto;
          transition: transform 0.3s;
          z-index: 1000;
        }

        .sidebar-header {
          padding: 24px;
          border-bottom: 1px solid #e2e8f0;
        }

        .sidebar-header h1 {
          font-size: 24px;
          font-weight: 700;
          color: var(--dark);
          margin-bottom: 4px;
        }

        .sidebar-header p {
          color: var(--gray);
          font-size: 13px;
        }

        .nav-menu {
          padding: 16px 0;
        }

        .nav-item {
          display: flex;
          align-items: center;
          gap: 12px;
          padding: 12px 24px;
          color: var(--gray);
          text-decoration: none;
          transition: all 0.2s;
          font-weight: 500;
          border-left: 3px solid transparent;
        }

        .nav-item:hover {
          background: var(--light-gray);
          color: var(--dark);
        }

        .nav-item.active {
          background: #eff6ff;
          color: var(--primary);
          border-left-color: var(--primary);
        }

        .nav-icon {
          font-size: 20px;
          width: 24px;
          text-align: center;
        }

        .status-badge {
          margin: 16px 24px;
          padding: 10px;
          background: #d1fae5;
          border-radius: 8px;
          display: flex;
          align-items: center;
          gap: 8px;
          font-size: 13px;
          color: #065f46;
          font-weight: 600;
        }

        .status-dot {
          width: 8px;
          height: 8px;
          border-radius: 50%;
          background: var(--success);
          animation: pulse 2s infinite;
        }

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }

        /* Main Content */
        .main-content {
          margin-left: var(--sidebar-width);
          flex: 1;
          padding: 24px;
        }

        .page-header {
          background: var(--card-bg);
          padding: 24px;
          border-radius: 16px;
          box-shadow: var(--shadow);
          margin-bottom: 24px;
        }

        .page-header h2 {
          font-size: 28px;
          color: var(--dark);
          margin-bottom: 8px;
        }

        .page-header p {
          color: var(--gray);
          font-size: 14px;
        }

        /* Mobile Menu Toggle */
        .menu-toggle {
          display: none;
          position: fixed;
          top: 20px;
          left: 20px;
          z-index: 1001;
          background: var(--card-bg);
          border: none;
          border-radius: 8px;
          padding: 12px;
          cursor: pointer;
          box-shadow: var(--shadow);
        }

        /* Responsive */
        @media (max-width: 768px) {
          .sidebar {
            transform: translateX(-100%);
          }

          .sidebar.show {
            transform: translateX(0);
          }

          .main-content {
            margin-left: 0;
          }

          .menu-toggle {
            display: block;
          }
        }
      </style>
    </head>
    <body>
      <div class="layout">
        <button class="menu-toggle" onclick="toggleMenu()">☰</button>

        <nav class="sidebar" id="sidebar">
          <div class="sidebar-header">
            <h1>🏠 YoloUNO</h1>
            <p>IoT Control System</p>
          </div>

          <div class="status-badge">
            <span class="status-dot"></span>
            <span>Đang hoạt động</span>
          </div>

          <div class="nav-menu">
            <a href="/" class="nav-item )rawliteral" + String(activePage == "home" ? "active" : "") + R"rawliteral(">
              <span class="nav-icon">📊</span>
              <span>Trang chủ</span>
            </a>
            <a href="/devices" class="nav-item )rawliteral" + String(activePage == "devices" ? "active" : "") + R"rawliteral(">
              <span class="nav-icon">💡</span>
              <span>Thiết bị</span>
            </a>
            <a href="/info" class="nav-item )rawliteral" + String(activePage == "info" ? "active" : "") + R"rawliteral(">
              <span class="nav-icon">ℹ️</span>
              <span>Thông tin</span>
            </a>
            <a href="/settings" class="nav-item )rawliteral" + String(activePage == "settings" ? "active" : "") + R"rawliteral(">
              <span class="nav-icon">⚙️</span>
              <span>Cài đặt</span>
            </a>
          </div>
        </nav>

        <div class="main-content">
  )rawliteral";
}

String htmlFooter() {
  return R"rawliteral(
        </div>
      </div>

      <script>
        function toggleMenu() {
          document.getElementById('sidebar').classList.toggle('show');
        }

        // Close sidebar when clicking outside on mobile
        document.addEventListener('click', function(e) {
          const sidebar = document.getElementById('sidebar');
          const toggle = document.querySelector('.menu-toggle');
          if (window.innerWidth <= 768 &&
              !sidebar.contains(e.target) &&
              !toggle.contains(e.target)) {
            sidebar.classList.remove('show');
          }
        });
      </script>
    </body>
    </html>
  )rawliteral";
}

String mainPage() {
  float temperature = 25.0;
  float humidity = 55.0;
  float light = 50.0;

  // === Đọc từ queues với timeout hợp lý ===
  if (xQueueTemperature != NULL) {
    xQueuePeek(xQueueTemperature, &temperature, pdMS_TO_TICKS(10));
  }
  if (xQueueHumidity != NULL) {
    xQueuePeek(xQueueHumidity, &humidity, pdMS_TO_TICKS(10));
  }
  if (xQueueLight != NULL) {
    xQueuePeek(xQueueLight, &light, pdMS_TO_TICKS(10));
  }
  String page = htmlHeader("Trang chủ", "home");

  page += R"rawliteral(
      <div class="page-header">
        <h2>📊 Bảng Điều Khiển Môi Trường</h2>
        <p>Theo dõi các thông số môi trường theo thời gian thực</p>
      </div>

      <style>
        .grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
          gap: 20px;
        }

        .sensor-card {
          background: var(--card-bg);
          padding: 24px;
          border-radius: 16px;
          box-shadow: var(--shadow);
          transition: transform 0.2s;
        }

        .sensor-card:hover {
          transform: translateY(-4px);
          box-shadow: var(--shadow-lg);
        }

        .sensor-header {
          display: flex;
          align-items: center;
          gap: 12px;
          margin-bottom: 16px;
        }

        .sensor-icon {
          width: 48px;
          height: 48px;
          border-radius: 12px;
          display: flex;
          align-items: center;
          justify-content: center;
          font-size: 24px;
        }

        .sensor-card:nth-child(1) .sensor-icon {
          background: linear-gradient(135deg, #ef4444, #dc2626);
        }

        .sensor-card:nth-child(2) .sensor-icon {
          background: linear-gradient(135deg, #06b6d4, #0891b2);
        }

        .sensor-card:nth-child(3) .sensor-icon {
          background: linear-gradient(135deg, #f59e0b, #d97706);
        }

        .sensor-title {
          font-size: 14px;
          color: var(--gray);
          font-weight: 500;
        }

        .sensor-value {
          font-size: 42px;
          font-weight: 700;
          color: var(--dark);
          line-height: 1;
        }

        .sensor-unit {
          font-size: 20px;
          color: var(--gray);
          margin-left: 4px;
        }

        .update-time {
          margin-top: 12px;
          font-size: 12px;
          color: var(--gray);
        }
      </style>

      <div class="grid">
        <div class="sensor-card">
          <div class="sensor-header">
            <div class="sensor-icon">🌡️</div>
            <div class="sensor-title">Nhiệt độ</div>
          </div>
          <div class="sensor-value">
            <span id="temp">)rawliteral" + String(temperature, 1) + R"rawliteral(</span><span class="sensor-unit">°C</span>
          </div>
          <div class="update-time">Cập nhật: <span id="temp-time">Vừa xong</span></div>
        </div>

        <div class="sensor-card">
          <div class="sensor-header">
            <div class="sensor-icon">💧</div>
            <div class="sensor-title">Độ ẩm</div>
          </div>
          <div class="sensor-value">
            <span id="hum">)rawliteral" + String(humidity, 1) + R"rawliteral(</span><span class="sensor-unit">%</span>
          </div>
          <div class="update-time">Cập nhật: <span id="hum-time">Vừa xong</span></div>
        </div>

        <div class="sensor-card">
          <div class="sensor-header">
            <div class="sensor-icon">💡</div>
            <div class="sensor-title">Ánh sáng</div>
          </div>
          <div class="sensor-value">
            <span id="light">)rawliteral" + String(light, 1) + R"rawliteral(</span><span class="sensor-unit">%</span>
          </div>
          <div class="update-time">Cập nhật: <span id="light-time">Vừa xong</span></div>
        </div>
      </div>

      <script>
        function updateSensors() {
          fetch('/sensors')
            .then(r => r.json())
            .then(d => {
              document.getElementById('temp').innerText = d.temp.toFixed(1);
              document.getElementById('hum').innerText = d.hum.toFixed(1);
              document.getElementById('light').innerText = d.light.toFixed(1);

              const now = new Date().toLocaleTimeString('vi-VN');
              document.getElementById('temp-time').innerText = now;
              document.getElementById('hum-time').innerText = now;
              document.getElementById('light-time').innerText = now;
            })
            .catch(e => console.error(e));
        }

        setInterval(updateSensors, 3000);
      </script>
  )rawliteral";

  page += htmlFooter();
  return page;
}

// Trang Thiết bị
String devicesPage() {
  // === Thread-safe đọc LED states ===
  bool led1_local, led2_local;
  if (xMutexLEDStates != NULL && xSemaphoreTake(xMutexLEDStates, pdMS_TO_TICKS(100)) == pdTRUE) {
    led1_local = led1_state;
    led2_local = led2_state;
    xSemaphoreGive(xMutexLEDStates);
  } else {
    led1_local = false;
    led2_local = false;
  }

  String page = htmlHeader("Thiết bị", "devices");

  page += R"rawliteral(
      <div class="page-header">
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <div>
            <h2>💡 Quản Lý Thiết Bị</h2>
            <p>Điều khiển và theo dõi trạng thái các thiết bị IoT</p>
          </div>
          <button class="btn-add-device" onclick="showAddDeviceModal()">
            ➕ Thêm Thiết Bị
          </button>
        </div>
      </div>

      <div class="devices-grid">

      <style>
        .btn-add-device {
          padding: 12px 24px;
          background: var(--primary);
          color: white;
          border: none;
          padding: 12px 24px;
          background: var(--primary);
          color: white;
          border: none;
          border-radius: 8px;
          font-weight: 600;
          font-size: 14px;
          cursor: pointer;
          transition: all 0.2s;
          white-space: nowrap;
        }

        .btn-add-device:hover {
          background: var(--primary-dark);
          transform: translateY(-2px);
          box-shadow: 0 4px 12px rgba(59, 130, 246, 0.4);
        }

        .modal {
          display: none;
          position: fixed;
          z-index: 2000;
          left: 0;
          top: 0;
          width: 100%;
          height: 100%;
          background: rgba(0, 0, 0, 0.5);
          backdrop-filter: blur(4px);
        }

        .modal.show {
          display: flex;
          align-items: center;
          justify-content: center;
          animation: fadeIn 0.3s ease;
        }

        @keyframes fadeIn {
          from { opacity: 0; }
          to { opacity: 1; }
        }

        .modal-content {
          background: var(--card-bg);
          padding: 32px;
          border-radius: 16px;
          box-shadow: var(--shadow-lg);
          max-width: 500px;
          width: 90%;
          animation: slideUp 0.3s ease;
        }

        @keyframes slideUp {
          from {
            opacity: 0;
            transform: translateY(20px);
          }
          to {
            opacity: 1;
            transform: translateY(0);
          }
        }

        .modal-header {
          margin-bottom: 24px;
        }

        .modal-header h3 {
          font-size: 20px;
          color: var(--dark);
          margin-bottom: 8px;
        }

        .modal-header p {
          color: var(--gray);
          font-size: 14px;
        }

        .form-group {
          margin-bottom: 20px;
        }

        .form-group label {
          display: block;
          font-weight: 600;
          color: var(--dark);
          margin-bottom: 8px;
          font-size: 14px;
        }

        .form-group input,
        .form-group select {
          width: 100%;
          padding: 12px 16px;
          border: 2px solid #e2e8f0;
          border-radius: 8px;
          font-size: 14px;
          transition: all 0.2s;
          background: var(--light-gray);
        }

        .form-group input:focus,
        .form-group select:focus {
          outline: none;
          border-color: var(--primary);
          background: white;
        }

        .modal-actions {
          display: flex;
          gap: 12px;
          margin-top: 24px;
        }

        .btn-modal {
          flex: 1;
          padding: 12px;
          border: none;
          border-radius: 8px;
          font-weight: 600;
          font-size: 14px;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-cancel {
          background: var(--light-gray);
          color: var(--dark);
        }

        .btn-cancel:hover {
          background: #e2e8f0;
        }

        .btn-save {
          background: var(--primary);
          color: white;
        }

        .btn-save:hover {
          background: var(--primary-dark);
        }

        .devices-grid {
          display: grid;
          grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
          gap: 20px;
        }

        .device-card {
          background: var(--card-bg);
          padding: 24px;
          border-radius: 16px;
          box-shadow: var(--shadow);
          transition: transform 0.2s;
        }

        .device-card:hover {
          transform: translateY(-4px);
          box-shadow: var(--shadow-lg);
        }

        .device-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 20px;
        }

        .device-info h3 {
          font-size: 18px;
          color: var(--dark);
          margin-bottom: 4px;
        }

        .device-gpio {
          font-size: 13px;
          color: var(--gray);
        }

        .device-status {
          width: 16px;
          height: 16px;
          border-radius: 50%;
          transition: all 0.3s;
        }

        .device-status.on {
          background: var(--success);
          box-shadow: 0 0 16px var(--success);
        }

        .device-status.off {
          background: var(--gray);
        }

        .device-controls {
          margin-top: 16px;
        }

        .toggle-btn {
          width: 100%;
          padding: 14px;
          border: none;
          border-radius: 8px;
          font-weight: 600;
          font-size: 15px;
          cursor: pointer;
          transition: all 0.2s;
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }

        .toggle-btn.on {
          background: var(--success);
          color: white;
        }

        .toggle-btn.on:hover {
          background: #059669;
        }

        .toggle-btn.off {
          background: var(--danger);
          color: white;
        }

        .toggle-btn.off:hover {
          background: #dc2626;
        }

        .toggle-btn:active {
          transform: scale(0.98);
        }

        .loading {
          opacity: 0.6;
          pointer-events: none;
        }
      </style>

      <div class="devices-grid">
        <div class="device-card">
          <div class="device-header">
            <div class="device-info">
              <h3>💡 LED 1</h3>
              <p class="device-gpio">GPIO 48</p>
            </div>
            <span class="device-status )rawliteral" + String(led1_local ? "on" : "off") + R"rawliteral(" id="status1"></span>
          </div>
          <div class="device-controls">
            <button class="toggle-btn )rawliteral" + String(led1_local ? "on" : "off") + R"rawliteral(" id="btn1" onclick="toggleDevice(1)">
              <span id="label1">)rawliteral" + String(led1_local ? "ĐANG BẬT" : "ĐANG TẮT") + R"rawliteral(</span>
            </button>
          </div>
        </div>

        <div class="device-card">
          <div class="device-header">
            <div class="device-info">
              <h3>🌈 LED 2 (NeoPixel)</h3>
              <p class="device-gpio">GPIO 45</p>
            </div>
            <span class="device-status )rawliteral" + String(led2_local ? "on" : "off") + R"rawliteral(" id="status2"></span>
          </div>
          <div class="device-controls">
            <button class="toggle-btn )rawliteral" + String(led2_local ? "on" : "off") + R"rawliteral(" id="btn2" onclick="toggleDevice(2)">
              <span id="label2">)rawliteral" + String(led2_local ? "ĐANG BẬT" : "ĐANG TẮT") + R"rawliteral(</span>
            </button>

            <!-- Color Picker for NeoPixel -->
            <div class="color-control" id="colorControl2" style="margin-top: 12px; display: )rawliteral" + String(led2_local ? "block" : "none") + R"rawliteral(;">
              <label style="display: block; font-size: 13px; color: var(--gray); margin-bottom: 6px;">🎨 Chọn màu:</label>
              <div style="display: flex; gap: 8px; align-items: center;">
                <input type="color" id="colorPicker2" value="#ff0000"
                       style="width: 50px; height: 40px; border: 2px solid #e2e8f0; border-radius: 6px; cursor: pointer;"
                       onchange="changeNeoPixelColor(this.value)">
                <div style="flex: 1; display: flex; flex-direction: column; gap: 4px;">
                  <button onclick="setPresetColor('#ff0000')" style="background: #ff0000; padding: 6px; border: none; border-radius: 4px; cursor: pointer;">🔴</button>
                  <button onclick="setPresetColor('#00ff00')" style="background: #00ff00; padding: 6px; border: none; border-radius: 4px; cursor: pointer;">🟢</button>
                </div>
                <div style="flex: 1; display: flex; flex-direction: column; gap: 4px;">
                  <button onclick="setPresetColor('#0000ff')" style="background: #0000ff; padding: 6px; border: none; border-radius: 4px; cursor: pointer;">🔵</button>
                  <button onclick="setPresetColor('#ffff00')" style="background: #ffff00; padding: 6px; border: none; border-radius: 4px; cursor: pointer;">🟡</button>
                </div>
                <div style="flex: 1; display: flex; flex-direction: column; gap: 4px;">
                  <button onclick="setPresetColor('#ff00ff')" style="background: #ff00ff; padding: 6px; border: none; border-radius: 4px; cursor: pointer;">🟣</button>
                  <button onclick="setPresetColor('#00ffff')" style="background: #00ffff; padding: 6px; border: none; border-radius: 4px; cursor: pointer;">🔷</button>
                </div>
              </div>

              <!-- Brightness Control -->
              <div style="margin-top: 10px;">
                <label style="display: block; font-size: 13px; color: var(--gray); margin-bottom: 6px;">💡 Độ sáng:</label>
                <input type="range" id="brightness2" min="10" max="255" value="128"
                       style="width: 100%;"
                       oninput="changeBrightness(this.value)">
                <span id="brightnessValue" style="font-size: 12px; color: var(--gray);">50%</span>
              </div>
            </div>
          </div>
        </div>
)rawliteral";

  // Add custom devices dynamically
  for (int i = 0; i < customDeviceCount; i++) {
    if (customDevices[i].active) {
      String icon = "📦";
      if (String(customDevices[i].type) == "led") icon = "💡";
      else if (String(customDevices[i].type) == "relay") icon = "🔌";
      else if (String(customDevices[i].type) == "motor") icon = "⚙️";
      else if (String(customDevices[i].type) == "fan") icon = "🌀";
      else if (String(customDevices[i].type) == "pump") icon = "💧";

      int deviceId = 100 + i; // Custom devices start from ID 100

      page += R"rawliteral(
        <div class="device-card">
          <div class="device-header">
            <div class="device-info">
              <h3>)rawliteral" + icon + " " + String(customDevices[i].name) + R"rawliteral(</h3>
              <p class="device-gpio">GPIO )rawliteral" + String(customDevices[i].gpio) + R"rawliteral(</p>
            </div>
            <span class="device-status )rawliteral" + String(customDevices[i].state ? "on" : "off") + R"rawliteral(" id="status)rawliteral" + String(deviceId) + R"rawliteral("></span>
          </div>
          <div class="device-controls">
            <button class="toggle-btn )rawliteral" + String(customDevices[i].state ? "on" : "off") + R"rawliteral(" id="btn)rawliteral" + String(deviceId) + R"rawliteral(" onclick="toggleDevice()rawliteral" + String(deviceId) + R"rawliteral()">
              <span id="label)rawliteral" + String(deviceId) + R"rawliteral(">)rawliteral" + String(customDevices[i].state ? "ĐANG BẬT" : "ĐANG TẮT") + R"rawliteral(</span>
            </button>
            <button class="btn-remove" onclick="removeDevice()rawliteral" + String(i) + R"rawliteral()" style="margin-top: 8px; width: 100%; padding: 10px; background: var(--danger); color: white; border: none; border-radius: 6px; cursor: pointer; font-size: 13px;">🗑️ Xóa</button>
          </div>
        </div>
      )rawliteral";
    }
  }

  page += R"rawliteral(
      </div>

      <!-- Modal Thêm Thiết Bị -->
      <div id="addDeviceModal" class="modal">
        <div class="modal-content">
          <div class="modal-header">
            <h3>➕ Thêm Thiết Bị Mới</h3>
            <p>Nhập thông tin thiết bị bạn muốn thêm vào hệ thống</p>
          </div>

          <form id="addDeviceForm">
            <div class="form-group">
              <label for="deviceName">Tên thiết bị</label>
              <input type="text" id="deviceName" placeholder="Ví dụ: Quạt phòng khách" required>
            </div>

            <div class="form-group">
              <label for="deviceType">Loại thiết bị</label>
              <select id="deviceType" required>
                <option value="">-- Chọn loại thiết bị --</option>
                <option value="led">💡 LED</option>
                <option value="relay">🔌 Relay</option>
                <option value="motor">⚙️ Motor</option>
                <option value="fan">🌀 Quạt</option>
                <option value="pump">💧 Máy bơm</option>
                <option value="other">📦 Khác</option>
              </select>
            </div>

            <div class="form-group">
              <label for="deviceGpio">GPIO Pin</label>
              <input type="number" id="deviceGpio" placeholder="Ví dụ: 12" min="0" max="39" required>
            </div>

            <div class="modal-actions">
              <button type="button" class="btn-modal btn-cancel" onclick="closeAddDeviceModal()">Hủy</button>
              <button type="submit" class="btn-modal btn-save">Thêm Thiết Bị</button>
            </div>
          </form>
        </div>
      </div>

      <script>
        // Modal functions
        function showAddDeviceModal() {
          document.getElementById('addDeviceModal').classList.add('show');
        }

        function closeAddDeviceModal() {
          document.getElementById('addDeviceModal').classList.remove('show');
          document.getElementById('addDeviceForm').reset();
        }

        // Close modal when clicking outside
        document.getElementById('addDeviceModal').addEventListener('click', function(e) {
          if (e.target === this) {
            closeAddDeviceModal();
          }
        });

        // Handle form submission
        document.getElementById('addDeviceForm').addEventListener('submit', function(e) {
          e.preventDefault();

          const deviceName = document.getElementById('deviceName').value;
          const deviceType = document.getElementById('deviceType').value;
          const deviceGpio = document.getElementById('deviceGpio').value;

          // Hiển thị thông báo tạm thời
          alert('⚠️ Tính năng đang phát triển!\n\nThiết bị: ' + deviceName + '\nLoại: ' + deviceType + '\nGPIO: ' + deviceGpio + '\n\nHiện tại hệ thống chỉ hỗ trợ 2 LED cố định. Tính năng thêm thiết bị động sẽ được cập nhật trong phiên bản tiếp theo.');

          closeAddDeviceModal();
        });

        function updateDeviceUI(num, state) {
          const btn = document.getElementById('btn' + num);
          const status = document.getElementById('status' + num);
          const label = document.getElementById('label' + num);

          const isOn = state === 'ON';
          label.innerText = isOn ? 'ĐANG BẬT' : 'ĐANG TẮT';
          btn.className = 'toggle-btn ' + (isOn ? 'on' : 'off');
          status.className = 'device-status ' + (isOn ? 'on' : 'off');
        }

        function toggleDevice(id) {
          const btn = document.getElementById('btn' + id);
          btn.classList.add('loading');

          fetch('/toggle?led=' + id)
            .then(r => r.json())
            .then(data => {
              updateDeviceUI(1, data.led1);
              updateDeviceUI(2, data.led2);
              btn.classList.remove('loading');
            })
            .catch(e => {
              console.error(e);
              btn.classList.remove('loading');
            });
        }

        function refreshDevices() {
          fetch('/toggle?led=0')
            .then(r => r.json())
            .then(data => {
              updateDeviceUI(1, data.led1);
              updateDeviceUI(2, data.led2);

              // Show/hide color control based on LED2 state
              const colorControl = document.getElementById('colorControl2');
              if (colorControl) {
                colorControl.style.display = data.led2 === 'ON' ? 'block' : 'none';
              }
            })
            .catch(() => {});
        }

        // NeoPixel Color Control Functions
        function changeNeoPixelColor(hexColor) {
          const r = parseInt(hexColor.substr(1,2), 16);
          const g = parseInt(hexColor.substr(3,2), 16);
          const b = parseInt(hexColor.substr(5,2), 16);

          fetch('/neopixel_color?r=' + r + '&g=' + g + '&b=' + b)
            .then(r => r.json())
            .then(data => {
              if (data.success) {
                console.log('Color changed to: ' + hexColor);
              }
            })
            .catch(e => console.error(e));
        }

        function setPresetColor(hexColor) {
          document.getElementById('colorPicker2').value = hexColor;
          changeNeoPixelColor(hexColor);
        }

        function changeBrightness(value) {
          const percent = Math.round((value / 255) * 100);
          document.getElementById('brightnessValue').innerText = percent + '%';

          fetch('/neopixel_brightness?value=' + value)
            .then(r => r.json())
            .then(data => {
              if (data.success) {
                console.log('Brightness changed to: ' + percent + '%');
              }
            })
            .catch(e => console.error(e));
        }

        // Tab functionality
        function showTab(tabName) {
          // Not needed for separate pages
        }

        setInterval(refreshDevices, 3000);
      </script>
  )rawliteral";

  page += htmlFooter();
  return page;
}


// Trang Thông tin
String infoPage() {
  String page = htmlHeader("Thông tin", "info");

  page += R"rawliteral(
      <div class="page-header">
        <h2>ℹ️ Thông Tin Hệ Thống</h2>
        <p>Thông tin chi tiết về thiết bị và kết nối</p>
      </div>

      <style>
        .info-grid {
          display: grid;
          gap: 20px;
        }

        .info-card {
          background: var(--card-bg);
          padding: 24px;
          border-radius: 16px;
          box-shadow: var(--shadow);
        }

        .info-card h3 {
          font-size: 18px;
          color: var(--dark);
          margin-bottom: 16px;
          display: flex;
          align-items: center;
          gap: 8px;
        }

        .info-row {
          display: flex;
          justify-content: space-between;
          padding: 12px 0;
          border-bottom: 1px solid #e2e8f0;
        }

        .info-row:last-child {
          border-bottom: none;
        }

        .info-label {
          color: var(--gray);
          font-size: 14px;
        }

        .info-value {
          font-weight: 600;
          color: var(--dark);
          font-size: 14px;
        }

        .status-online {
          color: var(--success);
        }

        .refresh-btn {
          padding: 10px 20px;
          background: var(--primary);
          color: white;
          border: none;
          border-radius: 8px;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.2s;
        }

        .refresh-btn:hover {
          background: var(--primary-dark);
        }
      </style>

      <div class="info-grid">
        <div class="info-card">
          <h3>📡 Thông Tin Kết Nối</h3>
          <div class="info-row">
            <span class="info-label">Trạng thái</span>
            <span class="info-value status-online" id="connection-status">Đang hoạt động</span>
          </div>
          <div class="info-row">
            <span class="info-label">Địa chỉ IP</span>
            <span class="info-value" id="ip-address">)rawliteral" + (isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + R"rawliteral(</span>
          </div>
          <div class="info-row">
            <span class="info-label">Chế độ</span>
            <span class="info-value" id="wifi-mode">)rawliteral" + String(isAPMode ? "Access Point" : "Station Mode") + R"rawliteral(</span>
          </div>
          <div class="info-row">
            <span class="info-label">SSID</span>
            <span class="info-value" id="wifi-ssid">)rawliteral" + (isAPMode ? ssid : wifi_ssid) + R"rawliteral(</span>
          </div>
        </div>

        <div class="info-card">
          <h3>⚙️ Thông Tin Thiết Bị</h3>
          <div class="info-row">
            <span class="info-label">Tên thiết bị</span>
            <span class="info-value">YoloUNO ESP32</span>
          </div>
          <div class="info-row">
            <span class="info-label">Chip</span>
            <span class="info-value">ESP32</span>
          </div>
          <div class="info-row">
            <span class="info-label">Hệ điều hành</span>
            <span class="info-value">FreeRTOS</span>
          </div>
          <div class="info-row">
            <span class="info-label">Thời gian hoạt động</span>
            <span class="info-value" id="uptime">)rawliteral" + String(millis() / 1000) + R"rawliteral( giây</span>
          </div>
        </div>

        <div class="info-card">
          <h3>🔌 Thiết Bị Kết Nối</h3>
          <div class="info-row">
            <span class="info-label">LED 1 (GPIO 48)</span>
            <span class="info-value" id="led1-info">)rawliteral" + String(led1_state ? "Đang bật" : "Đang tắt") + R"rawliteral(</span>
          </div>
          <div class="info-row">
            <span class="info-label">LED 2 / NeoPixel (GPIO 45)</span>
            <span class="info-value" id="led2-info">)rawliteral" + String(led2_state ? "Đang bật" : "Đang tắt") + R"rawliteral(</span>
          </div>
        </div>
      </div>

      <script>
        function updateUptime() {
          fetch('/sensors')
            .then(() => {
              const uptimeEl = document.getElementById('uptime');
              if (uptimeEl) {
                const seconds = parseInt(uptimeEl.innerText);
                uptimeEl.innerText = (seconds + 3) + ' giây';
              }
            })
            .catch(() => {});
        }

        setInterval(updateUptime, 3000);
      </script>
  )rawliteral";

  page += htmlFooter();
  return page;
}

String settingsPage() {
  String page = htmlHeader("Cài đặt", "settings");

  page += R"rawliteral(
      <div class="page-header">
        <h2>⚙️ Cài Đặt Wi-Fi</h2>
        <p>Kết nối thiết bị vào mạng Wi-Fi của bạn</p>
      </div>

      <style>
        .settings-container {
          max-width: 600px;
          margin: 0 auto;
        }

        .settings-card {
          background: var(--card-bg);
          padding: 32px;
          border-radius: 16px;
          box-shadow: var(--shadow);
        }

        .form-group {
          margin-bottom: 20px;
        }

        label {
          display: block;
          font-weight: 600;
          color: var(--dark);
          margin-bottom: 8px;
          font-size: 14px;
        }

        input {
          width: 100%;
          padding: 12px 16px;
          border: 2px solid #e2e8f0;
          border-radius: 8px;
          font-size: 14px;
          transition: all 0.2s;
          background: var(--light-gray);
        }

        input:focus {
          outline: none;
          border-color: var(--primary);
          background: white;
        }

        input::placeholder {
          color: #94a3b8;
        }

        .btn-group {
          display: flex;
          gap: 12px;
          margin-top: 24px;
        }

        .btn {
          flex: 1;
          padding: 14px;
          border: none;
          border-radius: 8px;
          font-weight: 600;
          font-size: 14px;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-primary {
          background: var(--primary);
          color: white;
        }

        .btn-primary:hover:not(:disabled) {
          background: var(--primary-dark);
          transform: translateY(-2px);
          box-shadow: 0 4px 12px rgba(59, 130, 246, 0.4);
        }

        .btn-secondary {
          background: var(--light-gray);
          color: var(--dark);
        }

        .btn-secondary:hover {
          background: #e2e8f0;
        }

        .btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        .alert {
          padding: 12px 16px;
          border-radius: 8px;
          margin-top: 20px;
          font-size: 14px;
          display: none;
        }

        .alert.info {
          background: #dbeafe;
          color: #1e40af;
          border: 1px solid #bfdbfe;
        }

        .alert.success {
          background: #d1fae5;
          color: #065f46;
          border: 1px solid #6ee7b7;
        }

        .alert.error {
          background: #fee2e2;
          color: #991b1b;
          border: 1px solid #fecaca;
        }

        .alert.show {
          display: block;
          animation: slideIn 0.3s ease;
        }

        @keyframes slideIn {
          from {
            opacity: 0;
            transform: translateY(-10px);
          }
          to {
            opacity: 1;
            transform: translateY(0);
          }
        }

        .spinner {
          display: inline-block;
          width: 14px;
          height: 14px;
          border: 2px solid rgba(255, 255, 255, 0.3);
          border-radius: 50%;
          border-top-color: white;
          animation: spin 0.6s linear infinite;
          margin-right: 8px;
        }

        @keyframes spin {
          to { transform: rotate(360deg); }
        }

        .info-box {
          background: #f0f9ff;
          border: 1px solid #bfdbfe;
          border-radius: 8px;
          padding: 16px;
          margin-bottom: 20px;
        }

        .info-box h4 {
          color: #1e40af;
          font-size: 14px;
          margin-bottom: 8px;
        }

        .info-box p {
          color: #3b82f6;
          font-size: 13px;
          line-height: 1.5;
        }
      </style>

      <div class="settings-container">
        <div class="settings-card">
          <div class="info-box">
            <h4>📶 Kết nối Wi-Fi</h4>
            <p>Nhập thông tin mạng Wi-Fi để kết nối thiết bị. Sau khi lưu cấu hình, thiết bị sẽ tự động kết nối.</p>
          </div>

          <form id="wifiForm">
            <div class="form-group">
              <label for="ssid">Tên mạng Wi-Fi (SSID)</label>
              <input type="text" id="ssid" name="ssid" placeholder="Nhập tên mạng Wi-Fi" required>
            </div>

            <div class="form-group">
              <label for="pass">Mật khẩu Wi-Fi</label>
              <input type="password" id="pass" name="password" placeholder="Nhập mật khẩu">
            </div>

            <div class="btn-group">
              <button type="button" class="btn btn-secondary" onclick="window.location='/'">Hủy</button>
              <button type="submit" class="btn btn-primary" id="connectBtn">Lưu cấu hình</button>
            </div>
          </form>

          <div id="alert" class="alert"></div>
        </div>
      </div>

      <script>
        const form = document.getElementById('wifiForm');
        const btn = document.getElementById('connectBtn');
        const alert = document.getElementById('alert');

        function showAlert(message, type) {
          alert.className = 'alert ' + type + ' show';
          alert.innerText = message;
        }

        form.addEventListener('submit', function(e) {
          e.preventDefault();

          const ssid = document.getElementById('ssid').value.trim();
          const pass = document.getElementById('pass').value;

          if (!ssid) {
            showAlert('Vui lòng nhập tên mạng Wi-Fi', 'error');
            return;
          }

          btn.disabled = true;
          btn.innerHTML = '<span class="spinner"></span>Đang kết nối...';
          showAlert('Đang kết nối đến ' + ssid + '...', 'info');

          fetch('/connect?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass))
            .then(() => {
              let attempts = 0;
              const maxAttempts = 15;

              const poll = setInterval(() => {
                fetch('/connect_status')
                  .then(r => r.text())
                  .then(status => {
                    if (status === 'CONNECTED') {
                      clearInterval(poll);
                      showAlert('✓ Kết nối thành công!', 'success');
                      setTimeout(() => { window.location = '/' }, 1500);
                    } else if (status === 'FAILED') {
                      clearInterval(poll);
                      showAlert('✗ Kết nối thất bại. Vui lòng kiểm tra lại thông tin.', 'error');
                      btn.disabled = false;
                      btn.innerText = 'Lưu cấu hình';
                    } else {
                      showAlert('Đang kết nối... (' + Math.min(attempts, maxAttempts) + 's)', 'info');
                    }
                  })
                  .catch(() => {});

                attempts++;
                if (attempts > maxAttempts) {
                  clearInterval(poll);
                  showAlert('✗ Quá thời gian chờ kết nối', 'error');
                  btn.disabled = false;
                  btn.innerText = 'Lưu cấu hình';
                }
              }, 1000);
            })
            .catch(e => {
              showAlert('✗ Lỗi: ' + e.message, 'error');
              btn.disabled = false;
              btn.innerText = 'Lưu cấu hình';
            });
        });
      </script>
  )rawliteral";

  page += htmlFooter();
  return page;
}

// ========== Xử lý request ==========
void handleRoot() { server.send(200, "text/html", mainPage()); }

void handleDevices() { server.send(200, "text/html", devicesPage()); }

void handleInfo() { server.send(200, "text/html", infoPage()); }

void handleSettings() { server.send(200, "text/html", settingsPage()); }

void handleToggle() {
  int led = server.arg("led").toInt();

  // Handle custom devices (ID >= 100)
  if (led >= 100) {
    int customIndex = led - 100;

    if (customIndex >= 0 && customIndex < customDeviceCount) {
      if (xMutexDevices != NULL && xSemaphoreTake(xMutexDevices, pdMS_TO_TICKS(100)) == pdTRUE) {
        customDevices[customIndex].state = !customDevices[customIndex].state;
        digitalWrite(customDevices[customIndex].gpio, customDevices[customIndex].state ? HIGH : LOW);

        String response = "{\"success\":true,\"state\":\"" + String(customDevices[customIndex].state ? "ON" : "OFF") + "\"}";
        xSemaphoreGive(xMutexDevices);

        saveCustomDevices(); // Save state to flash

        server.send(200, "application/json", response);
        return;
      } else {
        server.send(503, "application/json", "{\"error\":\"Resource busy\"}");
        return;
      }
    }
  }

  // === Critical Section: Thread-safe LED state modification ===
  if (xMutexLEDStates != NULL && xSemaphoreTake(xMutexLEDStates, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (led == 1) {
      led1_state = !led1_state;
      portENTER_CRITICAL(&criticalMux);
      digitalWrite(LED1_PIN, led1_state ? HIGH : LOW);
      portEXIT_CRITICAL(&criticalMux);

      if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.print("LED1 (GPIO");
        Serial.print(LED1_PIN);
        Serial.print(") -> ");
        Serial.println(led1_state ? "ON" : "OFF");
        xSemaphoreGive(xMutexSerial);
      }
    }
    else if (led == 2) {
      led2_state = !led2_state;
      neo_set_on(led2_state);

      if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.print("LED2 (NeoPin ");
        Serial.print(LED2_PIN);
        Serial.print(") -> ");
        Serial.println(led2_state ? "ON" : "OFF");
        xSemaphoreGive(xMutexSerial);
      }
    }

    // Tạo JSON response trong critical section để đảm bảo consistency
    String response = "{\"led1\":\"" + String(led1_state ? "ON":"OFF") +
                     "\",\"led2\":\"" + String(led2_state ? "ON":"OFF") + "\"}";
    xSemaphoreGive(xMutexLEDStates);

    server.send(200, "application/json", response);
  } else {
    server.send(503, "application/json", "{\"error\":\"Resource busy\"}");
  }
}

void handleSensors() {
  float t = 25.0;
  float h = 55.0;
  float l = 50.0;

  // === Đọc từ queues với timeout hợp lý ===
  if (xQueueTemperature != NULL) {
    xQueuePeek(xQueueTemperature, &t, pdMS_TO_TICKS(10));
  }
  if (xQueueHumidity != NULL) {
    xQueuePeek(xQueueHumidity, &h, pdMS_TO_TICKS(10));
  }
  if (xQueueLight != NULL) {
    xQueuePeek(xQueueLight, &l, pdMS_TO_TICKS(10));
  }

  String json = "{\"temp\":"+String(t, 1)+",\"hum\":"+String(h, 1)+",\"light\":"+String(l, 1)+"}";
  server.send(200, "application/json", json);
}

void handleConnect() {
  wifi_ssid = server.arg("ssid");
  wifi_password = server.arg("pass");

  if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.print("Connect request for SSID: ");
    Serial.println(wifi_ssid);
    xSemaphoreGive(xMutexSerial);
  }

  if (xMutexWiFi != NULL && xSemaphoreTake(xMutexWiFi, pdMS_TO_TICKS(100)) == pdTRUE) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    xSemaphoreGive(xMutexWiFi);

    connect_start_ms = millis();
    connecting = true;
    isAPMode = false;

    server.send(200, "text/plain", "CONNECTING");
  } else {
    server.send(503, "text/plain", "WiFi resource busy");
  }
}

void handleConnectStatus() {
  wl_status_t status;

  if (xMutexWiFi != NULL && xSemaphoreTake(xMutexWiFi, pdMS_TO_TICKS(50)) == pdTRUE) {
    status = WiFi.status();
    xSemaphoreGive(xMutexWiFi);
  } else {
    server.send(503, "text/plain", "BUSY");
    return;
  }

  if (status == WL_CONNECTED) {
    server.send(200, "text/plain", "CONNECTED");
  } else if (connecting) {
    if (millis() - connect_start_ms > 15000) {
      server.send(200, "text/plain", "FAILED");
    } else {
      server.send(200, "text/plain", "CONNECTING");
    }
  } else {
    server.send(200, "text/plain", isWifiConnected ? "CONNECTED" : "FAILED");
  }
}

// ========== NeoPixel Color Control ==========
void handleNeoPixelColor() {
  int r = server.arg("r").toInt();
  int g = server.arg("g").toInt();
  int b = server.arg("b").toInt();

  // Validate RGB values
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Invalid RGB values\"}");
    return;
  }

  // Set NeoPixel color
  neo_set_color((uint8_t)r, (uint8_t)g, (uint8_t)b);

  if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.printf("NeoPixel color set to RGB(%d, %d, %d)\n", r, g, b);
    xSemaphoreGive(xMutexSerial);
  }

  server.send(200, "application/json", "{\"success\":true}");
}

void handleNeoPixelBrightness() {
  int brightness = server.arg("value").toInt();

  // Validate brightness (10-255)
  if (brightness < 10 || brightness > 255) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Invalid brightness\"}");
    return;
  }

  // Set NeoPixel brightness
  neo_set_brightness((uint8_t)brightness);

  if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.printf("NeoPixel brightness set to %d\n", brightness);
    xSemaphoreGive(xMutexSerial);
  }

  server.send(200, "application/json", "{\"success\":true}");
}

// ========== Custom Device Handlers ==========
void handleAddDevice() {
  String name = server.arg("name");
  String type = server.arg("type");
  int gpio = server.arg("gpio").toInt();

  if (name.length() == 0 || gpio < 0 || gpio > 39) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Thông tin không hợp lệ\"}");
    return;
  }

  if (customDeviceCount >= MAX_CUSTOM_DEVICES) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Đã đạt giới hạn thiết bị\"}");
    return;
  }

  if (xMutexDevices != NULL && xSemaphoreTake(xMutexDevices, pdMS_TO_TICKS(200)) == pdTRUE) {
    // Add new device
    customDevices[customDeviceCount].active = true;
    name.toCharArray(customDevices[customDeviceCount].name, 32);
    type.toCharArray(customDevices[customDeviceCount].type, 16);
    customDevices[customDeviceCount].gpio = gpio;
    customDevices[customDeviceCount].state = false;

    // Initialize GPIO
    pinMode(gpio, OUTPUT);
    digitalWrite(gpio, LOW);

    customDeviceCount++;

    // Save to flash
    saveCustomDevices();

    xSemaphoreGive(xMutexDevices);

    if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
      Serial.printf("Added device: %s (GPIO %d)\n", name.c_str(), gpio);
      xSemaphoreGive(xMutexSerial);
    }

    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(503, "application/json", "{\"success\":false,\"message\":\"Resource busy\"}");
  }
}

void handleRemoveDevice() {
  int index = server.arg("index").toInt();

  if (index < 0 || index >= customDeviceCount) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Index không hợp lệ\"}");
    return;
  }

  if (xMutexDevices != NULL && xSemaphoreTake(xMutexDevices, pdMS_TO_TICKS(200)) == pdTRUE) {
    // Turn off GPIO before removing
    digitalWrite(customDevices[index].gpio, LOW);

    // Mark as inactive
    customDevices[index].active = false;

    // Shift array
    for (int i = index; i < customDeviceCount - 1; i++) {
      customDevices[i] = customDevices[i + 1];
    }
    customDeviceCount--;

    // Save to flash
    saveCustomDevices();

    xSemaphoreGive(xMutexDevices);

    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(503, "application/json", "{\"success\":false,\"message\":\"Resource busy\"}");
  }
}

// ========== Cấu hình WiFi ==========
void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/devices", HTTP_GET, handleDevices);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/sensors", HTTP_GET, handleSensors);
  server.on("/connect", HTTP_GET, handleConnect);
  server.on("/connect_status", HTTP_GET, handleConnectStatus);
  server.on("/add_device", HTTP_GET, handleAddDevice);
  server.on("/remove_device", HTTP_GET, handleRemoveDevice);
  server.on("/neopixel_color", HTTP_GET, handleNeoPixelColor);
  server.on("/neopixel_brightness", HTTP_GET, handleNeoPixelBrightness);

  server.begin();
}

// ========== OTA Setup với FreeRTOS synchronization ==========
void setupOTA() {
  if (xMutexOTA == NULL) {
    xMutexOTA = xSemaphoreCreateMutex();
  }

  ArduinoOTA.setHostname("YoloUNO-ESP32");
  ArduinoOTA.setPassword("yolouno2024");

  ArduinoOTA.onStart([]() {
    otaInProgress = true;

    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";

    if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(100)) == pdTRUE) {
      Serial.println("OTA Update Start: " + type);
      xSemaphoreGive(xMutexSerial);
    }

    // Suspend non-critical tasks during OTA
    portENTER_CRITICAL(&criticalMux);
  });

  ArduinoOTA.onEnd([]() {
    portEXIT_CRITICAL(&criticalMux);

    if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(100)) == pdTRUE) {
      Serial.println("\nOTA Update Complete!");
      xSemaphoreGive(xMutexSerial);
    }

    otaInProgress = false;
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static uint8_t lastPercent = 0;
    uint8_t percent = (progress / (total / 100));

    // Chỉ log mỗi 10%
    if (percent % 10 == 0 && percent != lastPercent) {
      lastPercent = percent;

      if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.printf("OTA Progress: %u%%\n", percent);
        xSemaphoreGive(xMutexSerial);
      }

      // Blink LED để báo tiến trình
      if (xMutexLEDStates != NULL && xSemaphoreTake(xMutexLEDStates, pdMS_TO_TICKS(10)) == pdTRUE) {
        digitalWrite(LED1_PIN, (percent % 20 == 0) ? HIGH : LOW);
        xSemaphoreGive(xMutexLEDStates);
      }
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    portEXIT_CRITICAL(&criticalMux);

    if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(100)) == pdTRUE) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
      xSemaphoreGive(xMutexSerial);
    }

    otaInProgress = false;
  });

  ArduinoOTA.begin();

  if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.println("OTA Ready");
    xSemaphoreGive(xMutexSerial);
  }
}

void startAP() {
  if (xMutexWiFi != NULL && xSemaphoreTake(xMutexWiFi, pdMS_TO_TICKS(200)) == pdTRUE) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str(), password.c_str());

    if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
      Serial.print("AP IP address: ");
      Serial.println(WiFi.softAPIP());
      xSemaphoreGive(xMutexSerial);
    }

    xSemaphoreGive(xMutexWiFi);
  }

  isAPMode = true;
  connecting = false;
}

void connectToWiFi() {
  if (xMutexWiFi != NULL && xSemaphoreTake(xMutexWiFi, pdMS_TO_TICKS(200)) == pdTRUE) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
      Serial.print("Connecting to: ");
      Serial.print(wifi_ssid.c_str());
      Serial.print(" Password: ");
      Serial.print(wifi_password.c_str());
      xSemaphoreGive(xMutexSerial);
    }

    xSemaphoreGive(xMutexWiFi);
  }
}

// ========== Main task ==========
void main_server_task(void *pvParameters){
  // === Tạo Mutexes cho thread safety ===
  xMutexLEDStates = xSemaphoreCreateMutex();
  xMutexWiFi = xSemaphoreCreateMutex();
  xMutexSerial = xSemaphoreCreateMutex();
  xMutexOTA = xSemaphoreCreateMutex();
  xMutexDevices = xSemaphoreCreateMutex();

  if (xMutexLEDStates == NULL || xMutexWiFi == NULL || xMutexSerial == NULL || xMutexOTA == NULL || xMutexDevices == NULL) {
    Serial.println("ERROR: Failed to create mutexes!");
    vTaskDelete(NULL);
    return;
  }

  pinMode(BOOT_PIN, INPUT_PULLUP);
  pinMode(LED1_PIN, OUTPUT);

  // === Thread-safe LED initialization ===
  if (xMutexLEDStates != NULL && xSemaphoreTake(xMutexLEDStates, portMAX_DELAY) == pdTRUE) {
    portENTER_CRITICAL(&criticalMux);
    digitalWrite(LED1_PIN, led1_state ? HIGH : LOW);
    portEXIT_CRITICAL(&criticalMux);
    xSemaphoreGive(xMutexLEDStates);
  }

  // === Load custom devices from flash ===
  loadCustomDevices();

  startAP();
  setupServer();

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20);

  while(1){
    // === Handle OTA updates (chỉ khi có WiFi connection) ===
    if (!isAPMode && isWifiConnected && !otaInProgress) {
      if (xMutexOTA != NULL && xSemaphoreTake(xMutexOTA, 0) == pdTRUE) {
        ArduinoOTA.handle();
        xSemaphoreGive(xMutexOTA);
      }
    }

    // === Handle web server ===
    if (!otaInProgress) {
      server.handleClient();
    }

    // === Button debouncing với proper delay ===
    if (digitalRead(BOOT_PIN) == LOW) {
      vTaskDelay(pdMS_TO_TICKS(50));
      if (digitalRead(BOOT_PIN) == LOW) {
        if (!isAPMode) {
          startAP();
          setupServer();
        }
      }
    }

    // === WiFi connection handling ===
    if (connecting) {
      wl_status_t wifi_status;

      if (xMutexWiFi != NULL && xSemaphoreTake(xMutexWiFi, pdMS_TO_TICKS(10)) == pdTRUE) {
        wifi_status = WiFi.status();
        xSemaphoreGive(xMutexWiFi);

        if (wifi_status == WL_CONNECTED) {
          if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
            Serial.print("STA IP address: ");
            Serial.println(WiFi.localIP());
            xSemaphoreGive(xMutexSerial);
          }

          isWifiConnected = true;

          // === Setup OTA sau khi kết nối WiFi thành công ===
          setupOTA();

          if (xBinarySemaphoreInternet != NULL) {
            xSemaphoreGive(xBinarySemaphoreInternet);
          }

          isAPMode = false;
          connecting = false;

        } else if (millis() - connect_start_ms > 10000) {
          if (xMutexSerial != NULL && xSemaphoreTake(xMutexSerial, pdMS_TO_TICKS(50)) == pdTRUE) {
            Serial.println("WiFi connect failed! Back to AP.");
            xSemaphoreGive(xMutexSerial);
          }

          startAP();
          setupServer();
          connecting = false;
          isWifiConnected = false;
        }
      }
    }

    // === Periodic delay với vTaskDelayUntil cho chính xác hơn ===
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

  }
}
