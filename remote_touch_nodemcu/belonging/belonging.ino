#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

#include "secret.h"

// NOTE on pin selections
// - D4 needs to be high on boot. No issue since it is an LED.
// - D0 needs to be high on boot. Need to make sure PSoC doesn't hold low.
// - D10 is the TX pin. Serial is not availiable.

// Pins ~~~~
#define PIN_LED_ESP         D4   // LED on ESP module
#define PIN_IS_TOUCHED      D0   // local touch state (from PSoC)
#define PIN_REMOTE_TOUCHED  D10  // remote touch state (signals PSoC to heat)
#define PIN_RGB_STRIP       D2   // RGB led strip data (WS281x)

// WiFi ~~~~
#define IS_SERVER true  // Change to false on the receiver board

const char* ssid = WIFI_SSID;            // defined in secret.h
const char* password = WIFI_PASS;        // defined in secret.h
const char* serverIP = "192.168.1.101";  // IP address of the server ESP8266

#if IS_SERVER
IPAddress local_IP(192, 168, 1, 101);  // must match serverIP above
#else
IPAddress local_IP(192, 168, 1, 202);
#endif
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

WiFiClient client;
WiFiServer server(80);

// LEDs ~~~~
#define NUM_LEDS        43
#define MAX_BRIGHTNESS  255
#define CHANGE_TIME_MS  2000
#define NUM_STEPS       100
#define STEP_SIZE_MS    (CHANGE_TIME_MS / NUM_STEPS)
#define STEP_SIZE_LED   (MAX_BRIGHTNESS / NUM_STEPS)
Adafruit_NeoPixel rgb_leds = Adafruit_NeoPixel(NUM_LEDS, PIN_RGB_STRIP, NEO_RGB + NEO_KHZ800);

#define SEND_FREQ_MS     10
#define DATA_TIMEOUT_MS  5000
Ticker ticker;

void rgb_strip_handler() {
  static int r = 0;
  static int g = 0;
  static int b = 0;
  static uint32_t led_strip_timer = millis();

  if (millis() - led_strip_timer > STEP_SIZE_MS) {
    int dir = digitalRead(PIN_REMOTE_TOUCHED) ? 1 : -1;
    r += dir * STEP_SIZE_LED;

    if (r > MAX_BRIGHTNESS) r = MAX_BRIGHTNESS;
    else if (r < 0) r = 0;

    set_rgb_strip(r,g,b);
    led_strip_timer = millis();
  }
}

// Toggles LED on ESP module
void toggle_esp_led() {
  digitalWrite(PIN_LED_ESP, !digitalRead(PIN_LED_ESP));
}

void set_rgb_strip(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    rgb_leds.setPixelColor(i, rgb_leds.Color(r,g,b));
  }
  rgb_leds.show();
}

void setup_wifi() {
  WiFi.config(local_IP, gateway, subnet);

  ticker.attach(0.5, toggle_esp_led);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  ticker.detach();
  digitalWrite(PIN_LED_ESP, HIGH);
}

void setup() {
  delay(10);

  // setup pins
  pinMode(PIN_LED_ESP, OUTPUT);
  pinMode(PIN_IS_TOUCHED, INPUT_PULLUP);
  pinMode(PIN_REMOTE_TOUCHED, OUTPUT);
  pinMode(PIN_RGB_STRIP, OUTPUT);

  set_rgb_strip(0,0,0);  // turn off LED strip

  setup_wifi();

#if IS_SERVER
    server.begin();  // Set up the server
#else
    client.connect(serverIP, 80);  // Set up the client to connect to the server
#endif
}

void send_local_touch_state() {
  static uint32_t send_timer = millis();
  if (millis() - send_timer > SEND_FREQ_MS) {
    if (client.connected()) {
      client.write(digitalRead(PIN_IS_TOUCHED));
      send_timer = millis();
    }
  }
}

void remote_touch_handler() {
  static uint32_t data_timer = millis();
  if (millis() - data_timer > DATA_TIMEOUT_MS) {
      // If connection is lost, turn off heating and flash the ESP LED
      if (!ticker.active()) ticker.attach(0.1, toggle_esp_led);
      digitalWrite(PIN_REMOTE_TOUCHED, LOW);
  }
#if IS_SERVER
  client = server.available();
  if(!client) return;
#endif
  if (client.connected()) {
    if (client.available()) {
      // reset timeout timer, stop flashing ESP LED
      data_timer = millis();
      if (ticker.active()) ticker.detach();

      if (client.read() == 0) {
        digitalWrite(PIN_LED_ESP, LOW);
        digitalWrite(PIN_REMOTE_TOUCHED, LOW);
      } else {
        digitalWrite(PIN_LED_ESP, HIGH);
        digitalWrite(PIN_REMOTE_TOUCHED, HIGH);
      }
    }
  } else {
#if !IS_SERVER
  client.connect(serverIP, 80);
#endif
  }
}

void loop() {
  send_local_touch_state();
  remote_touch_handler();
  rgb_strip_handler();
}
