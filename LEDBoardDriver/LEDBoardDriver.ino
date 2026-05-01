#include <FastLED.h>
#include "driver/spi_slave.h"

#define WIDTH 128
#define HEIGHT 64

CRGB framebuffer[WIDTH][HEIGHT];

#define DATA_PIN_1 15
#define DATA_PIN_2 2
#define DATA_PIN_3 22
#define DATA_PIN_4 21

CRGB leds1[2048];
CRGB leds2[2048];
CRGB leds3[2048];
CRGB leds4[2048];

#define PIN_MOSI 23
#define PIN_SCLK 18
#define PIN_CS   5

uint8_t rxBuf[64];

spi_slave_transaction_t t;

// --------------------
// APPLY PIXEL
// --------------------
void setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < WIDTH && y < HEIGHT) {
    framebuffer[x][y] = CRGB(r, g, b);
  }
}

// --------------------
// RENDER
// --------------------
void showFrame() {

  for (int i = 0; i < 2048; i++) {
    leds1[i] = 0;
    leds2[i] = 0;
    leds3[i] = 0;
    leds4[i] = 0;
  }

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      CRGB c = framebuffer[x][y];

      int px = x / 16;
      int py = y / 16;

      int lx = x % 16;
      int ly = y % 16;

      int idx = (lx % 2 == 0)
        ? lx * 16 + ly
        : lx * 16 + (15 - ly);

      int ledIndex = px * 256 + idx;

      if (py == 0) leds1[ledIndex] = c;
      else if (py == 1) leds2[ledIndex] = c;
      else if (py == 2) leds3[ledIndex] = c;
      else if (py == 3) leds4[ledIndex] = c;
    }
  }

  FastLED.show();
}

// --------------------
// PARSER STATE
// --------------------
uint8_t state = 0;
uint8_t x, y, r, g;

void processByte(uint8_t b) {

  switch (state) {

    case 0:
      if (b == 0xAA) state = 1;
      break;

    case 1:
      x = b;
      state = 2;
      break;

    case 2:
      y = b;
      state = 3;
      break;

    case 3:
      r = b;
      state = 4;
      break;

    case 4:
      g = b;
      state = 5;
      break;

    case 5:
      setPixel(x, y, r, g, b);
      state = 0;
      break;
  }
}

// --------------------
// SETUP
// --------------------
void setup() {

  Serial.begin(115200);

  FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds1, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds2, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds3, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_4, GRB>(leds4, 2048);

  FastLED.setBrightness(10);

  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = PIN_MOSI;
  buscfg.sclk_io_num = PIN_SCLK;
  buscfg.miso_io_num = -1;

  spi_slave_interface_config_t slvcfg = {};
  slvcfg.spics_io_num = PIN_CS;
  slvcfg.mode = 0;
  slvcfg.queue_size = 3;

  spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
}

// --------------------
// LOOP
// --------------------
void loop() {

  t.length = 64 * 8;
  t.rx_buffer = rxBuf;

  if (spi_slave_transmit(VSPI_HOST, &t, portMAX_DELAY) == ESP_OK) {

    for (int i = 0; i < 64; i++) {
      processByte(rxBuf[i]);
    }

    static int counter = 0;
    if (++counter > 10) {
      showFrame();
      counter = 0;
    }
  }
}