/*
    LEDBoard Raspberry Pi SPI Sender

    This software fetches display data from a remote API
    and sends it to the ESP32 over SPI.

    IMPORTANT:
    This code is timing-sensitive.

    Changes to:
    - SPI speed
    - READY synchronization
    - packet timing
    may cause instability.

    The Raspberry Pi only sends a row when the ESP32
    signals READY.
*/

const spi = require("spi-device");
const axios = require("axios");
const { execSync } = require("child_process");

// =====================================================
// SPI DEVICE
// =====================================================

// SPI bus 0
// Chip select 0
const device = spi.openSync(0, 0);

// =====================================================
// DISPLAY CONFIGURATION
// =====================================================

const WIDTH = 128;
const HEIGHT = 64;

// SPI packet size
const PACKET_SIZE = 512;

// =====================================================
// CONVERT HEX COLOR TO RGB
// =====================================================

function hexToRGB(hex) {

  /*
      API colors arrive as strings:

      "#RRGGBB"

      Example:
      "#FF0000"

      This converts them into:
      - red
      - green
      - blue
      byte values.
  */

  if (!hex || hex[0] !== "#") {
    return {
      r: 0,
      g: 0,
      b: 0
    };
  }

  const n = parseInt(hex.slice(1), 16);

  return {
    r: (n >> 16) & 255,
    g: (n >> 8) & 255,
    b: n & 255
  };
}

// =====================================================
// WAIT FOR ESP32 READY SIGNAL
// =====================================================

function waitReady() {

  /*
      The ESP32 raises READY HIGH when it is safe
      for the Pi to send another SPI packet.

      We use execSync() with the kernel GPIO utility
      because standard GPIO libraries were unreliable
      in this environment.
  */

  while (true) {

    const val = execSync(
      "gpioget --chip gpiochip0 17"
    )
    .toString()
    .trim();

    /*
        gpioget returns:

        17=active
        or
        17=inactive
    */

    if (val.includes("active")) {
      return;
    }
  }
}

// =====================================================
// SPI TRANSFER
// =====================================================

function transfer(buf) {

  /*
      Sends one complete SPI packet to the ESP32.

      Packet size is fixed at 512 bytes.

      SPI speed may be increased carefully,
      but excessively high values can cause:
      - corruption
      - dropped packets
      - instability
  */

  return new Promise((resolve, reject) => {

    const rx = Buffer.alloc(PACKET_SIZE);

    device.transfer([{
      sendBuffer: buf,
      receiveBuffer: rx,
      byteLength: PACKET_SIZE,

      // Current tested stable speed
      speedHz: 2000000,

      mode: 0
    }], (err) => {

      if (err) {
        reject(err);
        return;
      }

      resolve();
    });
  });
}

// =====================================================
// BUILD ROW PACKET
// =====================================================

function buildRowBuffer(rowData, y) {

  /*
      Packet Structure:

      Byte 0-1:
      Packet header

      Byte 2:
      Row number

      Byte 3:
      Width

      Byte 4+:
      RGB pixel data
  */

  const buf = Buffer.alloc(PACKET_SIZE);

  buf.fill(0);

  // Packet header
  buf[0] = 0xA5;
  buf[1] = 0x5A;

  // Row number
  buf[2] = y;

  // Width
  buf[3] = WIDTH;

  let i = 4;

  // Copy RGB pixel data
  for (let x = 0; x < WIDTH; x++) {

    const c = hexToRGB(rowData[x]);

    buf[i++] = c.r;
    buf[i++] = c.g;
    buf[i++] = c.b;
  }

  return buf;
}

// =====================================================
// FETCH FRAME FROM API
// =====================================================

async function fetchFrame() {

  /*
      Fetch full display contents from API.

      API returns:
      - width
      - height
      - pixel array
  */

  const res = await axios.get(
    "https://formplace.yorktechapps.com/api/canvas/full"
  );

  return res.data.data;
}

// =====================================================
// MAIN LOOP
// =====================================================

async function loop() {

  try {

    // Fetch full display frame
    const frame = await fetchFrame();

    // Send rows one at a time
    for (let y = 0; y < HEIGHT; y++) {

      const row = frame[y];

      // Validate row
      if (!row || row.length !== WIDTH) {

        console.log("INVALID ROW:", y);
        continue;
      }

      // Wait until ESP32 is ready
      waitReady();

      // Build packet
      const buf = buildRowBuffer(row, y);

      // Send packet
      await transfer(buf);

      console.log("ROW SENT:", y);
    }

    console.log("FRAME DONE\n");
  }
  catch (err) {

    console.error("MAIN LOOP ERROR:");
    console.error(err);
  }

  /*
      Delay before next frame fetch.

      Lower values:
      - faster refresh
      - more CPU/network usage

      Higher values:
      - lower system load
      - slower updates
  */

  setTimeout(loop, 10);
}

// =====================================================
// START
// =====================================================

loop();