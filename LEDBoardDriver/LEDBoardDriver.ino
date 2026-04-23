#include <FastLED.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =========================
// CONFIG
// =========================
#define PANEL_WIDTH 16
#define PANEL_HEIGHT 16
#define PANELS_X 4
#define PANELS_Y 8

#define DISPLAY_WIDTH (PANEL_WIDTH * PANELS_X)    // 64
#define DISPLAY_HEIGHT (PANEL_HEIGHT * PANELS_Y)  // 128

#define NUM_LEDS_PER_STRIP 2048

// =========================
// WIFI (FILL THIS IN)
// =========================
const char* ssid = "robonet";
const char* password = "formDog220!";

// =========================
// PINS
// =========================
#define DATA_PIN_1 5
#define DATA_PIN_2 18
#define DATA_PIN_3 19
#define DATA_PIN_4 21

// =========================
// STRIPS
// =========================
CRGB leds1[NUM_LEDS_PER_STRIP];
CRGB leds2[NUM_LEDS_PER_STRIP];
CRGB leds3[NUM_LEDS_PER_STRIP];
CRGB leds4[NUM_LEDS_PER_STRIP];

// =========================
// FRAMEBUFFER
// =========================
CRGB framebuffer[DISPLAY_WIDTH][DISPLAY_HEIGHT];

// =========================
// SETUP
// =========================
void setup() {

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds1, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds2, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds3, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, DATA_PIN_4, GRB>(leds4, NUM_LEDS_PER_STRIP);

  FastLED.setBrightness(10);
}

// =========================
// DRAW PIXEL
// =========================
void drawPixel(int x, int y, CRGB color) {
  if (x < 0 || x >= DISPLAY_WIDTH) return;
  if (y < 0 || y >= DISPLAY_HEIGHT) return;
  framebuffer[x][y] = color;
}

// =========================
// CLEAR FRAME
// =========================
void clearFrame() {
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
      framebuffer[x][y] = CRGB::Black;
    }
  }
}

// =========================
// HARDWARE MAPPING (YOUR WORKING VERSION)
// =========================
void showFrame() {

  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < DISPLAY_WIDTH; x++) {

      CRGB c = framebuffer[x][y];

      int panel_x = x / PANEL_WIDTH;
      int panel_y = y / PANEL_HEIGHT;

      int local_x = x % PANEL_WIDTH;
      int local_y = y % PANEL_HEIGHT;

      // column-serpentine (your confirmed hardware behavior)
      if (local_x % 2 == 1) {
        local_y = (PANEL_HEIGHT - 1) - local_y;
      }

      int index = local_x * PANEL_HEIGHT + local_y;
      int panel_index = panel_x * (PANEL_WIDTH * PANEL_HEIGHT) + index;

      if (panel_y == 0) leds1[panel_index] = c;
      else if (panel_y == 1) leds2[panel_index] = c;
      else if (panel_y == 2) leds3[panel_index] = c;
      else if (panel_y == 3) leds4[panel_index] = c;
    }
  }

  FastLED.show();
}

// =========================
// API FETCH (SKELETON - SAFE)
// =========================
void fetchAPI() {

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("https://formplace.yorktechapps.com/api/canvas.json");

  int code = http.GET();
  if (code != 200) {
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  Serial.println("Got API response:");
  Serial.println(payload.substring(0, 300));

  // -------------------------
  // PLACEHOLDER PARSING
  // -------------------------
  // We do NOT assume structure yet.

  StaticJsonDocument<20000> doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.println("JSON parse failed");
    return;
  }

  // TEMP DEBUG ONLY (we will replace once structure is known)
  clearFrame();

  drawPixel(0, 0, CRGB::Red);
  drawPixel(DISPLAY_WIDTH - 1, 0, CRGB::Green);
  drawPixel(0, DISPLAY_HEIGHT - 1, CRGB::Blue);
  drawPixel(DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, CRGB::Yellow);
}

// =========================
// LOOP
// =========================
void loop() {

  fetchAPI();

  showFrame();

  delay(1000);
}