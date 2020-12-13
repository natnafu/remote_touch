#include <BlynkSimpleEsp8266.h>
#include <Ticker.h>
#include <WiFiManager.h>

#include "secret.h"  // add to .gitignore, holds BLYNK tokens

#define LED_ESP           D4   // LED near ESP module
#define PIN_TOUCH         D0   // local touch state (from PSoC)
#define PIN_REMOTE_TOUCH  D10  // remote touch state

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

// Transmits touch state of local to remote
void check_touch() {
  static uint8_t state = digitalRead(PIN_TOUCH);

  if (!state != !digitalRead(PIN_TOUCH)) {
    state = !state;
    esp_bridge.digitalWrite(PIN_REMOTE_TOUCH, state);
  }
}

void setup() {
  pinMode(LED_ESP, OUTPUT);
  digitalWrite(LED_ESP, HIGH);

  pinMode(PIN_REMOTE_TOUCH, OUTPUT);
  pinMode(PIN_TOUCH, INPUT_PULLUP);

  // blink ESP LED until connection is made
  ticker.attach(0.5, toggle_led);

  setup_wifi();
  setup_blynk();

  ticker.detach();
  digitalWrite(LED_ESP, HIGH);

  // Check touch state every 100ms
  ticker.attach(0.1, check_touch);
}

void loop() {
  // Update LED based on remote pin state
  digitalWrite(LED_ESP, digitalRead(PIN_REMOTE_TOUCH));

  Blynk.run();
}
