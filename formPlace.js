const spi = require("spi-device");
const axios = require("axios");

const device = spi.openSync(0, 0);

const WIDTH = 128;
const HEIGHT = 64;
const PACKET_SIZE = 6 + (WIDTH * 3);

const FRAME_DELAY = 2;   // prevents burst overruns
const FETCH_DELAY = 500;

let frame = null;

// ---------------- CRC16 (MUST MATCH ESP)
function crc16(buf) {

  let crc = 0xFFFF;

  for (let i = 0; i < buf.length; i++) {
    crc ^= buf[i];

    for (let j = 0; j < 8; j++) {
      crc = (crc & 1)
        ? (crc >> 1) ^ 0xA001
        : crc >> 1;
    }
  }

  return crc & 0xFFFF;
}

// ---------------- FETCH FRAME
async function fetchLoop() {

  try {
    const res = await axios.get(
      "https://formplace.yorktechapps.com/api/canvas/full"
    );

    frame = res.data.data;

  } catch (e) {
    console.log("fetch error");
  }

  setTimeout(fetchLoop, FETCH_DELAY);
}

// ---------------- SEND ROW
function sendRow(row, y) {

  return new Promise((resolve, reject) => {

    const buf = Buffer.alloc(PACKET_SIZE);

    buf[0] = 0xA5;
    buf[1] = 0x5A;
    buf[2] = y;
    buf[3] = WIDTH;

    let i = 6;

    for (let x = 0; x < WIDTH; x++) {

      const n = parseInt(row[x].slice(1), 16);

      buf[i++] = (n >> 16) & 255;
      buf[i++] = (n >> 8) & 255;
      buf[i++] = n & 255;
    }

    const crc = crc16(buf.slice(6));

    buf[4] = (crc >> 8) & 255;
    buf[5] = crc & 255;

    device.transfer([{
      sendBuffer: buf,
      receiveBuffer: Buffer.alloc(buf.length),
      byteLength: buf.length,
      speedHz: 100000,
      mode: 0
    }], (err) => {

      if (err) return reject(err);
      resolve();
    });
  });
}

// ---------------- MAIN LOOP
async function loop() {

  if (!frame) {
    setTimeout(loop, 10);
    return;
  }

  for (let y = 0; y < HEIGHT; y++) {

    const row = frame[y];
    if (!row) continue;

    await sendRow(row, y);

    // critical pacing to prevent SPI burst overlap
    await new Promise(r => setTimeout(r, FRAME_DELAY));
  }

  setTimeout(loop, 5);
}

fetchLoop();
loop();