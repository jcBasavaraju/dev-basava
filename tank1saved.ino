// ═══════════════════════════════════════════════════════════════
//  GLYPH TANK — WiFiManager + WASD Only
//  - No hardcoded WiFi credentials
//  - Connects to saved WiFi or creates AP "GlyphTankSetup"
//  - WASD Keyboard Control (No Spacebar Stop Button)
// ═══════════════════════════════════════════════════════════════

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <U8g2lib.h>

// ───────────────────────────────────────────────────────────────
//  DISPLAY SETUP (U8g2)
// ───────────────────────────────────────────────────────────────

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, /* reset=*/ U8X8_PIN_NONE);

// ───────────────────────────────────────────────────────────────
//  SERVO SETTINGS
// ───────────────────────────────────────────────────────────────

const int SERVO1_PIN = 2;
const int SERVO2_PIN = 3;
const int STOP_POS   = 90;
const int ONE_ROT_MS = 100;

// ───────────────────────────────────────────────────────────────
//  WATCHDOG TIMER
// ───────────────────────────────────────────────────────────────

unsigned long lastCmdMs         = 0;
const unsigned long WATCHDOG_MS = 2000;
bool servosRunning = false;
bool stopFlag      = false;

// ───────────────────────────────────────────────────────────────
//  GLOBAL STATE
// ───────────────────────────────────────────────────────────────

String currentLabel = "READY";
Servo servo1, servo2;
WebServer server(80);

// ═══════════════════════════════════════════════════════════════
//  FULL SCREEN EMOJI DRAWING FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void drawFaceForward() {
  // CRAZY EXCITED (Spiral Eyes + Open Mouth)
  u8g2.drawCircle(40, 26, 16); u8g2.drawCircle(40, 26, 9); u8g2.drawDisc(40, 26, 4);
  u8g2.drawCircle(88, 26, 16); u8g2.drawCircle(88, 26, 9); u8g2.drawDisc(88, 26, 4);
  u8g2.drawBox(32, 46, 64, 16);
  u8g2.setDrawColor(0); u8g2.drawHLine(32, 54, 64); u8g2.drawVLine(52, 46, 16); u8g2.drawVLine(76, 46, 16);
  u8g2.setDrawColor(1);
}

void drawFaceReverse() {
  // DIZZY (X-X Eyes)
  u8g2.drawLine(25, 13, 55, 38); u8g2.drawLine(55, 13, 25, 38);
  u8g2.drawLine(73, 13, 103, 38); u8g2.drawLine(103, 13, 73, 38);
  u8g2.drawLine(40, 54, 60, 50); u8g2.drawLine(60, 50, 80, 56); u8g2.drawLine(80, 56, 100, 50);
}

void drawFaceLeft() {
  // WINK LEFT
  u8g2.drawCircle(40, 26, 14); u8g2.drawDisc(35, 26, 5);
  u8g2.drawLine(73, 26, 103, 26);
  u8g2.drawLine(30, 52, 75, 52); u8g2.drawLine(75, 52, 90, 45);
}

void drawFaceRight() {
  // WINK RIGHT
  u8g2.drawLine(25, 26, 55, 26);
  u8g2.drawCircle(88, 26, 14); u8g2.drawDisc(93, 26, 5);
  u8g2.drawLine(53, 52, 98, 52); u8g2.drawLine(53, 52, 38, 45);
}

void drawFaceStop() {
  // DEAD/SLEEPING
  u8g2.drawLine(25, 26, 55, 26);
  u8g2.drawLine(73, 26, 103, 26);
  u8g2.drawLine(44, 50, 84, 50);
  u8g2.setFont(u8g2_font_helvR08_tr); u8g2.drawStr(100, 18, "Z"); u8g2.drawStr(110, 12, "z");
}

void drawFaceReady() {
  // HAPPY DERP
  u8g2.drawCircle(40, 26, 12); u8g2.drawDisc(40, 26, 4);
  u8g2.drawCircle(88, 26, 12); u8g2.drawDisc(88, 26, 4);
  u8g2.drawEllipse(64, 40, 28, 14);
}

void updateDisplay(const String& label) {
  currentLabel = label;
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  String key = label;
  key.toUpperCase();
  
  if (key == "FORWARD" || key == "FWD") drawFaceForward();
  else if (key == "REVERSE" || key == "REV") drawFaceReverse();
  else if (key == "LEFT") drawFaceLeft();
  else if (key == "RIGHT") drawFaceRight();
  else if (key == "STOP" || key == "WATCHDOG") drawFaceStop();
  else drawFaceReady();

  u8g2.sendBuffer();
}

// ═══════════════════════════════════════════════════════════════
//  SERVO FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void stopServos() {
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(STOP_POS);
  servo2.write(STOP_POS);
  servo1.detach();
  servo2.detach();
  servosRunning = false;
  stopFlag = false;
  updateDisplay("STOP");
}

void moveBoth(int a1, int a2) {
  stopFlag = false;
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(a1);
  servo2.write(a2);
  lastCmdMs = millis();
  servosRunning = true;
}

// ═══════════════════════════════════════════════════════════════
//  HTTP HANDLERS
// ═══════════════════════════════════════════════════════════════

void handleMove() {
  stopFlag = false;
  if (!server.hasArg("s1") || !server.hasArg("s2")) { server.send(400, "text/plain", "Missing"); return; }
  int a1 = constrain(server.arg("s1").toInt(), 0, 180);
  int a2 = constrain(server.arg("s2").toInt(), 0, 180);

  String label = "Moving";
  if (a1 == 112 && a2 == 68) label = "Forward";
  else if (a1 == 68 && a2 == 112) label = "Reverse";
  else if (a1 == 68 && a2 == 68) label = "Left";
  else if (a1 == 112 && a2 == 112) label = "Right";
  else if (a1 == 90 && a2 == 90) label = "Stop";

  moveBoth(a1, a2);
  updateDisplay(label);
  server.send(200, "text/plain", label);
}

void handleTap() {
  stopFlag = false;
  if (!server.hasArg("s1") || !server.hasArg("s2")) { server.send(400, "text/plain", "Missing"); return; }
  int a1 = constrain(server.arg("s1").toInt(), 0, 180);
  int a2 = constrain(server.arg("s2").toInt(), 0, 180);

  String label = "Tap";
  if (a1 == 112 && a2 == 68) label = "Forward";
  else if (a1 == 68 && a2 == 112) label = "Reverse";
  else if (a1 == 68 && a2 == 68) label = "Left";
  else if (a1 == 112 && a2 == 112) label = "Right";

  server.send(200, "text/plain", "Tap");
  moveBoth(a1, a2);
  updateDisplay(label);
  delay(ONE_ROT_MS);
  stopServos();
}

void handleStop() {
  stopFlag = true;
  stopServos();
  server.send(200, "text/plain", "Stopped");
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }

void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <title>GLYPH TANK</title>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: sans-serif; background: #0f172a;
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 24px;
      user-select: none; -webkit-user-select: none; touch-action: none;
    }
    .card {
      background: #1e293b; border-radius: 20px; padding: 32px 24px;
      width: 100%; max-width: 360px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
    }
    h2 { font-size: 24px; color: #f1f5f9; margin-bottom: 6px; text-align: center; letter-spacing: 2px; }
    .subtitle { font-size: 13px; color: #64748b; text-align: center; margin-bottom: 28px; }
    .dot {
      display: inline-block; width: 8px; height: 8px;
      border-radius: 50%; background: #22c55e;
      margin-right: 6px; vertical-align: middle;
      box-shadow: 0 0 6px #22c55e;
    }
    .btn-grid {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      grid-template-rows: auto auto auto;
      gap: 12px; margin-bottom: 12px;
    }
    .btn {
      padding: 22px 10px; font-size: 14px; font-weight: 700;
      border: none; border-radius: 14px; cursor: pointer;
      display: flex; flex-direction: column;
      align-items: center; justify-content: center; gap: 6px;
      color: #fff; touch-action: none;
      -webkit-tap-highlight-color: transparent;
      transition: filter 0.08s, transform 0.08s;
    }
    .btn.pressed { filter: brightness(0.65); transform: scale(0.94); }
    .btn .icon { font-size: 26px; line-height: 1; }
    .btn .key-hint { font-size: 10px; opacity: 0.6; }
    .btn-forward  { background: #4f46e5; grid-column: 2; grid-row: 1; }
    .btn-left     { background: #0ea5e9; grid-column: 1; grid-row: 2; }
    .btn-right    { background: #0ea5e9; grid-column: 3; grid-row: 2; }
    .btn-backward { background: #4f46e5; grid-column: 2; grid-row: 3; }
    #status { margin-top: 16px; font-size: 13px; color: #94a3b8; text-align: center; min-height: 20px; }
    #ping { font-size: 11px; color: #475569; text-align: center; margin-top: 6px; }
  </style>
</head>
<body>
  <div class='card'>
    <h2>GLYPH TANK</h2>
    <p class='subtitle'><span class='dot'></span>WASD or Buttons</p>

    <div class='btn-grid'>
      <button class='btn btn-forward' id='btn-fwd'>
        <span class='icon'>&#9650;</span><span class='key-hint'>W</span>
      </button>
      <button class='btn btn-left' id='btn-left'>
        <span class='icon'>&#9664;</span><span class='key-hint'>A</span>
      </button>
      <button class='btn btn-right' id='btn-right'>
        <span class='icon'>&#9654;</span><span class='key-hint'>D</span>
      </button>
      <button class='btn btn-backward' id='btn-bwd'>
        <span class='icon'>&#9660;</span><span class='key-hint'>S</span>
      </button>
    </div>

    <div id='status'>Ready</div>
    <div id='ping'></div>
  </div>

<script>
  const statusEl = document.getElementById('status');
  const pingEl   = document.getElementById('ping');
  const BTN_IDS  = ['btn-fwd','btn-bwd','btn-left','btn-right'];

  const HOLD_THRESHOLD_MS = 300;
  const HEARTBEAT_MS      = 100;

  let isHolding    = false;
  let heartbeat    = null;
  let activeBtn    = null;
  let pressTimer   = null;
  let pressStart   = 0;
  let pendingFetch = false;
  let currentS1    = 90, currentS2 = 90;

  function sendMove(s1, s2) {
    if (pendingFetch) return;
    pendingFetch = true;
    const t0 = Date.now();
    fetch('/move?s1=' + s1 + '&s2=' + s2, { cache: 'no-store' })
      .then(r => r.text())
      .then(t => {
        pendingFetch = false;
        pingEl.textContent = (Date.now() - t0) + 'ms';
        if (isHolding) statusEl.textContent = t + ' (hold)';
      })
      .catch(() => { pendingFetch = false; });
  }

  function sendTap(s1, s2) {
    pendingFetch = false;
    const t0 = Date.now();
    fetch('/tap?s1=' + s1 + '&s2=' + s2, { cache: 'no-store' })
      .then(r => r.text())
      .then(t => {
        pingEl.textContent = (Date.now() - t0) + 'ms';
        statusEl.textContent = 'Tap done';
      })
      .catch(() => {});
  }

  function sendStop() {
    pendingFetch = false;
    fetch('/stop', { keepalive: true, cache: 'no-store' })
      .then(r => r.text())
      .then(t => { statusEl.textContent = t; })
      .catch(() => {});
  }

  function onPress(s1, s2, label, btn) {
    if (activeBtn && activeBtn !== btn) {
       clearTimeout(pressTimer);
       clearInterval(heartbeat);
       activeBtn.classList.remove('pressed');
    }

    activeBtn = btn;
    activeBtn.classList.add('pressed');

    currentS1  = s1;
    currentS2  = s2;
    pressStart = Date.now();
    isHolding  = false;

    statusEl.textContent = label + '...';

    pressTimer = setTimeout(() => {
      isHolding = true;
      statusEl.textContent = label + ' (hold)';
      sendMove(s1, s2);

      clearInterval(heartbeat);
      heartbeat = setInterval(() => {
        if (isHolding) sendMove(currentS1, currentS2);
      }, HEARTBEAT_MS);
    }, HOLD_THRESHOLD_MS);
  }

  function onRelease() {
    clearTimeout(pressTimer);
    pressTimer = null;

    if (activeBtn) activeBtn.classList.remove('pressed');
    
    const prevBtn = activeBtn; 
    activeBtn = null;

    if (isHolding) {
      isHolding = false;
      clearInterval(heartbeat);
      heartbeat = null;
      statusEl.textContent = 'Stopping...';
      sendStop();
    } else if (prevBtn) {
      const elapsed = Date.now() - pressStart;
      if (elapsed < HOLD_THRESHOLD_MS) {
        statusEl.textContent = 'Tap...';
        sendTap(currentS1, currentS2);
      }
    }
  }

  const bindings = [
    { id: 'btn-fwd',   s1: 112, s2: 68,  label: 'Forward', key: 'w' },
    { id: 'btn-bwd',   s1: 68,  s2: 112, label: 'Reverse', key: 's' },
    { id: 'btn-left',  s1: 68,  s2: 68,  label: 'Left',    key: 'a' },
    { id: 'btn-right', s1: 112, s2: 112, label: 'Right',   key: 'd' }
  ];

  bindings.forEach(({ id, s1, s2, label }) => {
    const el = document.getElementById(id);
    el.addEventListener('mousedown',  (e) => { e.preventDefault(); onPress(s1, s2, label, el); });
    el.addEventListener('touchstart', (e) => { e.preventDefault(); onPress(s1, s2, label, el); }, { passive: false });
  });

  document.addEventListener('mouseup',     () => onRelease());
  document.addEventListener('touchend',    () => onRelease());
  document.addEventListener('touchcancel', () => onRelease());

  document.addEventListener('keydown', (e) => {
    if (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA") return;
    const key = e.key.toLowerCase();
    const found = bindings.find(b => b.key === key);
    if (found) {
      e.preventDefault();
      if (e.repeat) return; 
      const el = document.getElementById(found.id);
      onPress(found.s1, found.s2, found.label, el);
    }
  });

  document.addEventListener('keyup', (e) => {
    const key = e.key.toLowerCase();
    const found = bindings.find(b => b.key === key);
    if (found) {
      onRelease();
    }
  });

  document.addEventListener('touchmove', (e) => {
    if (!isHolding) return;
    const t  = e.touches[0];
    const el = document.elementFromPoint(t.clientX, t.clientY);
    const over = BTN_IDS.some(id => {
      const b = document.getElementById(id);
      return b && b.contains(el);
    });
    if (!over) onRelease();
  }, { passive: true });

  document.addEventListener('contextmenu', (e) => e.preventDefault());
  document.addEventListener('visibilitychange', () => { if (document.hidden) onRelease(); });
  window.addEventListener('blur', () => onRelease());
</script>
</body>
</html>
)rawhtml";
  server.send(200, "text/html", html);
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Wire.begin(5, 4); 
  u8g2.begin();

  // Show "Config Mode" screen initially
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvR08_tr);
  u8g2.drawStr(0, 12, "GLYPH TANK");
  u8g2.drawStr(0, 30, "Starting...");
  u8g2.sendBuffer();

  // WiFiManager Local Initialization
  WiFiManager wm;

  // Sets timeout in seconds for the configuration portal.
  wm.setConfigPortalTimeout(180);

  // Custom Callback for when entering AP Mode
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvR08_tr);
    u8g2.drawStr(0, 12, "SETUP MODE");
    u8g2.drawStr(0, 30, "Connect WiFi to:");
    
    // Display SSID (AP Name)
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(0, 45, "GlyphTankSetup");
    
    // Display IP
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 58, "IP: 192.168.4.1");
    u8g2.sendBuffer();
  });

  // Attempt to connect to saved WiFi, or start AP
  bool res = wm.autoConnect("GlyphTankSetup", "glyph123"); // password protected

  if(!res) {
    Serial.println("Failed to connect or hit timeout");
    ESP.restart();
  } 
  else {
    Serial.println("Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvR08_tr);
    u8g2.drawStr(0, 20, "WiFi Connected!");
    u8g2.drawStr(0, 40, WiFi.localIP().toString().c_str());
    u8g2.sendBuffer();
    delay(2000);
  }

  // Start Server
  server.on("/",     HTTP_GET, handleRoot);
  server.on("/move", HTTP_GET, handleMove);
  server.on("/tap",  HTTP_GET, handleTap);
  server.on("/stop", HTTP_GET, handleStop);
  server.onNotFound(handleNotFound);

  server.begin();
  
  // Show Ready Emoji
  updateDisplay("READY");
}

void loop() {
  server.handleClient();
  if (servosRunning && (millis() - lastCmdMs > WATCHDOG_MS)) {
    stopServos();
    updateDisplay("WATCHDOG");
  }
}