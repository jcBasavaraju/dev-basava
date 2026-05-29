#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, U8X8_PIN_NONE);

const int SERVO1_PIN = 2;
const int SERVO2_PIN = 3;

const int STOP_POS_L   = 99;
const int STOP_POS_R   = 96;
const int MAX_SPEED    = 10;
const int DEADBAND     = 20;   // ← ignore noise below this value
const float SMOOTH     = 0.15f; // ← EMA factor: lower = smoother (try 0.1–0.3)

typedef struct struct_message {
  int leftMotor;
  int rightMotor;
} struct_message;

struct_message incomingData;

Servo servo1, servo2;
unsigned long lastCmdMs    = 0;
const long WATCHDOG_MS     = 600;
volatile bool newDataReceived = false;

// Smoothed targets (floats to accumulate fractional changes)
float smoothL = 0.0f;
float smoothR = 0.0f;

// Display update throttle
unsigned long lastDisplayMs = 0;
const long DISPLAY_INTERVAL = 150; // update OLED at most every 150ms
String lastLabel = "";

// ── OLED FACES ────────────────────────────────────────────────

void drawFaceForward() {
  u8g2.drawCircle(40,26,16); u8g2.drawCircle(40,26,9); u8g2.drawDisc(40,26,4);
  u8g2.drawCircle(88,26,16); u8g2.drawCircle(88,26,9); u8g2.drawDisc(88,26,4);
  u8g2.drawBox(32,46,64,16);
  u8g2.setDrawColor(0);
  u8g2.drawHLine(32,54,64); u8g2.drawVLine(52,46,16); u8g2.drawVLine(76,46,16);
  u8g2.setDrawColor(1);
}
void drawFaceReverse() {
  u8g2.drawLine(25,13,55,38); u8g2.drawLine(55,13,25,38);
  u8g2.drawLine(73,13,103,38); u8g2.drawLine(103,13,73,38);
  u8g2.drawLine(40,54,60,50); u8g2.drawLine(60,50,80,56); u8g2.drawLine(80,56,100,50);
}
void drawFaceLeft() {
  u8g2.drawCircle(40,26,14); u8g2.drawDisc(35,26,5);
  u8g2.drawLine(73,26,103,26);
  u8g2.drawLine(30,52,75,52); u8g2.drawLine(75,52,90,45);
}
void drawFaceRight() {
  u8g2.drawLine(25,26,55,26);
  u8g2.drawCircle(88,26,14); u8g2.drawDisc(93,26,5);
  u8g2.drawLine(53,52,98,52); u8g2.drawLine(53,52,38,45);
}
void drawFaceStop() {
  u8g2.drawLine(25,26,55,26); u8g2.drawLine(73,26,103,26); u8g2.drawLine(44,50,84,50);
  u8g2.setFont(u8g2_font_helvR08_tr);
  u8g2.drawStr(100,18,"Z"); u8g2.drawStr(110,12,"z");
}
void drawFaceReady() {
  u8g2.drawCircle(40,26,12); u8g2.drawDisc(40,26,4);
  u8g2.drawCircle(88,26,12); u8g2.drawDisc(88,26,4);
  u8g2.drawEllipse(64,40,28,14);
}

void updateDisplay(String label) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  label.toUpperCase();
  if      (label == "FORWARD")              drawFaceForward();
  else if (label == "REVERSE")              drawFaceReverse();
  else if (label == "LEFT")                 drawFaceLeft();
  else if (label == "RIGHT")                drawFaceRight();
  else if (label == "STOP" || label == "WATCHDOG") drawFaceStop();
  else                                      drawFaceReady();
  u8g2.sendBuffer();
}

// ── ESP-NOW CALLBACK ───────────────────────────────────────────

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  newDataReceived = true;
  lastCmdMs = millis();
}

// ── SETUP ──────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Wire.begin(5, 4);
  u8g2.begin();
  updateDisplay("BOOT");

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(STOP_POS_L);
  servo2.write(STOP_POS_R);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { updateDisplay("ERR"); return; }
  esp_now_register_recv_cb(OnDataRecv);
  updateDisplay("READY");
}

// ── LOOP ───────────────────────────────────────────────────────

void loop() {

  // 1. WATCHDOG
  if (millis() - lastCmdMs > WATCHDOG_MS) {
    servo1.write(STOP_POS_L);
    servo2.write(STOP_POS_R);
    smoothL = 0; smoothR = 0;
    // Throttle display update even in watchdog
    if (millis() - lastDisplayMs > DISPLAY_INTERVAL) {
      updateDisplay("WATCHDOG");
      lastDisplayMs = millis();
    }
    newDataReceived = false;
    return;
  }

  // 2. PROCESS DATA
  if (newDataReceived) {
    newDataReceived = false;

    int rawL = incomingData.leftMotor;
    int rawR = incomingData.rightMotor;

    // Apply deadband: treat small values as zero
    if (abs(rawL) < DEADBAND) rawL = 0;
    if (abs(rawR) < DEADBAND) rawR = 0;

    // Exponential Moving Average smoothing
    smoothL += SMOOTH * (rawL - smoothL);
    smoothR += SMOOTH * (rawR - smoothR);

    // Map smoothed value to servo angle
    int s1, s2;
    if (abs(smoothL) < 2.0f) {
      s1 = STOP_POS_L;   // snap to exact stop when near zero
    } else {
      s1 = map((int)smoothL, -255, 255, 180 - MAX_SPEED, MAX_SPEED);
    }
    if (abs(smoothR) < 2.0f) {
      s2 = STOP_POS_R;
    } else {
      s2 = map((int)smoothR, -255, 255, MAX_SPEED, 180 - MAX_SPEED);
    }

    servo1.write(s1);
    servo2.write(s2);

    // Throttled display update — does NOT block servo writes
    if (millis() - lastDisplayMs > DISPLAY_INTERVAL) {
      String label = "STOP";
      if      (rawL > 50  && rawR > 50)  label = "FORWARD";
      else if (rawL < -50 && rawR < -50) label = "REVERSE";
      else if (rawL > 50  && rawR < -50) label = "RIGHT";
      else if (rawL < -50 && rawR > 50)  label = "LEFT";

      if (label != lastLabel) {   // only redraw if face changed
        updateDisplay(label);
        lastLabel = label;
      }
      lastDisplayMs = millis();
    }
  }
}