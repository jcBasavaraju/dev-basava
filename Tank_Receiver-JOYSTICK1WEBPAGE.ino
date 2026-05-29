// ═══════════════════════════════════════════════════════════════
//  GLYPH TANK — Receiver Firmware
//  - Shows own MAC address on OLED until joystick connects
//  - Once first ESP-NOW packet arrives → switches to emoji faces
//  - Receives ESP-NOW from the joystick controller
// ═══════════════════════════════════════════════════════════════

#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <U8g2lib.h>

// --- DISPLAY ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, U8X8_PIN_NONE);

// --- SERVO PINS ---
const int SERVO1_PIN = 2;
const int SERVO2_PIN = 3;

// --- CALIBRATION ---
const int STOP_POS_L = 99;
const int STOP_POS_R = 99;
const int MAX_SPEED  = 10;

// --- DATA STRUCTURE (must match joystick) ---
typedef struct struct_message {
  int leftMotor;
  int rightMotor;
} struct_message;

struct_message incomingData;

// --- GLOBALS ---
Servo servo1, servo2;
unsigned long lastCmdMs       = 0;
const long    WATCHDOG_MS     = 600;
volatile bool newDataReceived = false;
bool          everConnected   = false;

// ═══════════════════════════════════════════════════════════════
//  OLED — MAC SCREEN
//  y=64 was clipping the last row — moved up to y=58
// ═══════════════════════════════════════════════════════════════

void showMacScreen(const String& mac) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(4, 12, "GLYPH TANK");

  u8g2.setFont(u8g2_font_helvR08_tr);
  u8g2.drawStr(4, 25, "Waiting for");
  u8g2.drawStr(4, 36, "joystick...");

  u8g2.drawLine(0, 41, 128, 41);

  // MAC split into two lines — baseline at 51 and 61 (not 55/64)
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(4, 51, mac.substring(0, 8).c_str());
  u8g2.drawStr(4, 61, mac.substring(9).c_str());

  u8g2.sendBuffer();
}

// ═══════════════════════════════════════════════════════════════
//  OLED — EMOJI FACES
//  Display is 128x64. Centre = (64, 32).
//  All faces redrawn around that centre with generous inset.
// ═══════════════════════════════════════════════════════════════

// Eye centres: left=(40,22) right=(88,22)  — well inside 128 wide
// Mouth zone: y=42..56  — well inside 64 tall

void drawFaceForward() {
  // Left eye
  u8g2.drawCircle(40, 22, 11);
  u8g2.drawCircle(40, 22, 6);
  u8g2.drawDisc  (40, 22, 3);
  // Right eye
  u8g2.drawCircle(88, 22, 11);
  u8g2.drawCircle(88, 22, 6);
  u8g2.drawDisc  (88, 22, 3);
  // Mouth (grid)
  u8g2.drawBox(36, 43, 56, 13);
  u8g2.setDrawColor(0);
  u8g2.drawHLine(36, 49, 56);
  u8g2.drawVLine(54, 43, 13);
  u8g2.drawVLine(74, 43, 13);
  u8g2.setDrawColor(1);
}

void drawFaceReverse() {
  // X eyes
  u8g2.drawLine(30, 14, 52, 32); u8g2.drawLine(52, 14, 30, 32);
  u8g2.drawLine(76, 14, 98, 32); u8g2.drawLine(98, 14, 76, 32);
  // Wavy frown — all y well above 64
  u8g2.drawLine(36, 52, 52, 46);
  u8g2.drawLine(52, 46, 76, 54);
  u8g2.drawLine(76, 54, 92, 48);
}

void drawFaceLeft() {
  // Left eye = circle with pupil shifted left
  u8g2.drawCircle(40, 22, 11);
  u8g2.drawDisc  (34, 22, 4);
  // Right eye = flat line
  u8g2.drawLine(76, 22, 100, 22);
  // Mouth
  u8g2.drawLine(36, 50, 78, 50);
  u8g2.drawLine(78, 50, 92, 44);
}

void drawFaceRight() {
  // Left eye = flat line
  u8g2.drawLine(28, 22, 52, 22);
  // Right eye = circle with pupil shifted right
  u8g2.drawCircle(88, 22, 11);
  u8g2.drawDisc  (94, 22, 4);
  // Mouth
  u8g2.drawLine(36, 50, 92, 50);
  u8g2.drawLine(36, 50, 24, 44);
}

void drawFaceStop() {
  // Flat eyes
  u8g2.drawLine(30, 22, 52, 22);
  u8g2.drawLine(76, 22, 98, 22);
  // Flat mouth
  u8g2.drawLine(42, 49, 86, 49);
  // Zs — top-right but inset
  u8g2.setFont(u8g2_font_helvR08_tr);
  u8g2.drawStr(88, 16, "Z");
  u8g2.drawStr(100, 10, "z");
}

void drawFaceReady() {
  // Eyes
  u8g2.drawCircle(40, 22, 9);
  u8g2.drawDisc  (40, 22, 3);
  u8g2.drawCircle(88, 22, 9);
  u8g2.drawDisc  (88, 22, 3);
  // Smile
  u8g2.drawEllipse(64, 42, 20, 9);
}

void updateDisplay(const String& label) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  String key = label;
  key.toUpperCase();
  if      (key == "FORWARD")                   drawFaceForward();
  else if (key == "REVERSE")                   drawFaceReverse();
  else if (key == "LEFT")                      drawFaceLeft();
  else if (key == "RIGHT")                     drawFaceRight();
  else if (key == "STOP" || key == "WATCHDOG") drawFaceStop();
  else                                         drawFaceReady();
  u8g2.sendBuffer();
}

// ═══════════════════════════════════════════════════════════════
//  ESP-NOW CALLBACK
// ═══════════════════════════════════════════════════════════════

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  newDataReceived = true;
  lastCmdMs       = millis();
  if (!everConnected) everConnected = true;
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  Wire.begin(5, 4);
  u8g2.begin();

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(STOP_POS_L);
  servo2.write(STOP_POS_R);

  WiFi.mode(WIFI_STA);

  String mac = WiFi.macAddress();
  Serial.println("Tank MAC: " + mac);

  showMacScreen(mac);

  if (esp_now_init() != ESP_OK) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvR08_tr);
    u8g2.drawStr(4, 32, "ESP-NOW FAILED");
    u8g2.sendBuffer();
    while (true) delay(1000);
  }

  esp_now_register_recv_cb(OnDataRecv);
  lastCmdMs = millis();

  Serial.println("Waiting for joystick...");
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════

void loop() {

  // ── Not yet connected: stay on MAC screen ────────────────────
  if (!everConnected) {
    delay(10);
    return;
  }

  // ── Connected: normal drive loop ─────────────────────────────

  // WATCHDOG — joystick signal lost
  if (millis() - lastCmdMs > WATCHDOG_MS) {
    servo1.write(STOP_POS_L);
    servo2.write(STOP_POS_R);
    updateDisplay("WATCHDOG");
    newDataReceived = false;
    lastCmdMs = millis();
    return;
  }

  // NEW DATA
  if (newDataReceived) {
    newDataReceived = false;

    int leftSpeed  = incomingData.leftMotor;
    int rightSpeed = incomingData.rightMotor;

    int s1 = map(leftSpeed,  -255, 255, 180 - MAX_SPEED, MAX_SPEED);
    int s2 = map(rightSpeed, -255, 255, MAX_SPEED, 180 - MAX_SPEED);

    if (leftSpeed  == 0) s1 = STOP_POS_L;
    if (rightSpeed == 0) s2 = STOP_POS_R;

    servo1.write(s1);
    servo2.write(s2);

    String label = "STOP";
    if      (leftSpeed > 50  && rightSpeed > 50)  label = "FORWARD";
    else if (leftSpeed < -50 && rightSpeed < -50) label = "REVERSE";
    else if (leftSpeed > 50  && rightSpeed < -50) label = "RIGHT";
    else if (leftSpeed < -50 && rightSpeed > 50)  label = "LEFT";

    updateDisplay(label);
  }
}