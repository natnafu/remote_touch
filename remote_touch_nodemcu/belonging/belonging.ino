#include <ESP8266WiFi.h>
#include <Ticker.h>

#include "secret.h"

// NOTE on pin selections
// - D4 needs to be high on boot. No issue since it is an LED.
// - D0 needs to be high on boot. Need to make sure PSoC doesn't hold low.
// - D10 is the TX pin. Serial is not availiable.

#define PIN_LED_ESP D4          // LED on ESP module
#define PIN_IS_TOUCHED D0       // local touch state (from PSoC)
#define PIN_REMOTE_TOUCHED D10  // remote touch state (signals PSoC to heat)

#define IS_SERVER false  // Change to false on the receiver board

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
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

Ticker ticker;

// Toggles LED on ESP module
void toggle_esp_led() {
  digitalWrite(PIN_LED_ESP, !digitalRead(PIN_LED_ESP));
}

void setup_wifi() {
  WiFi.config(local_IP, gateway, subnet);

  ticker.attach(0.1, toggle_esp_led);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  ticker.detach();
  digitalWrite(PIN_LED_ESP, HIGH);
}

void setup() {
  delay(10);

  // setup pins
  pinMode(PIN_LED_ESP, OUTPUT);
  pinMode(PIN_IS_TOUCHED, INPUT_PULLUP);
  pinMode(PIN_REMOTE_TOUCHED, OUTPUT);

  setup_wifi();

  if (IS_SERVER) {
    // Set up the server
    server.begin();
  } else {
    // Set up the client to connect to the server
    client.connect(serverIP, 80);
  }
}

void send_local_touch_state() {
  if (client.connected()) client.write(digitalRead(PIN_IS_TOUCHED));
}

void remote_touch_handler() {
#if IS_SERVER
  client = server.available();
  if(!client) return;
#endif
  if (client.connected()) {
    if (client.available()) {
      int receivedData = client.read();

      if (receivedData == 0) {
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
}
