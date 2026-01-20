// ============================================
// ВЕБ-СТРАНИЦЫ
// ============================================

// Extern переменные из main.ino
extern String device_name;
extern String home_ssid;
extern String home_password;

extern const String ap_password;
extern const IPAddress ap_ip;
extern int battery_cells;
extern unsigned long operation_time;
extern float measured_voltage;
extern double measured_current;
extern float measured_power;
extern LoadMode current_mode;
extern bool discharge_active;
extern bool system_enabled;
extern bool ina226_connected;
extern float accumulated_charge;
extern float accumulated_energy;
extern Settings settings;
extern float cutoff_voltage;
extern float discharge_current;
extern unsigned long discharge_start_time;

// Extern функции
extern String formatTime(unsigned long seconds);

// Используем F() для хранения строк в flash без PROGMEM массивов

void handleRoot() {
  // Отправка статических частей без загрузки в RAM
  server.sendContent(F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Электронная нагрузка</title><style>body{font-family:Arial,sans-serif;margin:10px;background:#f5f5f5;}.container{max-width:900px;margin:0 auto;background:white;padding:15px;border-radius:5px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}h1,h2,h3{margin:10px 0;}.header{background:#2c3e50;color:white;padding:10px;border-radius:5px;margin-bottom:15px;position:relative;}.version{position:absolute;bottom:5px;right:10px;font-size:12px;color:#bdc3c7;}.wifi-icon{position:absolute;top:5px;right:10px;font-size:18px;color:#27ae60;display:none;background:none;}.battery-icon{position:absolute;top:5px;right:35px;font-size:18px;color:#e74c3c;display:none;}@keyframes blink{0%{opacity:1;}50%{opacity:0.3;}100%{opacity:1;}}.battery-blink{animation:blink 1s infinite;}.tabs{display:flex;margin:10px 0;border-bottom:1px solid #ddd;}.tab{padding:10px 15px;cursor:pointer;background:#ecf0f1;border-radius:5px 5px 0 0;margin-right:5px;}.tab.active{background:#3498db;color:white;}.tab-content{display:none;padding:15px;border:1px solid #ddd;border-top:none;}.tab-content.active{display:block;}.button{padding:8px 16px;margin:5px;border:none;border-radius:3px;cursor:pointer;background:#3498db;color:white;}.btn-start{background:#27ae60;}.btn-stop{background:#e74c3c;}.param-table{width:100%;border-collapse:collapse;margin:10px 0;}.param-table th,.param-table td{padding:8px;text-align:left;border:1px solid #ddd;}.param-value{font-weight:bold;color:#2980b9;}.slider{width:100%;}.live-value{background:#f0f0f0;padding:2px 6px;border-radius:10px;font-size:12px;color:#333;margin-left:10px;display:none;}.input{width:100%;padding:5px;margin:5px 0;border:1px solid #ddd;border-radius:3px;}.progress-bar{background:#f0f0f0;border-radius:5px;height:20px;margin:5px 0;}.progress-fill{height:100%;border-radius:5px;background:#3498db;}</style><script>let updateInterval;function showTab(t){document.querySelectorAll('.tab').forEach(e=>e.classList.remove('active'));document.querySelectorAll('.tab-content').forEach(e=>e.classList.remove('active'));document.getElementById('tab-'+t).classList.add('active');document.getElementById(t).classList.add('active');if(t==='calibrate'){loadCalibrationData();updateCalData();}}function updateData(){fetch('/api/discharge/status').then(r=>r.json()).then(d=>{document.getElementById('voltage').textContent=d.voltage.toFixed(3);document.getElementById('current').textContent=d.current.toFixed(3);document.getElementById('power').textContent=d.power.toFixed(3);document.getElementById('capacity').textContent=d.capacity.toFixed(3);document.getElementById('energy').textContent=d.energy.toFixed(2);const s=document.getElementById('status');s.textContent=d.active?'Идет разряд':'Остановлен';s.style.color=d.active?'#27ae60':'#e74c3c';document.getElementById('discharge-time').textContent=d.discharge_time;document.getElementById('cutoff-value').textContent=d.cutoff_voltage.toFixed(2);document.getElementById('current-value').textContent=d.discharge_current.toFixed(3);document.getElementById('capacity-value').textContent=d.capacity_limit.toFixed(1);if(d.active){document.getElementById('battery-icon').style.display='block';document.getElementById('battery-icon').classList.add('battery-blink');}else{document.getElementById('battery-icon').style.display='none';document.getElementById('battery-icon').classList.remove('battery-blink');}}).catch(e=>console.error(e));}function startDischarge(){fetch('/api/discharge/control?action=start').then(()=>updateData());}function stopDischarge(){fetch('/api/discharge/control?action=stop').then(()=>updateData());}function resetDischarge(){if(confirm('Сбросить данные?'))fetch('/api/discharge/control?action=reset').then(()=>updateData());}function updateCutoff(v){fetch('/api/discharge/control?cutoff='+(v/10)).then(()=>updateData());}function updateCurrent(v){fetch('/api/discharge/control?current='+(v/1000)).then(()=>updateData());}function updateCapacity(v){fetch('/api/discharge/control?capacity='+(v/10)).then(()=>updateData());}function updateCutoffLive(v){const e=document.getElementById('cutoff-live');e.textContent=(v/10).toFixed(1);e.style.display='inline';}function updateCurrentLive(v){const e=document.getElementById('current-live');e.textContent=(v/1000).toFixed(3);e.style.display='inline';}function updateCapacityLive(v){const e=document.getElementById('capacity-live');e.textContent=(v/10).toFixed(1);e.style.display='inline';}function hideCutoffLive(){document.getElementById('cutoff-live').style.display='none';}function hideCurrentLive(){document.getElementById('current-live').style.display='none';}function hideCapacityLive(){document.getElementById('capacity-live').style.display='none';}function scanWifiNetworks(){document.getElementById('wifi-networks').innerHTML='<option value=\"\">Сканирование...</option>';fetch('/api/wifi/scan').then(r=>r.json()).then(n=>{const s=document.getElementById('wifi-networks');s.innerHTML='<option value=\"\">Выберите сеть</option>';n.forEach(e=>{const o=document.createElement('option');o.value=e.ssid;o.textContent=e.ssid+' ('+e.rssi+' dBm)';s.appendChild(o);});}).catch(e=>console.error('Ошибка сканирования Wi-Fi:',e));}function connectToWifi(){const s=document.getElementById('wifi-networks').value;if(!s){alert('Пожалуйста, выберите сеть');return;}const p=prompt('Введите пароль для '+s+' (оставьте пустым, если нет):');if(p===null)return;const f=new FormData();f.append('ssid',s);f.append('password',p);fetch('/api/wifi/connect',{method:'POST',body:f}).then(r=>r.text()).then(d=>alert(d)).catch(e=>console.error('Ошибка подключения Wi-Fi:',e));}function loadCalibrationData(){fetch('/api/calibrate').then(r=>r.json()).then(d=>{document.getElementById('shunt').value=d.shunt_resistance.toFixed(4);}).catch(e=>console.error('Ошибка загрузки калибровочных данных:',e));}function saveCalibration(){const d={shunt_resistance:parseFloat(document.getElementById('shunt').value)};fetch('/api/calibrate/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(r=>r.text()).then(d=>alert(d));}function updateWifiInfo(){fetch('/api/wifi/status').then(r=>r.json()).then(d=>{let h='<table class=\"param-table\"><tr><th>Параметр</th><th>Значение</th></tr>';h+='<tr><td>Режим</td><td>'+d.mode+'</td></tr>';if(d.connected){if(d.mode==='STA'){h+='<tr><td>SSID</td><td>'+d.ssid+'</td></tr><tr><td>IP адрес</td><td>'+d.ip+'</td></tr>';h+='<tr><td>Шлюз</td><td>'+d.gateway+'</td></tr><tr><td>Маска подсети</td><td>'+d.subnet+'</td></tr>';h+='<tr><td>DNS сервер</td><td>'+d.dns+'</td></tr><tr><td>MAC адрес</td><td>'+d.mac+'</td></tr><tr><td>Уровень сигнала</td><td>'+d.rssi+' dBm</td></tr>';}else if(d.mode==='AP'){h+='<tr><td>SSID точки доступа</td><td>'+d.ssid+'</td></tr><tr><td>IP адрес</td><td>'+d.ip+'</td></tr><tr><td>MAC адрес</td><td>'+d.mac+'</td></tr><tr><td>Подключенных клиентов</td><td>'+d.clients+'</td></tr>';}}else{h+='<tr><td>Статус</td><td>Не подключено</td></tr>';}h+='</table>';document.getElementById('wifi-info').innerHTML=h;if(d.mode==='STA' && d.connected){document.getElementById('wifi-icon').style.display='block';}else{document.getElementById('wifi-icon').style.display='none';}}).catch(e=>{document.getElementById('wifi-info').innerHTML='Ошибка загрузки информации: '+e.message;});}function openMosfet(){fetch('/api/mosfet/open');}function closeMosfet(){fetch('/api/mosfet/close');}function restartDevice(){if(confirm('Перезагрузить устройство?')){fetch('/api/reset').then(r=>r.text()).then(d=>{alert(d);setTimeout(()=>{window.location.reload();},1000);});}}function updateCalData(){fetch('/api/status').then(r=>r.json()).then(d=>{document.getElementById('cal-voltage').textContent=d.voltage.toFixed(3);document.getElementById('cal-current').textContent=d.current.toFixed(3);});}function checkINA226(){fetch('/api/calibrate/ina226').then(r=>r.json()).then(d=>{let info='Connected: '+d.connected+'<br>';if(d.connected==='true'){info+='Bus Voltage: '+d.bus_voltage_v+' V<br>';info+='Current: '+d.current_ma+' mA<br>';info+='Power: '+d.power_mw+' mW<br>';info+='Shunt Voltage: '+d.shunt_voltage_mv+' mV<br>';}info+='Config: '+d.config+'<br>';info+='Shunt Voltage Raw: '+d.shunt_voltage_raw+'<br>';info+='Bus Voltage Raw: '+d.bus_voltage_raw+'<br>';info+='Power Raw: '+d.power_raw+'<br>';info+='Current Raw: '+d.current_raw+'<br>';info+='Calibration Raw: '+d.calibration_raw+'<br>';info+='Shunt Resistance: '+d.shunt_resistance+' Ohm<br>';document.getElementById('ina226-info').innerHTML=info;}).catch(e=>{document.getElementById('ina226-info').innerHTML='Ошибка чтения INA226: '+e.message;});}window.onload=function(){showTab('main');updateData();updateInterval=setInterval(updateData,2000);updateWifiInfo();setInterval(updateCalData,1000);};</script></head>"));
  // Отправка динамической части по частям
  server.sendContent("<body><div class='container'>");
  server.sendContent("<div class='header'><h1>Электронная нагрузка</h1><p>Устройство: ");
  server.sendContent(device_name);
  server.sendContent("</p><div class='version'>Версия: ");
  server.sendContent(String(FIRMWARE_VERSION));
  server.sendContent("</div><div class='wifi-icon' id='wifi-icon'>📶</div><div class='battery-icon' id='battery-icon'>🔋</div></div>");
  server.sendContent("<div class='tabs'>");
  server.sendContent("<div class='tab active' id='tab-main' onclick='showTab(\"main\")'>Главная</div>");
  server.sendContent("<div class='tab' id='tab-wifi' onclick='showTab(\"wifi\")'>Wi-Fi</div>");
  server.sendContent("<div class='tab' id='tab-calibrate' onclick='showTab(\"calibrate\")'>Калибровка</div>");
  server.sendContent("<div class='tab' id='tab-ota' onclick='showTab(\"ota\")'>OTA</div>");
  server.sendContent("<div class='tab' id='tab-system' onclick='showTab(\"system\")'>Система</div></div>");
  server.sendContent("<div id='main' class='tab-content active'><h2>Разряд АКБ</h2>");
  server.sendContent("<div><button class='button btn-start' onclick='startDischarge()'>Старт</button>");
  server.sendContent("<button class='button btn-stop' onclick='stopDischarge()'>Стоп</button>");
  server.sendContent("<button class='button' onclick='resetDischarge()'>Сброс</button></div>");
  server.sendContent("<table class='param-table'><tr><th>Параметр</th><th>Значение</th></tr>");
  server.sendContent("<tr><td>Напряжение</td><td><span id='voltage' class='param-value'>0.000</span> В</td></tr>");
  server.sendContent("<tr><td>Ток</td><td><span id='current' class='param-value'>0.000</span> А</td></tr>");
  server.sendContent("<tr><td>Мощность</td><td><span id='power' class='param-value'>0.000</span> Вт</td></tr>");
  server.sendContent("<tr><td>Статус</td><td id='status'>Остановлен</td></tr>");
  server.sendContent("<tr><td>Емкость</td><td><span id='capacity' class='param-value'>0.000</span> Ач</td></tr>");
  server.sendContent("<tr><td>Энергия</td><td><span id='energy' class='param-value'>0.00</span> ВтЧ</td></tr>");
  server.sendContent("<tr><td>Время</td><td id='discharge-time'>0с</td></tr></table><h3>Настройки разряда</h3>");
  server.sendContent("<p>Напряжение окончания: <span id='cutoff-value'>");
  server.sendContent(String(cutoff_voltage, 2));
  server.sendContent("</span> V <span id='cutoff-live' class='live-value'>");
  server.sendContent(String(cutoff_voltage, 1));
  server.sendContent("</span><input type='range' class='slider' id='cutoff-slider' min='0' max='150' ");
  server.sendContent("oninput='updateCutoffLive(this.value)' onmouseup='hideCutoffLive()' onchange='updateCutoff(this.value)' value='");
  server.sendContent(String(cutoff_voltage * 10));
  server.sendContent("'></p><p>Ток разряда: <span id='current-value'>");
  server.sendContent(String(discharge_current, 3));
  server.sendContent("</span> A <span id='current-live' class='live-value'>");
  server.sendContent(String(discharge_current, 3));
  server.sendContent("</span><input type='range' class='slider' id='current-slider' min='0' max='10000' ");
  server.sendContent("oninput='updateCurrentLive(this.value)' onmouseup='hideCurrentLive()' onchange='updateCurrent(this.value)' value='");
  server.sendContent(String(discharge_current * 1000));
  server.sendContent("'></p><p>Лимит емкости: <span id='capacity-value'>");
  server.sendContent(String(settings.capacity_limit, 1));
  server.sendContent("</span> Ач <span id='capacity-live' class='live-value'>");
  server.sendContent(String(settings.capacity_limit, 1));
  server.sendContent("</span><input type='range' class='slider' id='capacity-slider' min='0' max='1000' ");
  server.sendContent("oninput='updateCapacityLive(this.value)' onmouseup='hideCapacityLive()' onchange='updateCapacity(this.value)' value='");
  server.sendContent(String(settings.capacity_limit * 10));
  server.sendContent("'></p></div>");
  server.sendContent("<div id='wifi' class='tab-content'><h2>Информация о Wi-Fi подключении</h2>");
  server.sendContent("<div id='wifi-info'>Загрузка...</div><button class='button' onclick='updateWifiInfo()'>Обновить</button>");
  server.sendContent("<hr><h3>Управление подключением</h3><button class='button' onclick='scanWifiNetworks()'>Сканировать сети</button>");
  server.sendContent("<select class='input' id='wifi-networks'><option value=''>Выберите сеть</option></select>");
  server.sendContent("<button class='button' onclick='connectToWifi()'>Подключиться</button></div>");
  server.sendContent("<div id='calibrate' class='tab-content'><h2>Калибровка</h2>");
  server.sendContent("<p>Напряжение INA226: <span id='cal-voltage'>0.000</span> В</p>");
  server.sendContent("<p>Ток INA226: <span id='cal-current'>0.000</span> А</p>");
  server.sendContent("<button class='button' onclick='openMosfet()'>Открыть MOSFET</button>");
  server.sendContent("<button class='button btn-stop' onclick='closeMosfet()'>Закрыть MOSFET</button><hr>");
  server.sendContent("<p>Шунт: <input id='shunt' type='text' class='input' placeholder='0.01'> Ом</p>");
  server.sendContent("<button class='button' onclick='saveCalibration()'>Сохранить</button>");
  server.sendContent("<button class='button btn-stop' onclick='restartDevice()'>Перезагрузить устройство</button><hr>");
  server.sendContent("<button class='button' onclick='checkINA226()'>Проверить INA226</button>");
  server.sendContent("<div id='ina226-info' style='margin-top:10px; font-family:monospace; font-size:12px;'></div></div>");
  server.sendContent("<div id='ota' class='tab-content'><h2>OTA Обновление</h2>");
  server.sendContent("<form method='POST' action='/update' enctype='multipart/form-data'>");
  server.sendContent("<input type='file' name='update' accept='.bin'><button type='submit' class='button'>Загрузить и обновить</button></form>");
  server.sendContent("<p>Выберите файл прошивки (.bin) и нажмите загрузить.</p></div>");

  int ram_total = 81920;
  int ram_free = ESP.getFreeHeap();
  int ram_used = ram_total - ram_free;
  float ram_percent = (ram_used / (float)ram_total) * 100;

  int flash_total = ESP.getFlashChipSize();
  int flash_used = ESP.getSketchSize();
  float flash_percent = (flash_used / (float)flash_total) * 100;

  server.sendContent("<div id='system' class='tab-content'><h2>Система</h2><p>RAM: <div class='progress-bar'><div class='progress-fill' style='width:");
  server.sendContent(String(ram_percent));
  server.sendContent("%;'></div></div> ");
  server.sendContent(String(ram_used));
  server.sendContent("/");
  server.sendContent(String(ram_total));
  server.sendContent(" байт</p><p>ROM: <div class='progress-bar'><div class='progress-fill' style='width:");
  server.sendContent(String(flash_percent));
  server.sendContent("%;'></div></div> ");
  server.sendContent(String(flash_used));
  server.sendContent("/");
  server.sendContent(String(flash_total));
  server.sendContent(" байт</p><p>Время работы: ");
  server.sendContent(formatTime(operation_time));
  server.sendContent("</p><h3>Разделы флеш-памяти</h3><table class='param-table'><tr><th>Название</th><th>Тип</th><th>Смещение</th><th>Размер</th></tr><tr><td>nvs</td><td>data/nvs</td><td>0x9000</td><td>0x5000</td></tr><tr><td>otadata</td><td>data/ota</td><td>0xe000</td><td>0x2000</td></tr><tr><td>app0</td><td>app/ota_0</td><td>0x10000</td><td>0x2F0000</td></tr><tr><td>app1</td><td>app/ota_1</td><td>0x300000</td><td>0x2F0000</td></tr></table><h3>Настройки в EEPROM</h3><table class='param-table'><tr><th>Параметр</th><th>Значение</th></tr><tr><td>Имя устройства</td><td>");
  server.sendContent(String(settings.device_name));
  server.sendContent("</td></tr><tr><td>SSID Wi-Fi</td><td>");
  server.sendContent(String(settings.home_ssid));
  server.sendContent("</td></tr><tr><td>Пароль Wi-Fi</td><td>");
  server.sendContent(String(settings.home_password));
  server.sendContent("</td></tr><tr><td>Шунт (Ом)</td><td>");
  server.sendContent(String(settings.shunt_resistance, 4));
  server.sendContent("</td></tr><tr><td>Макс. ток (А)</td><td>");
  server.sendContent(String(settings.max_current, 1));
  server.sendContent("</td></tr><tr><td>Макс. напряжение (В)</td><td>");
  server.sendContent(String(settings.max_voltage, 1));
  server.sendContent("</td></tr><tr><td>Напряжение окончания (В)</td><td>");
  server.sendContent(String(settings.cutoff_voltage, 2));
  server.sendContent("</td></tr><tr><td>Ток разряда (А)</td><td>");
  server.sendContent(String(settings.discharge_current, 3));
  server.sendContent("</td></tr><tr><td>Лимит емкости (Ач)</td><td>");
  server.sendContent(String(settings.capacity_limit, 2));
  server.sendContent("</td></tr></table></div></div></body></html>");
}

void handleNotFound() {
  String message = "File Not Found\n\nURI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

// ============================================
// API ОБРАБОТЧИКИ
// ============================================

void handleApiStatus() {
  String json = "{";
  json += "\"voltage\":" + String(measured_voltage, 3) + ",";
  json += "\"current\":" + String(measured_current, 3) + ",";
  json += "\"power\":" + String(measured_power, 3) + ",";
  json += "\"mode\":" + String(current_mode) + ",";
  json += "\"active\":" + String(discharge_active ? "true" : "false") + ",";
  json += "\"system_enabled\":" + String(system_enabled ? "true" : "false") + ",";
  json += "\"uptime\":" + String(operation_time) + ",";
  json += "\"ina226_connected\":" + String(ina226_connected ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

void handleApiControl() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");

    if (mode == "off") {
      current_mode = MODE_OFF;
      discharge_active = false;
      analogWrite(MOSFET_PIN, 0);
      system_enabled = true;
      server.send(200, "text/plain", "Mode: OFF");

    } else if (mode == "cc") {
      current_mode = MODE_CC;
      if (server.hasArg("current")) {
        target_current = server.arg("current").toFloat();
        setpoint_current = target_current;
        server.send(200, "text/plain", "CC mode, target: " + String(target_current, 3) + "A");
      } else {
        server.send(200, "text/plain", "CC mode");
      }

    } else if (mode == "cp") {
      current_mode = MODE_CP;
      if (server.hasArg("power")) {
        target_power = server.arg("power").toFloat();
        server.send(200, "text/plain", "CP mode, target: " + String(target_power, 2) + "W");
      } else {
        server.send(200, "text/plain", "CP mode");
      }

    } else if (mode == "cr") {
      current_mode = MODE_CR;
      if (server.hasArg("resistance")) {
        target_resistance = server.arg("resistance").toFloat();
        server.send(200, "text/plain", "CR mode, target: " + String(target_resistance, 1) + "Ω");
      } else {
        server.send(200, "text/plain", "CR mode");
      }
    }

    if (server.hasArg("emergency") && server.arg("emergency") == "1") {
      emergencyStop();
      server.send(200, "text/plain", "Emergency stop activated");
    }
  } else {
    server.send(400, "text/plain", "Missing 'mode' parameter");
  }
}

void handleApiSettings() {
  String json = "{";
  json += "\"device_name\":\"" + String(settings.device_name) + "\",";
  json += "\"home_ssid\":\"" + String(settings.home_ssid) + "\",";
  json += "\"max_current\":" + String(settings.max_current, 1) + ",";
  json += "\"max_voltage\":" + String(settings.max_voltage, 1);
  json += "}";

  server.send(200, "application/json", json);
}

void handleApiSaveSettings() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("device_name")) {
      String new_name = server.arg("device_name");
      new_name.toCharArray(settings.device_name, 32);
      device_name = new_name;
    }

    if (server.hasArg("home_ssid")) {
      server.arg("home_ssid").toCharArray(settings.home_ssid, 32);
      home_ssid = server.arg("home_ssid");
    }

    if (server.hasArg("home_password")) {
      server.arg("home_password").toCharArray(settings.home_password, 32);
      home_password = server.arg("home_password");
    }

    saveSettings();
    server.send(200, "text/plain", "Настройки сохранены. Перезагрузите устройство для применения.");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleApiReset() {
  server.send(200, "text/plain", "Перезагрузка устройства...");
  delay(100);
  ESP.restart();
}

void handleApiScanNetworks() {
  String json = "[";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\", \"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleApiConnectToWifi() {
  if (server.method() == HTTP_POST && server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    String password = server.hasArg("password") ? server.arg("password") : "";

    WiFi.begin(ssid.c_str(), password.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      ssid.toCharArray(settings.home_ssid, 32);
      password.toCharArray(settings.home_password, 32);
      home_ssid = ssid;
      home_password = password;
      saveSettings();
      server.send(200, "text/plain", "Успешное подключение к " + ssid + ". IP: " + WiFi.localIP().toString());
    } else {
      server.send(500, "text/plain", "Не удалось подключиться к " + ssid);
    }
  } else {
    server.send(400, "text/plain", "Неверный запрос");
  }
}

void handleApiWifiStatus() {
  String json = "{";
  json += "\"mode\":\"" + String(WiFi.getMode() == WIFI_AP ? "AP" : WiFi.getMode() == WIFI_STA ? "STA" : "OFF") + "\",";

  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {
    json += "\"connected\":true,";
    json += "\"ssid\":\"" + WiFi.SSID() + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
    json += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
    json += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI());
  } else if (WiFi.getMode() == WIFI_AP) {
    json += "\"connected\":true,";
    json += "\"ssid\":\"" + WiFi.softAPSSID() + "\",";
    json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"mac\":\"" + WiFi.softAPmacAddress() + "\",";
    json += "\"clients\":" + String(WiFi.softAPgetStationNum());
  } else {
    json += "\"connected\":false";
  }

  json += "}";
  server.send(200, "application/json", json);
}
