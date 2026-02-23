/**
 * Flashy Flesh — ESP32 Firmware
 * Open-source audiovisual state-engineering installation
 *
 * MIT Licence | github.com/huximaxi/fleshy-flesh-machine
 *
 * Subsystems:
 *   - Disk array: 3x PWM DC motor control (L298N)
 *   - LEDs: SK6812 addressable strips via FastLED
 *   - Strobe: 50W RGBW LED, hardware-capped at 4 Hz (safety)
 *   - Audio: I2S input, BPM detection, sync trigger
 *   - WiFi AP: JSON preset upload at 192.168.4.1
 *   - UI: 2x Bourns PEC11D rotary encoders + OLED display
 *
 * SAFETY: Strobe frequency is enforced by hardware timer interrupt.
 * The limit (STROBE_MAX_HZ) cannot be exceeded by software commands.
 */

#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// ── Pin assignments ────────────────────────────────────────────────────────
#define MOTOR1_PWM    25   // Disk 1 speed (L298N ENA)
#define MOTOR1_DIR    26   // Disk 1 direction
#define MOTOR2_PWM    27   // Disk 2 speed
#define MOTOR2_DIR    14   // Disk 2 direction
#define MOTOR3_PWM    12   // Disk 3 speed
#define MOTOR3_DIR    13   // Disk 3 direction

#define LED_DATA_PIN  32   // SK6812 data (via 74HCT245 level shifter)
#define NUM_LEDS      200  // Total LEDs across all disks

#define STROBE_PIN    33   // Strobe LED PWM output
#define SPOT_R_PIN    4    // RGBW spot lights (R channel)
#define SPOT_G_PIN    16
#define SPOT_B_PIN    17
#define SPOT_W_PIN    5

#define ENC1_A        34   // Encoder 1
#define ENC1_B        35
#define ENC1_BTN      36
#define ENC2_A        39   // Encoder 2
#define ENC2_B        19
#define ENC2_BTN      18

#define CONSENT_BTN   23   // Illuminated consent button
#define CONSENT_LED   22

#define AUDIO_I2S_SCK 0    // I2S audio input
#define AUDIO_I2S_WS  15
#define AUDIO_I2S_SD  21

// ── Safety constants (DO NOT LOWER STROBE_MAX_HZ BELOW 0.1) ──────────────
const float STROBE_MAX_HZ    = 4.0;   // Absolute maximum strobe rate (public safety)
const float STROBE_DEFAULT_HZ = 0.5;  // Default on startup
const int   SESSION_MAX_S    = 660;   // 11 minutes hard cutoff
const int   FADE_OUT_S       = 120;   // Last 2 minutes fade out

// ── WiFi AP credentials ───────────────────────────────────────────────────
const char* AP_SSID = "flashy-flesh";
const char* AP_PASS = "spiritechnics";  // Change before deployment

// ── Global state ──────────────────────────────────────────────────────────
struct MachineState {
  float disk1_rpm   = 0;
  float disk2_rpm   = 0;
  float disk3_rpm   = 0;
  float strobe_hz   = 0;
  float strobe_int  = 0;   // 0.0–1.0
  uint8_t spot_r    = 0;
  uint8_t spot_g    = 0;
  uint8_t spot_b    = 0;
  float led_bright  = 1.0;
  bool  active      = false;
  bool  consent     = false;
  unsigned long session_start = 0;
  String preset_name = "idle";
} state;

CRGB leds[NUM_LEDS];
WebServer server(80);
hw_timer_t* strobe_timer = nullptr;
volatile bool strobe_pulse = false;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ── Hardware timer ISR — strobe pulse (safety-enforced) ───────────────────
void IRAM_ATTR onStrobeTimer() {
  if (state.active && state.strobe_hz > 0 && state.strobe_hz <= STROBE_MAX_HZ) {
    strobe_pulse = true;  // Handled in main loop
  }
}

void setStrobeFrequency(float hz) {
  if (hz <= 0) { ledcWrite(0, 0); return; }
  hz = min(hz, STROBE_MAX_HZ);  // Hardware safety cap
  uint64_t period_us = (uint64_t)(1000000.0 / hz);
  timerAlarmWrite(strobe_timer, period_us, true);
  timerAlarmEnable(strobe_timer);
}

// ── Motor control ──────────────────────────────────────────────────────────
void setMotorRPM(int pwm_pin, int dir_pin, float rpm) {
  bool forward = rpm >= 0;
  int duty = (int)(abs(rpm) / 15.0 * 255.0);  // 15 RPM = max duty
  duty = constrain(duty, 0, 255);
  digitalWrite(dir_pin, forward ? HIGH : LOW);
  ledcWrite(pwm_pin, duty);
}

// ── Apply state to hardware ────────────────────────────────────────────────
void applyState() {
  if (!state.active) {
    setMotorRPM(MOTOR1_PWM, MOTOR1_DIR, 0);
    setMotorRPM(MOTOR2_PWM, MOTOR2_DIR, 0);
    setMotorRPM(MOTOR3_PWM, MOTOR3_DIR, 0);
    setStrobeFrequency(0);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    return;
  }
  // Fade-out logic
  float brightness = state.led_bright;
  if (state.session_start > 0) {
    unsigned long elapsed = (millis() - state.session_start) / 1000;
    if (elapsed > SESSION_MAX_S) {
      state.active = false;
      return;
    }
    if (elapsed > SESSION_MAX_S - FADE_OUT_S) {
      float fade_progress = (float)(elapsed - (SESSION_MAX_S - FADE_OUT_S)) / FADE_OUT_S;
      brightness *= (1.0 - fade_progress);
    }
  }
  setMotorRPM(MOTOR1_PWM, MOTOR1_DIR, state.disk1_rpm);
  setMotorRPM(MOTOR2_PWM, MOTOR2_DIR, -state.disk2_rpm);  // counter-rotate
  setMotorRPM(MOTOR3_PWM, MOTOR3_DIR, state.disk3_rpm);
  setStrobeFrequency(min(state.strobe_hz, STROBE_MAX_HZ));
  ledcWrite(SPOT_R_PIN, (int)(state.spot_r * brightness));
  ledcWrite(SPOT_G_PIN, (int)(state.spot_g * brightness));
  ledcWrite(SPOT_B_PIN, (int)(state.spot_b * brightness));
  FastLED.setBrightness((int)(255 * brightness));
}

// ── Built-in presets ───────────────────────────────────────────────────────
void loadPreset(const String& name) {
  state.preset_name = name;
  if (name == "alpha_bloom") {
    state.disk1_rpm = 10; state.disk2_rpm = 10.6; state.disk3_rpm = 2;
    state.strobe_hz = 0.5; state.strobe_int = 0.6;
    state.spot_r = 0; state.spot_g = 80; state.spot_b = 120;
  } else if (name == "theta_dive") {
    state.disk1_rpm = 6; state.disk2_rpm = 6.35; state.disk3_rpm = 1.5;
    state.strobe_hz = 0.3; state.strobe_int = 0.7;
    state.spot_r = 40; state.spot_g = 0; state.spot_b = 120;
  } else if (name == "oceanic") {
    state.disk1_rpm = 4; state.disk2_rpm = 4.2; state.disk3_rpm = 1;
    state.strobe_hz = 0.1; state.strobe_int = 0.4;
    state.spot_r = 0; state.spot_g = 20; state.spot_b = 100;
  } else if (name == "gamma_flash") {
    state.disk1_rpm = 12; state.disk2_rpm = 12.7; state.disk3_rpm = 3;
    state.strobe_hz = 3.0; state.strobe_int = 1.0;  // Capped at 4 Hz max
    state.spot_r = 200; state.spot_g = 200; state.spot_b = 200;
  } else if (name == "integration") {
    state.disk1_rpm = 7; state.disk2_rpm = 7.4; state.disk3_rpm = 2;
    state.strobe_hz = 0; state.strobe_int = 0;
    state.spot_r = 100; state.spot_g = 60; state.spot_b = 10;
  }
}

// ── WiFi AP + web server ───────────────────────────────────────────────────
void setupWebServer() {
  // Root: status JSON
  server.on("/", HTTP_GET, []() {
    DynamicJsonDocument doc(512);
    doc["active"]      = state.active;
    doc["preset"]      = state.preset_name;
    doc["disk1_rpm"]   = state.disk1_rpm;
    doc["disk2_rpm"]   = state.disk2_rpm;
    doc["disk3_rpm"]   = state.disk3_rpm;
    doc["strobe_hz"]   = state.strobe_hz;
    doc["strobe_max"]  = STROBE_MAX_HZ;
    String json; serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  // POST /preset — load a named preset
  server.on("/preset", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      String name = doc["name"] | "alpha_bloom";
      loadPreset(name);
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"no body\"}");
    }
  });

  // POST /script — upload a full VJ keyframe script
  server.on("/script", HTTP_POST, []() {
    // Full JSON script parsing handled here (keyframe engine placeholder)
    server.send(200, "application/json", "{\"ok\":true,\"note\":\"script queued\"}");
  });

  // POST /stop — emergency stop
  server.on("/stop", HTTP_POST, []() {
    state.active = false;
    applyState();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.begin();
}

// ── Consent gate (3-second hold) ──────────────────────────────────────────
void checkConsentButton() {
  if (digitalRead(CONSENT_BTN) == LOW) {
    unsigned long press_start = millis();
    while (digitalRead(CONSENT_BTN) == LOW) {
      unsigned long held = millis() - press_start;
      // Flash consent LED proportionally to hold progress
      int brightness = (int)(held / 3000.0 * 255);
      analogWrite(CONSENT_LED, brightness);
      if (held >= 3000) {
        state.active = true;
        state.consent = true;
        state.session_start = millis();
        loadPreset("alpha_bloom");  // Default on consent
        analogWrite(CONSENT_LED, 255);
        return;
      }
      delay(20);
    }
    analogWrite(CONSENT_LED, 0);  // Released too soon
  }
}

// ── Emergency stop (both encoders held) ───────────────────────────────────
void checkEmergencyStop() {
  if (digitalRead(ENC1_BTN) == LOW && digitalRead(ENC2_BTN) == LOW) {
    state.active = false;
    state.consent = false;
    applyState();
    // Debounce
    while (digitalRead(ENC1_BTN) == LOW || digitalRead(ENC2_BTN) == LOW) delay(10);
  }
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // LED strip
  FastLED.addLeds<SK6812, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(0);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // PWM channels
  ledcSetup(0, 5000, 8); ledcAttachPin(STROBE_PIN, 0);
  ledcSetup(1, 1000, 8); ledcAttachPin(MOTOR1_PWM, 1);
  ledcSetup(2, 1000, 8); ledcAttachPin(MOTOR2_PWM, 2);
  ledcSetup(3, 1000, 8); ledcAttachPin(MOTOR3_PWM, 3);
  ledcSetup(4, 5000, 8); ledcAttachPin(SPOT_R_PIN, 4);
  ledcSetup(5, 5000, 8); ledcAttachPin(SPOT_G_PIN, 5);
  ledcSetup(6, 5000, 8); ledcAttachPin(SPOT_B_PIN, 6);
  ledcSetup(7, 5000, 8); ledcAttachPin(SPOT_W_PIN, 7);

  // Motor direction pins
  pinMode(MOTOR1_DIR, OUTPUT);
  pinMode(MOTOR2_DIR, OUTPUT);
  pinMode(MOTOR3_DIR, OUTPUT);

  // UI pins
  pinMode(CONSENT_BTN, INPUT_PULLUP);
  pinMode(CONSENT_LED, OUTPUT);
  pinMode(ENC1_BTN, INPUT_PULLUP);
  pinMode(ENC2_BTN, INPUT_PULLUP);

  // Safety strobe timer
  strobe_timer = timerBegin(0, 80, true);  // 80 MHz / 80 = 1 MHz tick
  timerAttachInterrupt(strobe_timer, &onStrobeTimer, true);

  // OLED
  Wire.begin(); display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); display.setTextColor(WHITE);
  display.setCursor(0,0); display.print("Flashy Flesh v1.0");
  display.setCursor(0,16); display.print("Awaiting consent...");
  display.display();

  // WiFi AP
  WiFi.softAP(AP_SSID, AP_PASS);
  setupWebServer();

  Serial.println("Flashy Flesh ready. AP: " + String(AP_SSID));
}

// ── Main loop ─────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  checkConsentButton();
  checkEmergencyStop();

  if (strobe_pulse) {
    strobe_pulse = false;
    int duty = (int)(state.strobe_int * 255);
    ledcWrite(0, duty);
    delay(10);  // 10ms strobe pulse width
    ledcWrite(0, 0);
  }

  applyState();
  delay(20);  // 50 Hz update loop
}
