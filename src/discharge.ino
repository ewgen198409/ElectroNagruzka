// ============================================
// РАЗРЯД АККУМУЛЯТОРОВ
// ============================================

// Extern переменные и функции из main.ino
extern float measured_voltage;
extern double measured_current;
extern float measured_power;
extern LoadMode current_mode;
extern Settings settings;
extern double setpoint_current;
extern ESP8266WebServer server;
extern String formatTime(unsigned long seconds);
extern void saveSettings();
extern const int MOSFET_PIN;

// Extern переменные из calib.ino
extern int battery_cells;

// Переменные для разряда аккумулятора
float cutoff_voltage = 10.5;         // Напряжение окончания разряда
float discharge_current = 1.0;       // Ток разряда (А)
float capacity_limit = 0.0;          // Ограничение по емкости (Ач), 0 - не используется
float accumulated_charge = 0.0;      // Накопленный заряд (Ач) за сессию
float accumulated_energy = 0.0;      // Накопленная энергия (Вт*ч)
bool discharge_active = false;       // Флаг активного разряда
unsigned long discharge_start_time = 0; // Время начала разряда

// Функции разряда
void initializeDischarge() {
  // Инициализация параметров разряда
  Serial.println("Discharge module initialized");
}

void updateDischargeControl() {
  if (!discharge_active || current_mode != MODE_DISCHARGE) {
    return;
  }

  // Проверка напряжения окончания разряда
  if (measured_voltage <= settings.cutoff_voltage) {
    Serial.println("Разряд завершен: достигнуто напряжение окончания");
    stopDischarge();
    return;
  }

  // Проверка ограничения по емкости
  if (settings.capacity_limit > 0 && accumulated_charge >= settings.capacity_limit) {
    Serial.println("Разряд завершен: достигнуто ограничение по емкости");
    stopDischarge();
    return;
  }

  // Поддержание тока разряда - setpoint обновляется постоянно для корректной работы PID
  setpoint_current = settings.discharge_current;
}

void stopDischarge() {
  discharge_active = false;
  current_mode = MODE_OFF;
  analogWrite(MOSFET_PIN, 0);
  Serial.println("Discharge stopped");
}

void resetDischargeSession() {
  accumulated_charge = 0.0;
  accumulated_energy = 0.0;
  discharge_start_time = 0;
  Serial.println("Discharge session reset");
}

float calculatePercentCapacity(float capacity_c_rate) {
  if (capacity_c_rate <= 0) return 0.0;
  return (accumulated_charge / capacity_c_rate) * 100.0;
}

String formatDischargeTime() {
  if (!discharge_active || discharge_start_time == 0) {
    return "0д : 0ч : 0м : 0с";
  }

  unsigned long discharge_seconds = (millis() - discharge_start_time) / 1000;
  return formatTime(discharge_seconds);
}

// API обработчики разряда
void handleApiDischargeStatus() {
  String json = "{";
  json += "\"voltage\":" + String(measured_voltage, 3) + ",";
  json += "\"current\":" + String(measured_current, 3) + ",";
  json += "\"power\":" + String(measured_power, 3) + ",";
  json += "\"capacity\":" + String(accumulated_charge, 3) + ",";
  json += "\"energy\":" + String(accumulated_energy, 2) + ",";
  json += "\"mode\":" + String(current_mode) + ",";
  json += "\"active\":" + String(discharge_active ? "true" : "false") + ",";
  json += "\"cutoff_voltage\":" + String(settings.cutoff_voltage, 2) + ",";
  json += "\"discharge_current\":" + String(settings.discharge_current, 3) + ",";
  json += "\"capacity_limit\":" + String(settings.capacity_limit, 1) + ",";
  json += "\"battery_cells\":" + String(battery_cells) + ",";

  // Время разряда
  unsigned long discharge_seconds = 0;
  if (discharge_active && discharge_start_time > 0) {
    discharge_seconds = (millis() - discharge_start_time) / 1000;
  }
  json += "\"discharge_time\":\"" + formatTime(discharge_seconds) + "\",";

  // Проценты емкости
  float percent_c10 = calculatePercentCapacity(10.0);
  float percent_c20 = calculatePercentCapacity(20.0);
  json += "\"percent_c10\":" + String(percent_c10, 2) + ",";
  json += "\"percent_c20\":" + String(percent_c20, 2);
  json += "}";

  server.send(200, "application/json", json);
}

void handleApiDischargeControl() {
  if (server.hasArg("action")) {
    String action = server.arg("action");

    if (action == "start") {
      // Начать новый разряд
      if (current_mode != MODE_DISCHARGE) {
        current_mode = MODE_DISCHARGE;
      }
      discharge_active = true;
      discharge_start_time = millis();
      setpoint_current = settings.discharge_current;
      server.send(200, "text/plain", "Разряд начат");

    } else if (action == "stop") {
      // Остановить разряд
      stopDischarge();
      server.send(200, "text/plain", "Разряд остановлен");

    } else if (action == "continue") {
      // Продолжить разряд
      if (current_mode != MODE_DISCHARGE) {
        current_mode = MODE_DISCHARGE;
      }
      discharge_active = true;
      if (discharge_start_time == 0) {
        discharge_start_time = millis();
      }
      setpoint_current = settings.discharge_current;
      server.send(200, "text/plain", "Разряд продолжен");

    } else if (action == "reset") {
      // Сбросить сессию
      resetDischargeSession();
      server.send(200, "text/plain", "Сессия сброшена");
    }
  }

  if (server.hasArg("cutoff")) {
    settings.cutoff_voltage = server.arg("cutoff").toFloat();
    cutoff_voltage = settings.cutoff_voltage;
    saveSettings();
    server.send(200, "text/plain", "Напряжение окончания: " + String(cutoff_voltage) + "V");
  }

  if (server.hasArg("current")) {
    settings.discharge_current = server.arg("current").toFloat();
    discharge_current = settings.discharge_current;
    if (current_mode == MODE_DISCHARGE) {
      setpoint_current = discharge_current;
    }
    saveSettings();
    server.send(200, "text/plain", "Ток разряда: " + String(discharge_current) + "A");
  }

  if (server.hasArg("capacity")) {
    settings.capacity_limit = server.arg("capacity").toFloat();
    capacity_limit = settings.capacity_limit;
    saveSettings();
    server.send(200, "text/plain", "Ограничение по емкости: " + String(capacity_limit) + "Ah");
  }
}
