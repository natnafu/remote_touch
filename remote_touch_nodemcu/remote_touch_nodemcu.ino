#include <Adafruit_NeoPixel.h>
#include <BlynkSimpleEsp8266.h>
#include <Ticker.h>
#include <WiFiManager.h>

#include "secret.h"  // add to .gitignore, holds BLYNK tokens

// Pins
#define LED_ESP           D4     // LED near ESP module
#define PIN_TOUCH         D0     // local touch state (from PSoC)
#define PIN_REMOTE_TOUCH  V1     // remote touch state (from remote)
#define PIN_HEATER        D10    // signals PSoC to heat
#define PIN_RGB_LED       D2     // WS2812b data pin
// Time
#define UPDATE_RATE       100    // local state update rate, units ms
#define HEATER_TIMEOUT    30000  // heater timeout, units ms

#define NUM_LEDS          1
Adafruit_NeoPixel rgd_leds = Adafruit_NeoPixel(NUM_LEDS, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);


uint8_t local_state;         // local touch state
uint32_t timer_check_local;  // local update timer
uint32_t remote_state;       // remote state (doubles as time since touch)

WidgetBridge esp_bridge(V0);
Ticker ticker;

// Toggles LED on ESP module
void toggle_led() {
  digitalWrite(LED_ESP, !digitalRead(LED_ESP));
}

// Connect to WiFi
void setup_wifi() {
  WiFiManager wifi_manager;
  wifi_manager.setTimeout(180);

  // Setup unique SSID for AP
  char ssid[16];
  sprintf(ssid, "Frame-0x%08x", ESP.getChipId());

  // Connect to WiFi network, make AP if fails
  if(!wifi_manager.autoConnect(ssid)) {
    ESP.reset();  // reset if timeout
    while(1);
  }
}

// Connect to Blynk
void setup_blynk() {
  String wifi_ssid = WiFi.SSID();
  String wifi_pass = WiFi.psk();
  char blynk_ssid[wifi_ssid.length()];
  char blynk_pass[wifi_pass.length()];
  wifi_ssid.toCharArray(blynk_ssid, wifi_ssid.length());
  wifi_pass.toCharArray(blynk_pass, wifi_pass.length());
  Blynk.begin(TOKEN, blynk_ssid, blynk_pass);
}

// Setup connection to remote device
BLYNK_CONNECTED() {
  esp_bridge.setAuthToken(REMOTE_TOKEN);
}

// Responds to remote touch state
BLYNK_WRITE(PIN_REMOTE_TOUCH)
{
  if (param.asInt()) remote_state = millis();
  else remote_state = 0;
  digitalWrite(PIN_HEATER, remote_state);
  digitalWrite(LED_ESP, !remote_state);
  update_rgb_leds();
}

void update_rgb_leds() {
  uint8_t red = 0;
  uint8_t blu = 0;
  if (remote_state) red = 255;
  if (local_state)  blu = 255;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    rgd_leds.setPixelColor(i, rgd_leds.Color(red,0,blu));
  }
  rgd_leds.show();
}

void setup() {
  pinMode(LED_ESP, OUTPUT);
  digitalWrite(LED_ESP, HIGH);

  pinMode(PIN_REMOTE_TOUCH, OUTPUT);
  pinMode(PIN_TOUCH, INPUT_PULLUP);

  pinMode(PIN_RGB_LED, OUTPUT);

  // blink ESP LED until connection is made
  ticker.attach(0.5, toggle_led);

  setup_wifi();
  setup_blynk();

  ticker.detach();
  digitalWrite(LED_ESP, HIGH);

  timer_check_local = millis();
  local_state = digitalRead(PIN_TOUCH);  // initial local state
  Blynk.syncVirtual(PIN_REMOTE_TOUCH);   // initial remote state

  rgd_leds.begin();
  rgd_leds.show();
}

void loop() {

  // Check local state
  if (millis() - timer_check_local >= UPDATE_RATE) {
    // Check for local touch state change
    if (!local_state != !digitalRead(PIN_TOUCH)) {
      local_state = !local_state;
      esp_bridge.virtualWrite(PIN_REMOTE_TOUCH, local_state);
      update_rgb_leds();
    }

    // Reset timer
    timer_check_local = millis();
  }

  // Check for heater timeout
  if (remote_state && (millis() - remote_state >= HEATER_TIMEOUT)) {
    digitalWrite(PIN_HEATER, 0);
    digitalWrite(LED_ESP, 0);
  }

  Blynk.run();
}
