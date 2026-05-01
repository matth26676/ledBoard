const spi = require("spi-device");
const device = spi.openSync(0, 0);

const WIDTH = 128;
const HEIGHT = 64;

function sendFrame(t) {

  const buf = [];

  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 0; x < WIDTH; x++) {

      const r = (x + t) % 256;
      const g = (y + t) % 256;
      const b = (x + y + t) % 256;

      buf.push(
        0xAA,   // command: set pixel
        x,
        y,
        r,
        g,
        b
      );
    }
  }

  device.transferSync([{
    sendBuffer: Buffer.from(buf),
    byteLength: buf.length,
    speedHz: 500000
  }]);

  console.log("pixel stream sent");
}

let t = 0;
setInterval(() => sendFrame(t++), 200);