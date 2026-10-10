#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <math.h>

// ---------- PIN CONFIGURATION ----------
const int SDA_PIN = 21;
const int SCL_PIN = 22;
const int LED_PIN = 23;
const int BUTTON_PIN = 18;

// ---------- MPU6050 ----------
const uint8_t MPU_ADDR = 0x68;
const float RAD_TO_DEG_F = 57.2957795f;

// ---------- WIRELESS ----------
const uint8_t WIFI_CHANNEL = 1;
uint8_t broadcastAddress[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// ---------- PACKET TYPES ----------
const uint8_t TYPE_CONTROL = 1;
const uint8_t TYPE_ACK = 2;

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint32_t seq;
  float pitch;
  float roll;
  uint8_t calibrated;
} Packet;

volatile uint32_t lastAckMs = 0;
volatile uint32_t lastAckSeq = 0;
uint32_t sequenceNumber = 0;

struct MPUData {
  int16_t ax, ay, az;
  int16_t temp;
  int16_t gx, gy, gz;
};

float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;
float filteredRoll = 0, filteredPitch = 0;
float referenceRoll = 0, referencePitch = 0;
bool filterInitialized = false;
bool calibrated = false;
bool previousButtonState = false;
uint32_t previousMicros = 0;
uint32_t lastSendMs = 0;

// Change to -1 if an axis feels reversed.
const float PITCH_SIGN = 1.0f;
const float ROLL_SIGN = 1.0f;

void writeMPU(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

bool readMPU(MPUData &d) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (size_t)14, true) != 14)
    return false;

  d.ax = (Wire.read() << 8) | Wire.read();
  d.ay = (Wire.read() << 8) | Wire.read();
  d.az = (Wire.read() << 8) | Wire.read();
  d.temp = (Wire.read() << 8) | Wire.read();
  d.gx = (Wire.read() << 8) | Wire.read();
  d.gy = (Wire.read() << 8) | Wire.read();
  d.gz = (Wire.read() << 8) | Wire.read();
  return true;
}

void calibrateGyro() {
  Serial.println("Keep transmitter completely still.");
  Serial.println("Calibrating gyroscope...");
  const int samples = 500;
  long sx = 0, sy = 0, sz = 0;
  int validSamples = 0;

  for (int i = 0; i < samples; i++) {
    MPUData d;
    if (readMPU(d)) {
      sx += d.gx; sy += d.gy; sz += d.gz;
      validSamples++;
    }
    delay(5);
  }

  if (validSamples < 450) {
    Serial.println("MPU6050 read failure!");
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }

  gyroBiasX = (float)sx / validSamples;
  gyroBiasY = (float)sy / validSamples;
  gyroBiasZ = (float)sz / validSamples;
  Serial.println("Gyroscope calibration complete.");
}

void updateOrientation(float dt) {
  MPUData d;
  if (!readMPU(d)) return;

  float ax = d.ax, ay = d.ay, az = d.az;
  float accelRoll = atan2f(ay, az) * RAD_TO_DEG_F;
  float accelPitch =
      atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG_F;

  float gyroRollRate = (d.gx - gyroBiasX) / 131.0f;
  float gyroPitchRate = (d.gy - gyroBiasY) / 131.0f;

  if (!filterInitialized) {
    filteredRoll = accelRoll;
    filteredPitch = accelPitch;
    filterInitialized = true;
  } else {
    filteredRoll = 0.98f * (filteredRoll + gyroRollRate * dt)
                   + 0.02f * accelRoll;
    filteredPitch = 0.98f * (filteredPitch + gyroPitchRate * dt)
                    + 0.02f * accelPitch;
  }
}

void onDataReceived(const esp_now_recv_info_t *info,
                    const uint8_t *data, int len) {
  if (len != sizeof(Packet)) return;
  Packet p;
  memcpy(&p, data, sizeof(p));
  if (p.type == TYPE_ACK) {
    lastAckSeq = p.seq;
    lastAckMs = millis();
  }
}

void setupWireless() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onDataReceived);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddress, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;

  if (!esp_now_is_peer_exist(broadcastAddress)) {
    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.println("Peer setup failed!");
      while (true) delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  writeMPU(0x6B, 0x00);  // Wake MPU6050
  writeMPU(0x1B, 0x00);  // Gyroscope +/-250 deg/s
  writeMPU(0x1C, 0x00);  // Accelerometer +/-2g
  delay(100);

  MPUData test;
  if (!readMPU(test)) {
    Serial.println("MPU6050 not detected at 0x68!");
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }

  calibrateGyro();
  updateOrientation(0.01f);
  referencePitch = filteredPitch;
  referenceRoll = filteredRoll;
  calibrated = true;
  previousMicros = micros();

  setupWireless();
  Serial.println("Transmitter ready.");
  Serial.println("Press button to recalibrate.");
}

void loop() {
  uint32_t nowUs = micros();
  float dt = (nowUs - previousMicros) / 1000000.0f;
  previousMicros = nowUs;
  if (dt <= 0 || dt > 0.1f) dt = 0.01f;

  updateOrientation(dt);

  bool buttonPressed = digitalRead(BUTTON_PIN) == LOW;
  if (buttonPressed && !previousButtonState) {
    referencePitch = filteredPitch;
    referenceRoll = filteredRoll;
    Serial.println("Neutral reference updated.");
  }
  previousButtonState = buttonPressed;

  if (millis() - lastSendMs >= 40) {
    lastSendMs = millis();
    Packet p;
    p.type = TYPE_CONTROL;
    p.seq = ++sequenceNumber;
    p.pitch = (filteredPitch - referencePitch) * PITCH_SIGN;
    p.roll = (filteredRoll - referenceRoll) * ROLL_SIGN;
    p.calibrated = calibrated ? 1 : 0;
    esp_now_send(broadcastAddress, (uint8_t *)&p, sizeof(p));
    Serial.printf("Pitch: %.1f  Roll: %.1f\n", p.pitch, p.roll);
  }

  digitalWrite(LED_PIN,
      (millis() - lastAckMs < 500 && lastAckMs != 0) ? HIGH : LOW);
  delay(2);
}
