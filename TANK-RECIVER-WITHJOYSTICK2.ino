// ═══════════════════════════════════════════════════════════════
//  GLYPH TANK — Joystick Firmware (Button Press Exit Logic)
// ═══════════════════════════════════════════════════════════════

#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ── PIN DEFINITIONS ──────────────────────────────────────────
const int joyX_Pin = 0;
const int joyY_Pin = 2;
// NEW: Define the pin for the Joystick Button (SW pin)
// Connect the "SW" pin on your joystick module to GPIO 15
const int joyButton_Pin = 6; 

// ── YOUR HOTSPOT CREDENTIALS ─────────────────────────────────
const char* WIFI_SSID = "your SSID";
const char* WIFI_PASS = "your password";

// ── JOYSTICK CALIBRATION ─────────────────────────────────────
const int X_MIN    = 0;
const int X_CENTER = 2126;
const int X_MAX    = 4095;
const int Y_MIN    = 0;
const int Y_CENTER = 2200;
const int Y_MAX    = 4095;
const int DEADZONE = 20;
const int CENTER_THRESHOLD = 300;

// ── DATA STRUCTURE ────────────────────────────────────────────
typedef struct struct_message {
  int leftMotor;
  int rightMotor;
} struct_message;

struct_message myData;

// ── STORAGE ──────────────────────────────────────────────────
Preferences prefs;

const int MAX_TANKS = 18;
const int COMMAND_ALL_ID = 99; // ID for "Control All"

struct TankEntry {
  String name;
  String mac;
};

TankEntry tanks[MAX_TANKS];
int       tankCount  = 0;
int       activeTank = -1; // -1=None, 99=All, 0-17=Specific

uint8_t   activeMacBytes[6] = {0};
bool      peerAdded  = false;
bool      configMode = false;

WebServer server(80);

// ── Button Exit State (Drive mode) ───────────────────────────
unsigned long buttonPressStart = 0;
bool          isButtonPressed  = false;
// Time required to hold button to exit (3 seconds)
const unsigned long EXIT_HOLD_TIME = 3000; 

// ═══════════════════════════════════════════════════════════════
//  JOYSTICK HELPER
// ═══════════════════════════════════════════════════════════════

bool isJoystickCentred() {
  int x = analogRead(joyX_Pin);
  int y = analogRead(joyY_Pin);
  return (abs(x - X_CENTER) < CENTER_THRESHOLD) &&
         (abs(y - Y_CENTER) < CENTER_THRESHOLD);
}

// ═══════════════════════════════════════════════════════════════
//  NVS
// ═══════════════════════════════════════════════════════════════

void saveToNVS() {
  prefs.begin("gt", false);
  prefs.putInt("count",  tankCount);
  prefs.putInt("active", activeTank);
  for (int i = 0; i < tankCount; i++) {
    prefs.putString(("n" + String(i)).c_str(), tanks[i].name);
    prefs.putString(("m" + String(i)).c_str(), tanks[i].mac);
  }
  prefs.end();
}

void loadFromNVS() {
  prefs.begin("gt", true);
  tankCount  = prefs.getInt("count",  0);
  activeTank = prefs.getInt("active", -1);
  for (int i = 0; i < tankCount; i++) {
    tanks[i].name = prefs.getString(("n" + String(i)).c_str(), "Tank " + String(i+1));
    tanks[i].mac  = prefs.getString(("m" + String(i)).c_str(), "");
  }
  prefs.end();
}

void setDriveFlag(bool drive) {
  prefs.begin("gt", false);
  prefs.putBool("drive", drive);
  prefs.end();
}

bool getDriveFlag() {
  prefs.begin("gt", true);
  bool d = prefs.getBool("drive", false);
  prefs.end();
  return d;
}

// ═══════════════════════════════════════════════════════════════
//  MAC + ESP-NOW
// ═══════════════════════════════════════════════════════════════

bool parseMac(const String& macStr, uint8_t* out) {
  if (macStr.length() != 17) return false;
  for (int i = 0; i < 6; i++)
    out[i] = (uint8_t) strtol(macStr.substring(i*3, i*3+2).c_str(), nullptr, 16);
  return true;
}

void removeCurrentPeer() {
  if (peerAdded) { esp_now_del_peer(activeMacBytes); peerAdded = false; }
}

bool addPeer(uint8_t* macBytes) {
  removeCurrentPeer();
  esp_now_peer_info_t pi = {};
  memcpy(pi.peer_addr, macBytes, 6);
  pi.channel = 0; pi.encrypt = false;
  if (esp_now_add_peer(&pi) != ESP_OK) return false;
  memcpy(activeMacBytes, macBytes, 6);
  peerAdded = true;
  return true;
}

void applyActiveTank() {
  if (activeTank == COMMAND_ALL_ID) {
    uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (addPeer(broadcastMac)) {
      Serial.println("BROADCAST MODE: Controlling ALL tanks.");
    }
    return;
  }

  if (activeTank < 0 || activeTank >= tankCount) return;
  uint8_t mac[6];
  if (!parseMac(tanks[activeTank].mac, mac)) return;
  if (addPeer(mac)) {
    Serial.println("Peer added: " + tanks[activeTank].name + " [" + tanks[activeTank].mac + "]");
  }
}

// ═══════════════════════════════════════════════════════════════
//  JOYSTICK MATH
// ═══════════════════════════════════════════════════════════════

int mapJoy(int raw, int minVal, int center, int maxVal) {
  if (raw < center) return map(raw, minVal, center, -255, 0);
  else              return map(raw, center, maxVal,    0, 255);
}

// ═══════════════════════════════════════════════════════════════
//  WEB PAGE
// ═══════════════════════════════════════════════════════════════

String buildPage(const String& message = "") {
  String html =
    "<!DOCTYPE html>"
    "<html lang='en'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>GlyphTank</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
    "background:#0b0f1a;color:#e2e8f0;min-height:100vh;"
    "display:flex;flex-direction:column;align-items:center;padding:24px 16px}"
    "h1{font-size:20px;font-weight:700;letter-spacing:3px;color:#f1f5f9;margin-bottom:4px}"
    ".sub{font-size:12px;color:#475569;margin-bottom:20px;letter-spacing:1px}"
    ".ip-box{background:#0f172a;border:1px solid #1d4ed8;border-radius:10px;"
    "padding:10px 16px;font-family:monospace;font-size:13px;color:#60a5fa;"
    "width:100%;max-width:480px;margin-bottom:16px;text-align:center}"
    ".card{background:#1e293b;border-radius:16px;padding:20px;"
    "width:100%;max-width:480px;margin-bottom:16px;border:1px solid #334155}"
    ".card h2{font-size:12px;font-weight:600;letter-spacing:1px;"
    "color:#64748b;margin-bottom:14px;text-transform:uppercase}"
    ".tank-row{display:flex;align-items:center;gap:8px;"
    "padding:10px 0;border-bottom:1px solid #1e3a5f}"
    ".tank-row:last-child{border-bottom:none}"
    ".tank-info{flex:1;min-width:0}"
    ".tank-name{font-size:14px;font-weight:600;color:#f1f5f9}"
    ".tank-mac{font-size:11px;color:#64748b;font-family:monospace;word-break:break-all}"
    ".active-badge{background:#1d4ed8;color:#bfdbfe;font-size:10px;"
    "font-weight:700;padding:2px 8px;border-radius:20px;white-space:nowrap}"
    ".btn{border:none;border-radius:8px;padding:8px 14px;"
    "font-size:12px;font-weight:600;cursor:pointer}"
    ".btn-select{background:#1d3a5f;color:#60a5fa}"
    ".btn-delete{background:#3b1a1a;color:#f87171}"
    ".btn-all{background:#4c1d95;color:#c4b5fd;width:100%;padding:14px;"
    "border-radius:10px;font-size:14px;border:none;cursor:pointer;font-weight:700;margin-bottom:16px}"
    ".btn-all:hover{background:#5b21b6}"
    ".btn-select:hover{background:#1d4ed8}"
    ".btn-delete:hover{background:#7f1d1d}"
    ".form-row{display:flex;flex-direction:column;gap:10px}"
    ".field-label{font-size:11px;font-weight:600;color:#64748b;"
    "text-transform:uppercase;letter-spacing:1px;margin-bottom:4px}"
    "input[type=text]{background:#0f172a;border:1px solid #334155;"
    "border-radius:8px;padding:10px 12px;color:#f1f5f9;"
    "font-size:13px;width:100%;font-family:monospace}"
    "input[type=text]:focus{outline:none;border-color:#3b82f6;background:#0a1020}"
    "input[type=text]::placeholder{color:#334155}"
    ".mac-seg{background:#0f172a;border:1px solid #334155;border-radius:6px;"
    "padding:10px 4px;color:#f1f5f9;font-size:15px;font-family:monospace;"
    "text-align:center;width:100%;text-transform:uppercase}"
    ".mac-seg:focus{outline:none;border-color:#3b82f6;background:#0a1020}"
    ".mac-sep{color:#475569;font-size:18px;font-weight:700;flex-shrink:0}"
    ".mac-row{display:flex;align-items:center;gap:4px}"
    ".btn-add{background:#14532d;color:#86efac;width:100%;"
    "padding:12px;font-size:13px;border-radius:10px;margin-top:4px;"
    "border:none;cursor:pointer;font-weight:600}"
    ".btn-add:hover{background:#166534}"
    ".msg{background:#1d3a2a;border:1px solid #166534;border-radius:8px;"
    "padding:10px 14px;color:#86efac;font-size:13px;margin-bottom:16px;"
    "width:100%;max-width:480px;text-align:center}"
    ".msg.err{background:#3b1a1a;border-color:#7f1d1d;color:#f87171}"
    ".hint{font-size:11px;color:#475569;margin-top:4px;line-height:1.5}"
    ".no-tanks{color:#475569;font-size:13px;text-align:center;padding:12px 0}"
    ".drive-btn{background:#1d4ed8;color:#fff;width:100%;padding:14px;"
    "border-radius:10px;font-size:14px;border:none;"
    "cursor:pointer;font-weight:700;margin-bottom:10px}"
    ".drive-btn:hover{background:#1e40af}"
    ".cfg-btn{background:#1e293b;color:#94a3b8;width:100%;padding:12px;"
    "border-radius:10px;font-size:13px;border:1px solid #334155;cursor:pointer;font-weight:600}"
    ".cfg-btn:hover{background:#0f172a;color:#e2e8f0}"
    ".status-bar{width:100%;max-width:480px;background:#0f172a;border:1px solid #1e3a5f;"
    "border-radius:8px;padding:8px 12px;font-size:11px;color:#475569;"
    "margin-bottom:12px;font-family:monospace}"
    "</style></head><body>"
    "<h1>GLYPH TANK</h1>"
    "<p class='sub'>Controller Config</p>";

  html += "<div class='ip-box'>&#127760; http://";
  html += WiFi.localIP().toString();
  html += "</div>";

  html += "<div class='status-bar'>Tanks saved: ";
  html += String(tankCount);
  html += " / 18 &nbsp;|&nbsp; Active: ";
  
  if (activeTank == COMMAND_ALL_ID) {
    html += "<span style='color:#c4b5fd'>ALL TANKS</span>";
  } else {
    html += (activeTank >= 0 && activeTank < tankCount) ? tanks[activeTank].name : "None";
  }
  html += "</div>";

  if (message.length()) {
    bool isErr = message.startsWith("ERR");
    html += "<div class='msg";
    html += (isErr ? " err" : "");
    html += "'>";
    html += message;
    html += "</div>";
  }

  // --- CONTROL ALL BUTTON ---
  html += "<form method='POST' action='/selectall'>";
  if (activeTank == COMMAND_ALL_ID) {
    html += "<button class='btn-all' type='submit' style='background:#4c1d95;border:2px solid #a78bfa;'>&#9679; CONTROLLING ALL TANKS</button>";
  } else {
    html += "<button class='btn-all' type='submit'>Control All Tanks Simultaneously</button>";
  }
  html += "</form>";

  html += "<div class='card'><h2>&#x1F6E1; Saved Tanks</h2>";
  if (tankCount == 0) {
    html += "<p class='no-tanks'>No tanks saved yet. Add one below.</p>";
  } else {
    for (int i = 0; i < tankCount; i++) {
      html += "<div class='tank-row'>"
              "<div class='tank-info'>"
              "<div class='tank-name'>" + tanks[i].name + "</div>"
              "<div class='tank-mac'>"  + tanks[i].mac  + "</div>"
              "</div>";
      if (i == activeTank) {
        html += "<span class='active-badge'>&#9679; ACTIVE</span>";
      } else {
        html += "<form method='POST' action='/select' style='display:inline'>"
                "<input type='hidden' name='idx' value='" + String(i) + "'>"
                "<button class='btn btn-select' type='submit'>Select</button>"
                "</form>";
      }
      html += "<form method='POST' action='/delete' style='display:inline;margin-left:6px'>"
              "<input type='hidden' name='idx' value='" + String(i) + "'>"
              "<button class='btn btn-delete' type='submit'>Del</button>"
              "</form></div>";
    }
  }
  html += "</div>";

  html +=
    "<div class='card'>"
    "<h2>&#43; Add Tank</h2>"
    "<form method='POST' action='/add' id='addForm'>"
    "<div class='form-row'>"
    "<div>"
    "<div class='field-label'>Tank Name</div>"
    "<input type='text' name='name' placeholder='e.g. Tank 1' maxlength='16'>"
    "</div>"
    "<div>"
    "<div class='field-label'>MAC Address</div>"
    "<div class='mac-row'>"
    "<input class='mac-seg' maxlength='2' id='m0' placeholder='AA'>"
    "<span class='mac-sep'>:</span>"
    "<input class='mac-seg' maxlength='2' id='m1' placeholder='BB'>"
    "<span class='mac-sep'>:</span>"
    "<input class='mac-seg' maxlength='2' id='m2' placeholder='CC'>"
    "<span class='mac-sep'>:</span>"
    "<input class='mac-seg' maxlength='2' id='m3' placeholder='DD'>"
    "<span class='mac-sep'>:</span>"
    "<input class='mac-seg' maxlength='2' id='m4' placeholder='EE'>"
    "<span class='mac-sep'>:</span>"
    "<input class='mac-seg' maxlength='2' id='m5' placeholder='FF'>"
    "</div>"
    "<input type='hidden' name='mac' id='macHidden'>"
    "<p class='hint'>Tip: paste full MAC like A1:B2:C3:D4:E5:F6 into first box.</p>"
    "</div>"
    "<button class='btn btn-add' type='submit'>+ Add Tank</button>"
    "</div></form></div>";

   html +=
    "<script>"
    "var segs=document.querySelectorAll('.mac-seg');"
    "segs.forEach(function(el,idx){"
    "  el.addEventListener('input',function(){"
    "    this.value=this.value.replace(/[^0-9a-fA-F]/g,'').toUpperCase();"
    "    if(this.value.length===2&&idx<5)segs[idx+1].focus();"
    "  });"
    "  el.addEventListener('keydown',function(e){"
    "    if(e.key==='Backspace'&&this.value===''&&idx>0)segs[idx-1].focus();"
    "    if(e.key===':'&&idx<5){e.preventDefault();segs[idx+1].focus();}"
    "  });"
    "  el.addEventListener('paste',function(e){"
    "    e.preventDefault();"
    "    var txt=(e.clipboardData||window.clipboardData).getData('text');"
    "    var hex=txt.replace(/[^0-9a-fA-F]/g,'').toUpperCase();"
    "    for(var i=0;i<6&&i*2<hex.length;i++)segs[i].value=hex.substr(i*2,2);"
    "  });"
    "});"
    "document.getElementById('addForm').addEventListener('submit',function(){"
    "  var parts=[];"
    "  for(var i=0;i<6;i++)parts.push(segs[i].value.padStart(2,'0'));"
    "  document.getElementById('macHidden').value=parts.join(':');"
    "});"
    "</script>";

  html +=
    "<div style='width:100%;max-width:480px'>"
    "<form method='POST' action='/drive'>"
    "<button class='drive-btn' type='submit'>&#9654; Start Drive Mode</button>"
    "</form>"
    "<form method='POST' action='/reload'>"
    "<button class='cfg-btn' type='submit'>&#8635; Reload Page</button>"
    "</form>"
    "</div></body></html>";

  return html;
}

// ── Route handlers ────────────────────────────────────────────

void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleAdd() {
  if (tankCount >= MAX_TANKS) {
    server.send(200, "text/html", buildPage("ERR: Max " + String(MAX_TANKS) + " tanks reached"));
    return;
  }
  String name = server.arg("name");
  String mac  = server.arg("mac");
  mac.toUpperCase(); mac.trim(); name.trim();
  if (name.length() == 0) name = "Tank " + String(tankCount + 1);

  bool valid = (mac.length() == 17);
  if (valid) {
    for (int i = 0; i < 17; i++) {
      char c = mac[i];
      if (i % 3 == 2) { if (c != ':') { valid = false; break; } }
      else             { if (!isxdigit(c)) { valid = false; break; } }
    }
  }
  if (!valid) {
    server.send(200, "text/html", buildPage("ERR: Invalid MAC — use AA:BB:CC:DD:EE:FF"));
    return;
  }
  for (int i = 0; i < tankCount; i++) {
    if (tanks[i].mac == mac) {
      server.send(200, "text/html", buildPage("ERR: Already saved as \"" + tanks[i].name + "\""));
      return;
    }
  }

  tanks[tankCount].name = name;
  tanks[tankCount].mac  = mac;
  tankCount++;
  if (activeTank < 0) activeTank = 0; 
  saveToNVS();

  Serial.println("Tank added: " + name + " [" + mac + "]");
  server.send(200, "text/html", buildPage("Saved: " + name + " (" + mac + ")"));
}

void handleSelect() {
  int idx = server.arg("idx").toInt();
  if (idx >= 0 && idx < tankCount) {
    activeTank = idx;
    saveToNVS();
    Serial.println("Active tank set: " + tanks[idx].name);
    server.send(200, "text/html", buildPage("Active: " + tanks[idx].name));
  } else {
    server.send(200, "text/html", buildPage("ERR: Invalid index"));
  }
}

void handleSelectAll() {
  activeTank = COMMAND_ALL_ID;
  saveToNVS();
  Serial.println("Active set to: ALL TANKS (Broadcast)");
  server.send(200, "text/html", buildPage("Active: ALL TANKS (Broadcast Mode)"));
}

void handleDelete() {
  int idx = server.arg("idx").toInt();
  if (idx < 0 || idx >= tankCount) {
    server.send(200, "text/html", buildPage("ERR: Invalid index"));
    return;
  }
  String deleted = tanks[idx].name;
  for (int i = idx; i < tankCount - 1; i++) tanks[i] = tanks[i + 1];
  tankCount--;
  if      (activeTank == idx)  activeTank = (tankCount > 0) ? 0 : -1;
  else if (activeTank > idx)   activeTank--;
  saveToNVS();
  Serial.println("Tank deleted: " + deleted);
  server.send(200, "text/html", buildPage("Deleted: " + deleted));
}

void handleDrive() {
  if (activeTank == -1) {
     server.send(200, "text/html", buildPage("ERR: No tank selected! Add and select a tank first."));
    return;
  }
  setDriveFlag(true);
  Serial.println("Drive flag set. Rebooting into drive mode...");
  String targetName = (activeTank == COMMAND_ALL_ID) ? "ALL TANKS" : tanks[activeTank].name;
  
  server.send(200, "text/html",
    "<html><body style='background:#0b0f1a;color:#e2e8f0;"
    "display:flex;flex-direction:column;align-items:center;"
    "justify-content:center;height:100vh;font-family:sans-serif;text-align:center;gap:16px'>"
    "<div style='font-size:48px'>&#9654;</div>"
    "<div style='font-size:20px;font-weight:700;color:#f1f5f9'>Drive Mode Starting...</div>"
    "<div style='font-size:13px;color:#475569'>Target: " + targetName + "</div>"
    "<div style='font-size:12px;color:#334155;margin-top:8px'>"
    "Press and hold joystick button for 3 seconds to exit.</div>" // Updated hint
    "</body></html>");
  delay(1500);
  ESP.restart();
}

void handleReload() {
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

// ═══════════════════════════════════════════════════════════════
//  SETUP & MODES
// ═══════════════════════════════════════════════════════════════

void startConfigMode() {
  configMode = true;
  Serial.println("═══════════════════════════════");
  Serial.println("        CONFIG MODE");
  Serial.println("═══════════════════════════════");
  Serial.print("Connecting to: "); Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (++attempts > 30) {
      Serial.println("\nERROR: WiFi failed!");
      while (true) delay(1000);
    }
  }

  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());

  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/add",    HTTP_POST, handleAdd);
  server.on("/select", HTTP_POST, handleSelect);
  server.on("/selectall", HTTP_POST, handleSelectAll); 
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/drive",  HTTP_POST, handleDrive);
  server.on("/reload", HTTP_POST, handleReload);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void startDriveMode() {
  Serial.println("═══════════════════════════════");
  Serial.println("        DRIVE MODE");
  Serial.println("═══════════════════════════════");

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED!");
    while (true) delay(1000);
  }

  applyActiveTank();

  if (activeTank == COMMAND_ALL_ID) {
    Serial.println("Mode: BROADCAST (All Tanks)");
  } else if (activeTank >= 0) {
    Serial.println("Mode: SINGLE TANK (" + tanks[activeTank].name + ")");
  } else {
    Serial.println("Mode: IDLE (No tank selected)");
  }
  Serial.println("Hold joystick button for 3 sec to exit.");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Initialize Button Pin with Pullup (Input defaults to HIGH)
  // When pressed, button connects Pin to GND (LOW).
  pinMode(joyButton_Pin, INPUT_PULLUP);

  loadFromNVS();
  
  bool driveFlag = getDriveFlag();
  Serial.println("Drive flag: " + String(driveFlag));

  if (driveFlag) {
    startDriveMode();
  } else {
    startConfigMode();
  }
}

void loop() {
  if (configMode) {
    server.handleClient();
    return;
  }

  // ── BUTTON EXIT LOGIC (3 SEC HOLD) ───────────────────────────────
  // digitalRead returns LOW (0) when pressed because of INPUT_PULLUP
  if (digitalRead(joyButton_Pin) == LOW) {
    if (!isButtonPressed) {
      // Button just pressed
      isButtonPressed = true;
      buttonPressStart = millis();
      Serial.println("Button pressed... waiting for hold.");
    } else {
      // Button is being held
      if (millis() - buttonPressStart >= EXIT_HOLD_TIME) {
        Serial.println("Button held for 3 seconds. Exiting Drive Mode...");
        setDriveFlag(false);
        delay(200);
        ESP.restart();
      }
    }
  } else {
    // Button released or not pressed
    if (isButtonPressed) {
      Serial.println("Button released too early.");
    }
    isButtonPressed = false;
  }
  // ────────────────────────────────────────────────────────────────

  if (!peerAdded) { delay(20); return; }

  // Joystick Logic
  int xRaw = analogRead(joyX_Pin);
  int yRaw = analogRead(joyY_Pin);

  int throttle =  mapJoy(xRaw, X_MIN, X_CENTER, X_MAX);
  int steering = -mapJoy(yRaw, Y_MIN, Y_CENTER, Y_MAX);

  if (abs(throttle) < DEADZONE) throttle = 0;
  if (abs(steering) < DEADZONE) steering = 0;

  int L = constrain(throttle + steering, -255, 255);
  int R = constrain(throttle - steering, -255, 255);

  myData.leftMotor  = L;
  myData.rightMotor = R;

  esp_now_send(activeMacBytes, (uint8_t*)&myData, sizeof(myData));

  delay(20);
}