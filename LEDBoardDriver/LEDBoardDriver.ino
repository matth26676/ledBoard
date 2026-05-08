#include <FastLED.h>
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"

#define WIDTH 128
#define HEIGHT 64
#define PACKET_SIZE (6 + (WIDTH * 3))

#define DATA_PIN_1 15
#define DATA_PIN_2 2
#define DATA_PIN_3 22
#define DATA_PIN_4 21

#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_SCLK 18
#define PIN_CS   5

CRGB leds1[2048];
CRGB leds2[2048];
CRGB leds3[2048];
CRGB leds4[2048];

CRGB* ledMap[WIDTH * HEIGHT];

uint8_t* rxBuf;
uint8_t* safeBuf;
spi_slave_transaction_t t;

// ---------------- CRC16
uint16_t crc16(const uint8_t* data, int len) {

  uint16_t crc = 0xFFFF;

  for (int i = 0; i < len; i++) {

    crc ^= data[i];

    for (int j = 0; j < 8; j++) {
      crc = (crc & 1)
        ? (crc >> 1) ^ 0xA001
        : crc >> 1;
    }
  }

  return crc;
}

// ---------------- APPLY ROW
void applyRow(uint8_t* buf) {

  uint8_t row = buf[2];

  int i = 6;
  int base = row * WIDTH;

  for (int x = 0; x < WIDTH; x++) {

    ledMap[base + x]->setRGB(
      buf[i],
      buf[i + 1],
      buf[i + 2]
    );

    i += 3;
  }
}

// ---------------- SETUP
void setup() {

  Serial.begin(115200);
  Serial.println("BOOT");

  FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds1, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds2, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds3, 2048);
  FastLED.addLeds<WS2812B, DATA_PIN_4, GRB>(leds4, 2048);

  FastLED.setBrightness(10);

  rxBuf = (uint8_t*) heap_caps_malloc(PACKET_SIZE, MALLOC_CAP_DMA);
  safeBuf = (uint8_t*) heap_caps_malloc(PACKET_SIZE, MALLOC_CAP_8BIT);

  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = PIN_MOSI;
  buscfg.miso_io_num = PIN_MISO;
  buscfg.sclk_io_num = PIN_SCLK;

  spi_slave_interface_config_t slvcfg = {};
  slvcfg.spics_io_num = PIN_CS;
  slvcfg.queue_size = 1;
  slvcfg.mode = 0;

  spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);

  Serial.println("READY");
}

// ---------------- LOOP (FULLY SAFE PIPELINE)
void loop() {

  memset(&t, 0, sizeof(t));

  t.length = PACKET_SIZE * 8;
  t.rx_buffer = rxBuf;

  esp_err_t err = spi_slave_transmit(
    VSPI_HOST,
    &t,
    portMAX_DELAY
  );

  if (err != ESP_OK)
    return;

  // ---------------- HARD HEADER CHECK
  if (rxBuf[0] != 0xA5 || rxBuf[1] != 0x5A) {
    Serial.println("BAD HEADER DROP");
    return;
  }

  memcpy(safeBuf, rxBuf, PACKET_SIZE);

  uint8_t row = safeBuf[2];

  if (row >= HEIGHT)
    return;

  // ---------------- CRC CHECK (PIXEL ONLY)
  uint16_t rxCRC = (safeBuf[4] << 8) | safeBuf[5];
  uint16_t calcCRC = crc16(&safeBuf[6], WIDTH * 3);

  if (rxCRC != calcCRC) {
    Serial.printf("CRC FAIL row=%d DROP\n", row);
    return;
  }

  applyRow(safeBuf);

  FastLED.show();
}