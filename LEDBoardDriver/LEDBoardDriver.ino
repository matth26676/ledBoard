/*
    LEDBoard ESP32 Firmware

    This firmware receives display data from a Raspberry Pi over SPI,
    stores the full frame in memory, and renders the frame to WS2812B LEDs.

    IMPORTANT:
    This code is timing-sensitive.

    Changes to:
    - SPI timing
    - READY synchronization
    - rendering order
    may cause instability.

    The ESP32 only renders AFTER receiving all 64 rows.
*/

#include <FastLED.h>
#include "driver/spi_slave.h"

// =====================================================
// DISPLAY CONFIGURATION
// =====================================================

#define WIDTH 128
#define HEIGHT 64

// =====================================================
// LED OUTPUT PINS
// =====================================================

#define DATA_PIN_1 15
#define DATA_PIN_2 2
#define DATA_PIN_3 22
#define DATA_PIN_4 21

// =====================================================
// SPI PINS
// =====================================================

#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_SCLK 18
#define PIN_CS   5

// READY line
// HIGH = ESP32 ready for next packet
#define READY_PIN 4

// =====================================================
// SPI PACKET SIZE
// =====================================================

#define PACKET_SIZE 512

// =====================================================
// LED ARRAYS
// =====================================================

// Four physical LED sections
CRGB leds1[2048];
CRGB leds2[2048];
CRGB leds3[2048];
CRGB leds4[2048];

// =====================================================
// FRAME BUFFER
// =====================================================

// Stores the completed frame before rendering
CRGB frameBuffer[WIDTH * HEIGHT];

// Tracks whether each row has been received
bool rowReceived[HEIGHT];

// =====================================================
// LED MAP
// =====================================================

// Maps logical XY coordinates to physical LEDs
CRGB* ledMap[WIDTH * HEIGHT];

// =====================================================
// SPI RECEIVE BUFFER
// =====================================================

static uint8_t rxBuf[PACKET_SIZE] __attribute__((aligned(32)));

spi_slave_transaction_t t;

// =====================================================
// BUILD LED MAP
// =====================================================

void buildMap() {

  /*
      The display is composed of multiple
      16x16 serpentine sections.

      This function converts logical X/Y coordinates
      into physical LED indices.
  */

  for (int y = 0; y < HEIGHT; y++) {

    for (int x = 0; x < WIDTH; x++) {

      int px = x / 16;
      int py = y / 16;

      int lx = x % 16;
      int ly = y % 16;

      // Serpentine LED layout
      int idx = (lx % 2 == 0)
        ? lx * 16 + ly
        : lx * 16 + (15 - ly);

      int ledIndex = px * 256 + idx;

      if (py == 0) {
        ledMap[y * WIDTH + x] = &leds1[ledIndex];
      }
      else if (py == 1) {
        ledMap[y * WIDTH + x] = &leds2[ledIndex];
      }
      else if (py == 2) {
        ledMap[y * WIDTH + x] = &leds3[ledIndex];
      }
      else {
        ledMap[y * WIDTH + x] = &leds4[ledIndex];
      }
    }
  }
}

// =====================================================
// COPY FRAME BUFFER TO LEDS
// =====================================================

void renderFrame() {

  for (int i = 0; i < WIDTH * HEIGHT; i++) {
    *ledMap[i] = frameBuffer[i];
  }

  FastLED.show();
}

// =====================================================
// CHECK IF FULL FRAME RECEIVED
// =====================================================

bool allRowsReceived() {

  for (int i = 0; i < HEIGHT; i++) {

    if (!rowReceived[i]) {
      return false;
    }
  }

  return true;
}

// =====================================================
// RESET ROW TRACKING
// =====================================================

void clearRowTracking() {

  for (int i = 0; i < HEIGHT; i++) {
    rowReceived[i] = false;
  }
}

// =====================================================
// PROCESS SPI PACKET
// =====================================================

void processPacket(uint8_t* buf) {

  // Validate packet header
  if (buf[0] != 0xA5 || buf[1] != 0x5A) {
    return;
  }

  uint8_t row = buf[2];
  uint8_t width = buf[3];

  // Validate row and width
  if (row >= HEIGHT || width != WIDTH) {
    return;
  }

  int i = 4;
  int base = row * WIDTH;

  // Copy row into frame buffer
  for (int x = 0; x < WIDTH; x++) {

    CRGB color;

    color.r = buf[i++];
    color.g = buf[i++];
    color.b = buf[i++];

    frameBuffer[base + x] = color;
  }

  rowReceived[row] = true;

  Serial.printf("ROW %d OK\n", row);

  // Only render once entire frame is complete
  if (allRowsReceived()) {

    Serial.println("FRAME COMPLETE");

    renderFrame();
    clearRowTracking();
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(READY_PIN, OUTPUT);
  digitalWrite(READY_PIN, LOW);

  FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds1, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds2, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds3, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_4, GRB>(leds4, 2048);

  FastLED.setBrightness(10);

  buildMap();
  clearRowTracking();

  // SPI BUS CONFIG
  spi_bus_config_t buscfg = {};

  buscfg.mosi_io_num = PIN_MOSI;
  buscfg.miso_io_num = PIN_MISO;
  buscfg.sclk_io_num = PIN_SCLK;

  // SPI SLAVE CONFIG
  spi_slave_interface_config_t slvcfg = {};

  slvcfg.spics_io_num = PIN_CS;
  slvcfg.queue_size = 1;
  slvcfg.mode = 0;

  spi_slave_initialize(
    VSPI_HOST,
    &buscfg,
    &slvcfg,
    SPI_DMA_CH_AUTO
  );

  Serial.println("ESP READY");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  memset(&t, 0, sizeof(t));

  t.length = PACKET_SIZE * 8;
  t.rx_buffer = rxBuf;

  // Signal Pi that ESP32 is ready
  digitalWrite(READY_PIN, HIGH);

  // Wait for SPI packet
  spi_slave_transmit(
    VSPI_HOST,
    &t,
    portMAX_DELAY
  );

  // Lower READY after transfer
  digitalWrite(READY_PIN, LOW);

  processPacket(rxBuf);
}
