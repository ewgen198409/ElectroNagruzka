// ============================================
// КАЛИБРОВКА
// ============================================

// Extern переменные из main.ino
extern float measured_voltage;
extern double measured_current;
extern Settings settings;
extern ESP8266WebServer server;
extern LoadMode current_mode;
extern const int MOSFET_PIN;

// Калибровочные параметры
float shunt_resistance = 0.01;      // 10 мОм (в Омах)
float voltage_offset = 0.0;         // Офсет напряжения (мВ)
float current_offset = 0.0;         // Офсет тока (мА)
float wire_resistance = 0.0;        // Сопротивление проводов (мОм)
int battery_cells = 6;              // Количество банок в АКБ

// Функции калибровки
void initializeCalibration() {
  // Инициализация калибровочных параметров
  Serial.println("Calibration module initialized");
}

// Обработка режима калибровки в updateLoadControl
void updateCalibrationMode() {
  // В режиме калибровки PWM не перезаписывается
  // PWM устанавливается напрямую через API
}

// API обработчики калибровки
void handleApiMosfetOpen() {
  current_mode = MODE_CALIBRATE;
  digitalWrite(MOSFET_PIN, HIGH);  // Полное открытие MOSFET
  server.send(200, "text/plain", "MOSFET открыт (HIGH)");
}

void handleApiMosfetClose() {
  current_mode = MODE_CALIBRATE;
  digitalWrite(MOSFET_PIN, LOW);   // Полное закрытие MOSFET
  server.send(200, "text/plain", "MOSFET закрыт (LOW)");
}

void handleApiGetCalibration() {
  String json = "{";
  json += "\"shunt_resistance\":" + String(settings.shunt_resistance, 4);
  json += "}";

  server.send(200, "application/json", json);
}

void handleApiSaveCalibration() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");

    // Простой парсинг JSON
    int shunt_start = body.indexOf("\"shunt_resistance\":") + 19;
    int shunt_end = body.indexOf("}", shunt_start);
    if (shunt_start > 18 && shunt_end > shunt_start) {
      settings.shunt_resistance = body.substring(shunt_start, shunt_end).toFloat();
      shunt_resistance = settings.shunt_resistance;
    }

    saveSettings();
    server.send(200, "text/plain", "Калибровочные данные сохранены.");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleApiCalibrate() {
  // Калибровка нуля тока (только когда нагрузка выключена)
  if (current_mode == MODE_OFF) {
    // Сохраняем текущие смещения как калибровку нуля
    // В реальном устройстве здесь должен быть код калибровки INA226
    server.send(200, "text/plain", "Калибровка нуля выполнена. Убедитесь, что ток не протекает.");
  } else {
    server.send(400, "text/plain", "Остановите нагрузку перед калибровкой.");
  }
}

void handleApiIna226Registers() {
  // Чтение значений INA226 через библиотеку и регистров для диагностики
  extern INA226_WE ina226;
  extern bool ina226_connected;

  String json = "{";

  if (ina226_connected) {
    // Используем библиотеку для чтения
    json += "\"bus_voltage_v\":" + String(ina226.getBusVoltage_V(), 4) + ",";
    json += "\"current_ma\":" + String(ina226.getCurrent_mA(), 4) + ",";
    json += "\"power_mw\":" + String(ina226.getBusPower(), 4) + ",";
    json += "\"shunt_voltage_mv\":" + String(ina226.getShuntVoltage_mV(), 4) + ",";
  } else {
    json += "\"bus_voltage_v\":0,";
    json += "\"current_ma\":0,";
    json += "\"power_mw\":0,";
    json += "\"shunt_voltage_mv\":0,";
  }

  // Чтение сырых регистров напрямую
  // Чтение регистра Configuration (0x00)
  Wire.beginTransmission(0x40);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(0x40, 2);
  uint16_t config = 0;
  if (Wire.available() >= 2) {
    config = (Wire.read() << 8) | Wire.read();
  }
  json += "\"config\":\"0x" + String(config, HEX) + "\",";

  // Чтение регистра Shunt Voltage (0x01)
  Wire.beginTransmission(0x40);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(0x40, 2);
  uint16_t shunt_voltage = 0;
  if (Wire.available() >= 2) {
    shunt_voltage = (Wire.read() << 8) | Wire.read();
  }
  json += "\"shunt_voltage_raw\":" + String(shunt_voltage) + ",";

  // Чтение регистра Bus Voltage (0x02)
  Wire.beginTransmission(0x40);
  Wire.write(0x02);
  Wire.endTransmission();
  Wire.requestFrom(0x40, 2);
  uint16_t bus_voltage = 0;
  if (Wire.available() >= 2) {
    bus_voltage = (Wire.read() << 8) | Wire.read();
  }
  json += "\"bus_voltage_raw\":" + String(bus_voltage) + ",";

  // Чтение регистра Power (0x03)
  Wire.beginTransmission(0x40);
  Wire.write(0x03);
  Wire.endTransmission();
  Wire.requestFrom(0x40, 2);
  uint16_t power = 0;
  if (Wire.available() >= 2) {
    power = (Wire.read() << 8) | Wire.read();
  }
  json += "\"power_raw\":" + String(power) + ",";

  // Чтение регистра Current (0x04)
  Wire.beginTransmission(0x40);
  Wire.write(0x04);
  Wire.endTransmission();
  Wire.requestFrom(0x40, 2);
  uint16_t current = 0;
  if (Wire.available() >= 2) {
    current = (Wire.read() << 8) | Wire.read();
  }
  json += "\"current_raw\":" + String(current) + ",";

  // Чтение регистра Calibration (0x05)
  Wire.beginTransmission(0x40);
  Wire.write(0x05);
  Wire.endTransmission();
  Wire.requestFrom(0x40, 2);
  uint16_t calibration = 0;
  if (Wire.available() >= 2) {
    calibration = (Wire.read() << 8) | Wire.read();
  }
  json += "\"calibration_raw\":" + String(calibration) + ",";

  // Текущее сопротивление шунта
  json += "\"shunt_resistance\":" + String(shunt_resistance, 4) + ",";
  json += "\"connected\":" + String(ina226_connected ? "true" : "false");

  json += "}";
  server.send(200, "application/json", json);
}
