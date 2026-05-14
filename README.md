# LEDBoard SPI Display System

## Overview

This project is a custom LED display system built using:

* Raspberry Pi 4 Model B
* ESP32
* Four WS2812B LED panel sections

The Raspberry Pi acts as the main controller and network device.
It fetches display data from a remote API and sends the data to the ESP32 over SPI.

The ESP32 acts as a dedicated LED renderer.
It receives SPI packets containing display rows, stores them in memory, and renders the completed frame to the LED panels.

---

# System Architecture

## Data Flow

```text
Remote API
    ↓
Raspberry Pi
    ↓ (SPI)
ESP32
    ↓
WS2812B LED Panels
```

---

# Why SPI Was Chosen

SPI was selected because:

* It is significantly faster than serial UART
* It provides deterministic timing
* It works well for transferring fixed-size display packets
* The ESP32 includes hardware SPI slave support

---

# Why the READY Line Exists

The ESP32 cannot safely receive continuous SPI packets while rendering LED data.

To prevent timing issues:

1. The ESP32 raises the READY GPIO pin
2. The Raspberry Pi waits until READY is active
3. The Raspberry Pi sends exactly one SPI packet
4. The ESP32 processes the packet
5. The cycle repeats

This prevents:

* dropped rows
* packet overlap
* frame corruption
* timing drift

---

# Frame Rendering Strategy

The ESP32 does NOT render LEDs immediately after receiving a row.

Instead:

1. All 64 rows are received
2. Rows are stored in a frame buffer
3. The full display updates simultaneously

This prevents:

* visible row scanning
* flickering
* partial frame updates

---

# Display Layout

The display is composed of:

* 4 LED output channels
* 16x16 serpentine sections
* Total resolution: 128x64

The ESP32 maps logical X/Y coordinates into the physical LED arrangement.

---

# Hardware Connections

## Raspberry Pi GPIO

| Function | GPIO   | Physical Pin |
| -------- | ------ | ------------ |
| SPI_MOSI | GPIO10 | Pin 19       |
| SPI_MISO | GPIO9  | Pin 21       |
| SPI_SCLK | GPIO11 | Pin 23       |
| SPI_CS   | GPIO8  | Pin 24       |
| READY    | GPIO17 | Pin 11       |

---

## ESP32 GPIO

| Function   | GPIO   |
| ---------- | ------ |
| SPI_MOSI   | GPIO23 |
| SPI_MISO   | GPIO19 |
| SPI_SCLK   | GPIO18 |
| SPI_CS     | GPIO5  |
| READY      | GPIO4  |
| LED_DATA_1 | GPIO15 |
| LED_DATA_2 | GPIO2  |
| LED_DATA_3 | GPIO22 |
| LED_DATA_4 | GPIO21 |

---

# SPI Packet Format

Each SPI packet is 512 bytes.

## Packet Structure

| Byte Range | Purpose                   |
| ---------- | ------------------------- |
| 0-1        | Packet header (0xA5 0x5A) |
| 2          | Row number                |
| 3          | Width                     |
| 4+         | RGB pixel data            |

Each row contains:

* 128 pixels
* RGB format
* 384 bytes of color data

---

# API Source

The Raspberry Pi fetches display data from:

```text
https://formplace.yorktechapps.com/api/canvas/full
```

The API returns:

```json
{
  "width": 128,
  "height": 64,
  "data": [...]
}
```

---

# Running the Raspberry Pi Software

## Install dependencies

```bash
npm install spi-device axios
```

## Run

```bash
node index.js
```

---

# Flashing the ESP32

## Required Libraries

* FastLED

## Board Package

ESP32 Arduino Core

## Upload

Use Arduino IDE or PlatformIO.

---

# Known Constraints

## READY Timing

The READY signal is critical for synchronization.

Removing or bypassing READY may cause:

* skipped rows
* packet corruption
* display instability

---

## FastLED Timing

WS2812B rendering temporarily blocks timing-sensitive operations.

This is why:

* frame buffering exists
* rendering only occurs after full frame reception

---

# Future Improvements

Possible future upgrades:

* Compatibility with formBuzzer
* interrupt-driven READY detection
* higher SPI transfer speeds

---

# Troubleshooting

## Every Other Row Missing

Usually indicates:

* synchronization timing issues
* READY line problems
* packet overlap

---

## Random Pixel Corruption

Usually indicates:

* SPI clock too high
* poor grounding
* power instability

---

## ESP32 Reboots

Usually indicates:

* memory corruption
* invalid packet handling
* insufficient power

---

# Repository Structure

```text
/LEDBoardDriver
    ESP32 firmware

/formPlace.js
    Raspberry Pi transfer software

/LEDBoardSchematic
    Hardware schematics
```
