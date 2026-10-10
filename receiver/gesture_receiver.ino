#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>

// Existing hardwired pins
const int IN1 = 26;
const int IN2 = 27;
const int IN3 = 14;
const int IN4 = 25;
const int LED_PIN = 23;

// Additional wires required for PWM speed control
const int ENA = 32;  // L298N ENA
const int ENB = 33;  // L298N ENB

const int PWM_FREQ = 15000;
const int PWM_RESOLUTION = 8;
const int MAX_PWM = 220;

const bool LEFT_MOTOR_INVERTED = false;
const bool RIGHT_MOTOR_INVERTED = false;

const float DEADZONE_DEG = 5.0f;
const float MAX_TILT_DEG = 30.0f;
const float MIN_COMMAND = 0.08f;

const uint8_t WIFI_CHANNEL = 1;
const uint32_t FAILSAFE_MS = 350;

uint8_t broadcastAddress[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

const uint8_t TYPE_CONTROL = 1;
const uint8_t TYPE_ACK = 2;

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint32_t seq;
  float pitch;
  float roll;
  uint8_t calibrated;
} Packet;

volatile float receivedPitch = 0;
volatile float receivedRoll = 0;
volatile uint8_t receivedCalibrated = 0;
volatile uint32_t receivedSequence = 0;
volatile uint32_t lastControlMs = 0;
volatile bool gotControl = false;
uint32_t lastAckSentSequence = 0;

void stopMotors() {
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void driveMotor(int speed, int pinA, int pinB,
                int enablePin, bool inverted) {
  speed = constrain(speed, -255, 255);
  if (inverted) speed = -speed;

  if (speed > 0) {
    digitalWrite(pinA, HIGH);
    digitalWrite(pinB, LOW);
  } else if (speed < 0) {
    digitalWrite(pinA, LOW);
    digitalWrite(pinB, HIGH);
  } else {
    digitalWrite(pinA, LOW);
    digitalWrite(pinB, LOW);
  }
  ledcWrite(enablePin, abs(speed));
}

float angleToAxis(float angle) {
  float magnitude = fabsf(angle);
  if (magnitude <= DEADZONE_DEG) return 0.0f;

  float value = (magnitude - DEADZONE_DEG) /
                (MAX_TILT_DEG - DEADZONE_DEG);
  value = constrain(value, 0.0f, 1.0f);
  return (angle < 0) ? -value : value;
}

void onDataReceived(const esp_now_recv_info_t *info,
                    const uint8_t *data, int len) {
  if (len != sizeof(Packet)) return;
  Packet p;
  memcpy(&p, data, sizeof(p));
  if (p.type != TYPE_CONTROL) return;

  receivedPitch = p.pitch;
  receivedRoll = p.roll;
  receivedCalibrated = p.calibrated;
  receivedSequence = p.seq;
  lastControlMs = millis();
  gotControl = true;
}

void setupWireless() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed!");
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
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  stopMotors();

  // Arduino-ESP32 core 3.x LEDC API
  bool leftPWM = ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
  bool rightPWM = ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);
  if (!leftPWM || !rightPWM) {
    Serial.println("PWM setup failed!");
    while (true) {
      stopMotors();
      delay(1000);
    }
  }
  stopMotors();
  setupWireless();
  Serial.println("Receiver ready. Waiting for transmitter...");
}

void loop() {
  uint32_t now = millis();
  bool connected = gotControl &&
      (uint32_t)(now - lastControlMs) < FAILSAFE_MS;

  digitalWrite(LED_PIN, connected ? HIGH : LOW);

  if (!connected || receivedCalibrated == 0) {
    stopMotors();
  } else {
    float pitch = receivedPitch;
    float roll = receivedRoll;
    float forward = angleToAxis(pitch);
    float steering = angleToAxis(roll);

    float left = forward - steering;
    float right = forward + steering;
    float largest = fmaxf(fabsf(left), fabsf(right));
    if (largest > 1.0f) {
      left /= largest;
      right /= largest;
    }

    if (fabsf(left) < MIN_COMMAND) left = 0;
    if (fabsf(right) < MIN_COMMAND) right = 0;

    int leftSpeed = (int)roundf(left * MAX_PWM);
    int rightSpeed = (int)roundf(right * MAX_PWM);
    driveMotor(leftSpeed, IN1, IN2, ENA, LEFT_MOTOR_INVERTED);
    driveMotor(rightSpeed, IN3, IN4, ENB, RIGHT_MOTOR_INVERTED);
  }

  uint32_t seq = receivedSequence;
  if (connected && seq != lastAckSentSequence) {
    Packet ack = {};
    ack.type = TYPE_ACK;
    ack.seq = seq;
    ack.calibrated = 1;
    esp_now_send(broadcastAddress, (uint8_t *)&ack, sizeof(ack));
    lastAckSentSequence = seq;
  }
  delay(5);
}
