#include <FastLED.h>
#include "driver/spi_slave.h"

#define WIDTH 128
#define HEIGHT 64

#define DATA_PIN_1 15
#define DATA_PIN_2 2
#define DATA_PIN_3 22
#define DATA_PIN_4 21

#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_SCLK 18
#define PIN_CS   5

#define READY_PIN 4

#define PACKET_SIZE 512

CRGB leds1[2048];
CRGB leds2[2048];
CRGB leds3[2048];
CRGB leds4[2048];

CRGB* ledMap[WIDTH * HEIGHT];

static uint8_t rxBuf[PACKET_SIZE] __attribute__((aligned(32)));

spi_slave_transaction_t t;

// ---------------- FRAME BUFFER ----------------
CRGB frame[HEIGHT][WIDTH];
bool rowReceived[HEIGHT];
int receivedCount = 0;

// -----------------------------
void buildMap() {

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      int px = x / 16;
      int py = y / 16;

      int lx = x % 16;
      int ly = y % 16;

      int idx = (lx % 2 == 0)
        ? lx * 16 + ly
        : lx * 16 + (15 - ly);

      int ledIndex = px * 256 + idx;

      if (py == 0) ledMap[y * WIDTH + x] = &leds1[ledIndex];
      else if (py == 1) ledMap[y * WIDTH + x] = &leds2[ledIndex];
      else if (py == 2) ledMap[y * WIDTH + x] = &leds3[ledIndex];
      else ledMap[y * WIDTH + x] = &leds4[ledIndex];
    }
  }
}

// ---------------- PROCESS PACKET ----------------
void processPacket(uint8_t* buf) {

  if (buf[0] != 0xA5 || buf[1] != 0x5A) return;

  uint8_t row = buf[2];
  uint8_t width = buf[3];

  if (row >= HEIGHT || width != WIDTH) return;

  int i = 4;

  for (int x = 0; x < WIDTH; x++) {

    frame[row][x].r = buf[i++];
    frame[row][x].g = buf[i++];
    frame[row][x].b = buf[i++];
  }

  if (!rowReceived[row]) {
    rowReceived[row] = true;
    receivedCount++;
  }

  Serial.printf("ROW %d STORED (%d/64)\n", row, receivedCount);
}

// ---------------- RENDER FULL FRAME ----------------
void renderFrame() {

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      *ledMap[y * WIDTH + x] = frame[y][x];
    }
  }

  FastLED.show();

  Serial.println("FRAME RENDERED");
}

// ---------------- RESET FRAME STATE ----------------
void resetFrame() {

  for (int i = 0; i < HEIGHT; i++) {
    rowReceived[i] = false;
  }

  receivedCount = 0;
}

// ---------------- SETUP ----------------
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

  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = PIN_MOSI;
  buscfg.miso_io_num = PIN_MISO;
  buscfg.sclk_io_num = PIN_SCLK;

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

  resetFrame();

  Serial.println("ESP READY");
}

// ---------------- LOOP ----------------
void loop() {

  memset(&t, 0, sizeof(t));

  t.length = PACKET_SIZE * 8;
  t.rx_buffer = rxBuf;

  digitalWrite(READY_PIN, HIGH);

  spi_slave_transmit(
    VSPI_HOST,
    &t,
    portMAX_DELAY
  );

  digitalWrite(READY_PIN, LOW);

  processPacket(rxBuf);

  // ONLY render when full frame received
  if (receivedCount >= HEIGHT) {

    renderFrame();
    resetFrame();
  }
}