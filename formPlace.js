const spi = require("spi-device");
const axios = require("axios");
const { execSync } = require("child_process");

const device = spi.openSync(0, 0);

const WIDTH = 128;
const HEIGHT = 64;
const PACKET_SIZE = 512;

// -----------------------------
function readReady() {
  try {
    const v = execSync(
      "gpioget --chip gpiochip0 17"
    )
    .toString()
    .trim();

    // your system outputs "17=active"
    return v.includes("active");

  } catch (e) {
    return false;
  }
}

// -----------------------------
function waitReady() {
  while (!readReady()) {
    // small delay to prevent CPU lockup
    const start = Date.now();
    while (Date.now() - start < 1) {}
  }
}

// -----------------------------
function hexToRGB(hex) {
  if (!hex || hex[0] !== "#") return { r: 0, g: 0, b: 0 };

  const n = parseInt(hex.slice(1), 16);
  return {
    r: (n >> 16) & 255,
    g: (n >> 8) & 255,
    b: n & 255
  };
}

// -----------------------------
function transfer(buf) {

  return new Promise((resolve) => {

    const rx = Buffer.alloc(PACKET_SIZE);

    device.transfer([{
      sendBuffer: buf,
      receiveBuffer: rx,
      byteLength: PACKET_SIZE,
      speedHz: 2000000, // your known stable value
      mode: 0
    }], () => resolve());
  });
}

// -----------------------------
function buildRowBuffer(rowData, y) {

  const buf = Buffer.alloc(PACKET_SIZE);
  buf.fill(0);

  buf[0] = 0xA5;
  buf[1] = 0x5A;
  buf[2] = y;
  buf[3] = WIDTH;

  let i = 4;

  for (let x = 0; x < WIDTH; x++) {

    const c = hexToRGB(rowData[x]);

    buf[i++] = c.r;
    buf[i++] = c.g;
    buf[i++] = c.b;
  }

  return buf;
}

// -----------------------------
async function fetchFrame() {

  const res = await axios.get(
    "https://formplace.yorktechapps.com/api/canvas/full"
  );

  return res.data.data;
}

// -----------------------------
async function loop() {

  const frame = await fetchFrame();

  for (let y = 0; y < HEIGHT; y++) {

    const row = frame[y];
    if (!row || row.length !== WIDTH) continue;

    waitReady();

    const buf = buildRowBuffer(row, y);
    await transfer(buf);

    console.log("ROW SENT:", y);
  }

  console.log("FRAME DONE");

  setTimeout(loop, 10);
}

loop();