#include <esp_now.h>
#include <WiFi.h>

// --- PIN DEFINITIONS ---
const int joyX_Pin = 1;
const int joyY_Pin = 2;

// --- RECEIVER MAC ADDRESS ---
uint8_t broadcastAddress[] = {0x10, 0xB4, 0x1D, 0x0C, 0x6F, 0x14};

// --- CALIBRATION VALUES ---
const int X_MIN    = 0;
const int X_CENTER = 2126;
const int X_MAX    = 4095;

const int Y_MIN    = 0;
const int Y_CENTER = 2200;
const int Y_MAX    = 4095;

const int DEADZONE = 20;

// --- DATA STRUCTURE ---
typedef struct struct_message {
  int leftMotor;
  int rightMotor;
} struct_message;

struct_message myData;

int mapJoy(int raw, int minVal, int center, int maxVal) {
  if (raw < center) {
    return map(raw, minVal, center, -255, 0);
  } else {
    return map(raw, center, maxVal, 0, 255);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Init Failed");
    return;
  }

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Peer Failed");
    return;
  }
  Serial.println("Joystick Ready!");
}

void loop() {
  // 1. Read Joystick
  int xRaw = analogRead(joyX_Pin);
  int yRaw = analogRead(joyY_Pin);

  // 2. Map using actual center points
  int throttle =  mapJoy(xRaw, X_MIN, X_CENTER, X_MAX);
  int steering = -mapJoy(yRaw, Y_MIN, Y_CENTER, Y_MAX); // negative = swapped

  // 3. Deadzone BEFORE mixing
  if (abs(throttle) < DEADZONE) throttle = 0;
  if (abs(steering) < DEADZONE) steering = 0;

  // 4. Tank Mixing
  int leftMotorSpeed  = throttle + steering;
  int rightMotorSpeed = throttle - steering;

  leftMotorSpeed  = constrain(leftMotorSpeed,  -255, 255);
  rightMotorSpeed = constrain(rightMotorSpeed, -255, 255);

  // 5. Send Data
  myData.leftMotor  = leftMotorSpeed;
  myData.rightMotor = rightMotorSpeed;

  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  Serial.print("Throttle: "); Serial.print(throttle);
  Serial.print("  Steering: "); Serial.print(steering);
  Serial.print("  L: "); Serial.print(leftMotorSpeed);
  Serial.print("  R: "); Serial.println(rightMotorSpeed);

  delay(20);
}