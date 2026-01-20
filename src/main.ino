/*
 * Electronic Load Controller - Электронная нагрузка
 * ESP01 + INA226 + MOSFET
 * Версия: 2023.12.01
 * 
 * Подключение:
 * INA226 SDA -> GPIO0
 * INA226 SCL -> GPIO2  
 * MOSFET Gate -> GPIO3 (через резистор 100-470 Ом)
 * 
 * Режимы работы:
 * - Constant Current (CC) - постоянный ток
 * - Constant Power (CP) - постоянная мощность  
 * - Constant Resistance (CR) - постоянное сопротивление
 * - Battery Discharge - разряд аккумулятора
 * - OFF - выключено
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>

// Конфигурация пинов ESP-01
const int MOSFET_PIN = 3;   // GPIO3 (RX) - ШИМ для MOSFET (электронная нагрузка)
#define I2C_SDA 2         // GPIO0 - SDA для INA226
#define I2C_SCL 0         // GPIO2 - SCL для INA226

// Константы по умолчанию
#define DEFAULT_MAX_CURRENT 10.0
#define DEFAULT_MAX_VOLTAGE 30.0
#define DEFAULT_SHUNT_RESISTOR 0.0135
#define FIRMWARE_VERSION "1.0.0"

// Режимы работы нагрузки
enum LoadMode {
  MODE_OFF = 0,
  MODE_CC = 1,     // Constant Current
  MODE_CP = 2,     // Constant Power
  MODE_CR = 3,     // Constant Resistance
  MODE_DISCHARGE = 4, // Battery Discharge
  MODE_CALIBRATE = 5 // Calibration mode
};

// Веб-сервер
ESP8266WebServer server(80);

// HTTP Update Server
ESP8266HTTPUpdateServer httpUpdater;

// INA226
INA226_WE ina226 = INA226_WE(0x40);
bool ina226_connected = false;

// Простое регулирование для режима CC
double setpoint_current = 0.0;     // Заданный ток (А)
double pwm_output = 0.0;           // Выход ШИМ (0-1023)

// Основные переменные состояния
LoadMode current_mode = MODE_OFF;
float target_current = 0.0;        // Целевой ток (А)
float target_power = 0.0;          // Целевая мощность (Вт)
float target_resistance = 10.0;    // Целевое сопротивление (Ом)
float measured_voltage = 0.0;      // Измеренное напряжение (В)
double measured_current = 0.0;     // Измеренный ток (А)
float measured_power = 0.0;        // Измеренная мощность (Вт)

// Фильтры для стабилизации измерений
const float ALPHA = 0.3; // коэффициент фильтра (0.1-0.5)
float filtered_voltage = 0.0;
double filtered_current = 0.0;

// Общее время работы (с)
unsigned long operation_time = 0;

// WiFi настройки
String device_name = "Nagruzka";
String home_ssid = "";
String home_password = "";
IPAddress logger_ip(192, 168, 1, 171);
int logger_period = 60;             // Период отправки данных (сек)
const String ap_password = "12345678";
const IPAddress ap_ip(192, 168, 4, 1);

// Системные флаги
bool system_enabled = true;
bool protection_enabled = true;
bool auto_restart = false;

  // Структура для хранения настроек в EEPROM
  struct Settings {
    char device_name[32];
    char home_ssid[32];
    char home_password[32];

    // Калибровка
    float shunt_resistance;

    // Защиты
    float max_current;
    float max_voltage;

    // Разряд
    float cutoff_voltage;
    float discharge_current;
    float capacity_limit;

    uint32_t crc;
  } settings;

// Прототипы функций
void setupPins();
void initializeINA226();
void setupWiFi();
void setupWebServer();
void loadSettings();
void saveSettings();
uint32_t calculateCRC32(uint8_t *data, size_t length);
void readMeasurements();
void updateLoadControl();
void checkProtection();
void emergencyStop();
void sendToLogger();

// Веб-страницы
void handleRoot();
void handleNotFound();

// API обработчики
void handleApiStatus();
void handleApiDischargeStatus();
void handleApiControl();
void handleApiDischargeControl();
void handleApiSettings();
void handleApiCalibrate();
void handleApiReset();
void handleApiSaveSettings();
// Extern функции из calib.ino
extern void initializeCalibration();
extern void updateCalibrationMode();
extern void handleApiMosfetOpen();
extern void handleApiMosfetClose();
extern void handleApiGetCalibration();
extern void handleApiSaveCalibration();
extern void handleApiCalibrate();

// Extern функции из discharge.ino
extern void initializeDischarge();
extern void handleApiDischargeStatus();
extern void handleApiDischargeControl();

// Extern переменные из calib.ino
extern float shunt_resistance;
extern float voltage_offset;
extern float current_offset;
extern float wire_resistance;
extern int battery_cells;

// Extern переменные из discharge.ino
extern float cutoff_voltage;
extern float discharge_current;
extern float capacity_limit;
extern float accumulated_charge;
extern float accumulated_energy;
extern bool discharge_active;
extern unsigned long discharge_start_time;

// Extern функции из discharge.ino
extern void updateDischargeControl();
extern void stopDischarge();
extern void resetDischargeSession();

void handleApiSaveCalibration();
void handleApiScanNetworks();
void handleApiConnectToWifi();

// Вспомогательные функции
String formatTime(unsigned long seconds);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== Electronic Load ESP01 ===");
  Serial.println("Version: 2023.12.01");
  Serial.println("INA226 Current/Voltage Sensor");
  Serial.println("Battery Discharge Controller");
  
  // Настройка пинов
  setupPins();
  
  // Инициализация I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);  // 100 kHz
  
  // Инициализация EEPROM
  EEPROM.begin(512);
  loadSettings();
  
  // Инициализация модулей
  initializeCalibration();
  initializeDischarge();

  // Инициализация INA226
  initializeINA226();

  // Настройка WiFi
  setupWiFi();
  
  // Настройка веб-сервера
  setupWebServer();
  
  // Настройка mDNS
  if (MDNS.begin(device_name.c_str())) {
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }

  // Настройка OTA
  ArduinoOTA.setHostname(device_name.c_str());
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update started");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update finished");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");

  Serial.println("System initialized successfully");
  Serial.print("Device name: ");
  Serial.println(device_name);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();
  
  static unsigned long last_measurement = 0;
  static unsigned long last_display = 0;
  static unsigned long last_pid_update = 0;
  static unsigned long last_logger_send = 0;
  
  unsigned long current_time = millis();
  
  // Чтение измерений каждые 100 мс
  if (current_time - last_measurement >= 100) {
    last_measurement = current_time;
    readMeasurements();
    
    // Управление разрядом
    if (current_mode == MODE_DISCHARGE && discharge_active) {
      updateDischargeControl();
    }
    
    // Проверка защиты
    if (protection_enabled) {
      checkProtection();
    }
    
    // Накопление энергии и заряда
    accumulated_energy += (measured_power * 0.1 / 3600.0); // Вт*ч
    if (discharge_active) {
      accumulated_charge += (measured_current * 0.1 / 3600.0); // А*ч
    }
  }
  
  // Управление нагрузкой каждые 10 мс (максимальная скорость)
  if (current_time - last_pid_update >= 10) {
    last_pid_update = current_time;
    updateLoadControl();
  }
  
  // Отправка данных логгеру
  if (logger_period > 0 && current_time - last_logger_send >= (unsigned long)logger_period * 1000) {
    last_logger_send = current_time;
    sendToLogger();
  }
  
  // Вывод статуса каждую секунду
  if (current_time - last_display >= 1000) {
    last_display = current_time;
    operation_time++;
    
    if (discharge_active) {
      Serial.print("DISCHARGE | ");
    } else {
      Serial.print("Mode: ");
      switch(current_mode) {
        case MODE_OFF: Serial.print("OFF"); break;
        case MODE_CC: Serial.print("CC"); break;
        case MODE_CP: Serial.print("CP"); break;
        case MODE_CR: Serial.print("CR"); break;
        case MODE_DISCHARGE: Serial.print("DISCHARGE_IDLE"); break;
        case MODE_CALIBRATE: Serial.print("CALIBRATE"); break;
      }
      Serial.print(" | ");
    }
    
    Serial.print("V: ");
    Serial.print(measured_voltage, 3);
    Serial.print("V | I: ");
    Serial.print(measured_current, 3);
    Serial.print("A | Setpoint: ");
    Serial.print(setpoint_current, 3);
    Serial.print("A | P: ");
    Serial.print(measured_power, 2);
    Serial.print("W | Ah: ");
    Serial.print(accumulated_charge, 3);
    Serial.print("Ah | Cutoff: ");
    Serial.print(cutoff_voltage, 2);
    Serial.print("V | Discharge I: ");
    Serial.print(discharge_current, 3);
    Serial.print("A | Time: ");
    Serial.print(formatDischargeTime());
    Serial.println();
  }
  
  delay(10);
}

void setupPins() {
  pinMode(MOSFET_PIN, OUTPUT);
  analogWriteFreq(1000);    // Частота ШИМ 1 кГц
  analogWriteRange(1023);   // 10-битный ШИМ
  digitalWrite(MOSFET_PIN, LOW);
}

void initializeINA226() {
  Serial.print("Initializing INA226... ");

  if (!ina226.init()) {
    Serial.println("FAILED!");
    ina226_connected = false;

    // Значения по умолчанию для отладки
    measured_voltage = 12.0;
    measured_current = 0.0;
    measured_power = 0.0;
  } else {
    Serial.println("OK");
    ina226_connected = true;

    // Установка сопротивления шунта и максимального тока
    ina226.setResistorRange(shunt_resistance, 10.0); // Увеличить max current до 10A как в примере

    Serial.print("Shunt resistance set to: ");
    Serial.print(shunt_resistance * 1000, 2);
    Serial.println(" mOhm");
  }
}

void readMeasurements() {
  if (ina226_connected) {
    // Базовые измерения
    float raw_voltage = ina226.getBusVoltage_V();
    double raw_current = ina226.getCurrent_mA() / 1000.0; // мА -> А

    // Применение калибровок
    float calibrated_voltage = raw_voltage + (voltage_offset / 1000.0); // мВ -> В
    double calibrated_current = raw_current + (current_offset / 1000.0); // мА -> А

    // Фильтр для стабилизации измерений
    filtered_voltage = ALPHA * calibrated_voltage + (1 - ALPHA) * filtered_voltage;
    filtered_current = ALPHA * calibrated_current + (1 - ALPHA) * filtered_current;

    // Компенсация сопротивления проводов (используем отфильтрованные значения)
    measured_voltage = filtered_voltage - filtered_current * (wire_resistance / 1000.0);
    measured_current = filtered_current;

    measured_power = measured_voltage * measured_current;
  } else {
    // Эмуляция для отладки
    static float sim_voltage = 12.0;

    if (current_mode == MODE_CC) {
      measured_current = target_current * 0.95 + random(-5, 5) * 0.001;
    } else if (current_mode == MODE_DISCHARGE && discharge_active) {
      measured_current = setpoint_current * 0.95 + random(-5, 5) * 0.001;
    } else if (current_mode == MODE_OFF) {
      measured_current = 0.0;
    } else {
      measured_current = 0.5 + random(-10, 10) * 0.001;
    }

    // Симуляция падения напряжения при разряде
    if (current_mode == MODE_DISCHARGE && discharge_active) {
      float discharge_time = (millis() - discharge_start_time) / 1000.0 / 3600.0; // часы
      sim_voltage = 12.0 - discharge_time * 0.5; // Линейное падение
    } else {
      sim_voltage = 12.0 - measured_current * 0.1;
    }

    measured_voltage = sim_voltage + random(-5, 5) * 0.01;
    measured_power = measured_voltage * measured_current;
  }
}

void updateLoadControl() {
  if (!system_enabled || current_mode == MODE_OFF) {
    analogWrite(MOSFET_PIN, 0);
    return;
  }

  switch(current_mode) {
    case MODE_CC: {
      // Режим постоянного тока (простое регулирование)
      setpoint_current = target_current;
      // Простое П-регулирование тока (максимальная точность)
      float error = setpoint_current - measured_current;
      pwm_output += error * 2.0;  // Минимальный шаг для высокой точности
      pwm_output = constrain(pwm_output, 0, 1023);
      analogWrite(MOSFET_PIN, (int)pwm_output);
      break;
    }
    case MODE_DISCHARGE: {
      // Режим разряда (простое регулирование)
      if (discharge_active) {
        setpoint_current = settings.discharge_current;
        // Простое П-регулирование тока
        float error = setpoint_current - measured_current;
        pwm_output += error * 2.0;  // Минимальный шаг для высокой точности
        pwm_output = constrain(pwm_output, 0, 1023);
        analogWrite(MOSFET_PIN, (int)pwm_output);
      } else {
        analogWrite(MOSFET_PIN, 0);
      }
      break;
    }
      
    case MODE_CP:
      // Режим постоянной мощности (простой П-регулятор)
      if (measured_voltage > 0.1) {
        float target_current_cp = target_power / measured_voltage;
        target_current_cp = constrain(target_current_cp, 0, settings.max_current);
        
        // Простое управление
        float error = target_current_cp - measured_current;
        pwm_output += error * 10.0;
        pwm_output = constrain(pwm_output, 0, 1023);
        analogWrite(MOSFET_PIN, (int)pwm_output);
      }
      break;
      
    case MODE_CR:
      // Режим постоянного сопротивления
      if (measured_voltage > 0.1) {
        float target_current_cr = measured_voltage / target_resistance;
        target_current_cr = constrain(target_current_cr, 0, settings.max_current);

        // Простое управление
        float error = target_current_cr - measured_current;
        pwm_output += error * 10.0;
        pwm_output = constrain(pwm_output, 0, 1023);
        analogWrite(MOSFET_PIN, (int)pwm_output);
      }
      break;

    case MODE_CALIBRATE:
      // Режим калибровки - PWM не перезаписывается
      break;

    default:
      analogWrite(MOSFET_PIN, 0);
      break;
  }
}

void checkProtection() {
  bool fault_detected = false;
  String fault_message = "";

  if (measured_current > settings.max_current * 1.1) {
    fault_detected = true;
    fault_message = "Overcurrent: " + String(measured_current, 2) + "A";
  }

  if (measured_voltage > settings.max_voltage) {
    fault_detected = true;
    fault_message = "Overvoltage: " + String(measured_voltage, 2) + "V";
  }



  if (fault_detected) {
    emergencyStop();
    Serial.println("PROTECTION: " + fault_message);
  }
}

void emergencyStop() {
  current_mode = MODE_OFF;
  discharge_active = false;
  analogWrite(MOSFET_PIN, 0);
  system_enabled = false;

  Serial.println("EMERGENCY STOP ACTIVATED");
}

void sendToLogger() {
  if (logger_ip == IPAddress(0,0,0,0)) {
    return;
  }
  
  WiFiClient client;
  if (client.connect(logger_ip, 80)) {
  String data = "device=" + device_name;
  data += "&voltage=" + String(measured_voltage, 3);
  data += "&current=" + String(measured_current, 3);
  data += "&power=" + String(measured_power, 2);
  data += "&charge=" + String(accumulated_charge, 3);
  data += "&energy=" + String(accumulated_energy, 2);
  data += "&mode=" + String(current_mode);
  data += "&active=" + String(discharge_active);
  data += "&cutoff_voltage=" + String(cutoff_voltage, 2);
  data += "&discharge_current=" + String(discharge_current, 3);
    
    client.println("POST /log HTTP/1.1");
    client.println("Host: " + logger_ip.toString());
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.println(data);
    
    client.stop();
    Serial.println("Data sent to logger");
  }
}

void setupWiFi() {
  Serial.print("Device name: ");
  Serial.println(device_name);
  
  WiFi.hostname(device_name);
  
  // Попытка подключения к домашней сети
  if (home_ssid.length() > 0) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(home_ssid);
    
    WiFi.mode(WIFI_STA);
    // Использовать DHCP для автоматического назначения IP
    WiFi.begin(home_ssid.c_str(), home_password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      return;
    }
  }
  
  // Если не удалось подключиться, запускаем точку доступа
  Serial.println("\nWiFi connection failed, starting AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_ip, ap_ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(device_name.c_str(), ap_password.c_str());
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
  // Основная страница со всеми вкладками
  server.on("/", handleRoot);

  // API endpoints
  server.on("/api/status", handleApiStatus);
  server.on("/api/discharge/status", handleApiDischargeStatus);
  server.on("/api/control", HTTP_GET, handleApiControl);
  server.on("/api/discharge/control", HTTP_GET, handleApiDischargeControl);
  server.on("/api/settings", HTTP_GET, handleApiSettings);
  server.on("/api/settings/save", HTTP_POST, handleApiSaveSettings);
  server.on("/api/calibrate/save", HTTP_POST, handleApiSaveCalibration);
  server.on("/api/calibrate", HTTP_GET, handleApiGetCalibration);
  server.on("/api/calibrate/zero", handleApiCalibrate);
  server.on("/api/calibrate/ina226", handleApiIna226Registers);
  server.on("/api/reset", handleApiReset);
  server.on("/api/wifi/scan", handleApiScanNetworks);
  server.on("/api/wifi/connect", HTTP_POST, handleApiConnectToWifi);
  server.on("/api/wifi/status", handleApiWifiStatus);
  server.on("/api/mosfet/open", handleApiMosfetOpen);
  server.on("/api/mosfet/close", handleApiMosfetClose);

  server.onNotFound(handleNotFound);

  // Настройка OTA через веб
  httpUpdater.setup(&server);

  server.begin();
  Serial.println("HTTP server started on port 80");
}



// ============================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================

String formatTime(unsigned long seconds) {
  unsigned long days = seconds / 86400;
  unsigned long hours = (seconds % 86400) / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;
  
  String result = "";
  if (days > 0) result += String(days) + "д : ";
  if (hours > 0 || days > 0) result += String(hours) + "ч : ";
  result += String(minutes) + "м : " + String(secs) + "с";
  return result;
}



void loadSettings() {
  EEPROM.get(0, settings);
  
  // Проверка контрольной суммы
  uint32_t saved_crc = settings.crc;
  settings.crc = 0;
  uint32_t calculated_crc = calculateCRC32((uint8_t*)&settings, sizeof(Settings));
  
  if (saved_crc != calculated_crc) {
    // Загрузка значений по умолчанию
    Serial.println("Invalid CRC, loading default settings");
    
    strcpy(settings.device_name, "Nagruzka");
    strcpy(settings.home_ssid, "");
    strcpy(settings.home_password, "");
    
    // Калибровка
    settings.shunt_resistance = DEFAULT_SHUNT_RESISTOR;
    
    // Защиты
    settings.max_current = DEFAULT_MAX_CURRENT;
    settings.max_voltage = DEFAULT_MAX_VOLTAGE;
    
    // Разряд
    settings.cutoff_voltage = 10.5;
    settings.discharge_current = 1.0;
    settings.capacity_limit = 0.0;
    
    saveSettings();
  } else {
    Serial.println("Settings loaded from EEPROM");
    
    // Загрузка значений в переменные
    device_name = String(settings.device_name);
    home_ssid = String(settings.home_ssid);
    home_password = String(settings.home_password);
    
    shunt_resistance = settings.shunt_resistance;
    voltage_offset = 0.0;
    current_offset = 0.0;
    wire_resistance = 0.0;
    battery_cells = 6;
    
    cutoff_voltage = settings.cutoff_voltage;
    discharge_current = settings.discharge_current;
    capacity_limit = settings.capacity_limit;
  }
}

void saveSettings() {
  // Обновление полей структуры из переменных
  device_name.toCharArray(settings.device_name, 32);
  home_ssid.toCharArray(settings.home_ssid, 32);
  home_password.toCharArray(settings.home_password, 32);
  
  settings.shunt_resistance = shunt_resistance;
  
  settings.cutoff_voltage = cutoff_voltage;
  settings.discharge_current = discharge_current;
  settings.capacity_limit = capacity_limit;
  
  // Расчет контрольной суммы
  settings.crc = 0;
  settings.crc = calculateCRC32((uint8_t*)&settings, sizeof(Settings));
  
  EEPROM.put(0, settings);
  EEPROM.commit();
  
  Serial.println("Settings saved to EEPROM");
}

uint32_t calculateCRC32(uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  
  while (length--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc >> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  
  return ~crc;
}
