const axios = require("axios");
const fs = require("fs");
const { createCanvas } = require("canvas");

const API = "https://formplace.yorktechapps.com/api/canvas/full";

function hexToRGB(hex) {
  return {
    r: parseInt(hex.substr(1, 2), 16),
    g: parseInt(hex.substr(3, 2), 16),
    b: parseInt(hex.substr(5, 2), 16)
  };
}

async function renderPNG() {

  console.log("[1] Fetching canvas...");

  const res = await axios.get(API);
  const { data } = res.data;

  const width = data[0].length;
  const height = data.length;

  console.log(`[2] Rendering ${width}x${height}`);

  const canvas = createCanvas(width, height);
  const ctx = canvas.getContext("2d");

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {

      const c = hexToRGB(data[y][x]);

      ctx.fillStyle = `rgb(${c.r},${c.g},${c.b})`;
      ctx.fillRect(x, y, 1, 1);
    }
  }

  const buffer = canvas.toBuffer("image/png");

  fs.writeFileSync("canvas.png", buffer);

  console.log("[3] Saved canvas.png");
}

renderPNG().catch(err => {
  console.error("Error:", err.message);
});