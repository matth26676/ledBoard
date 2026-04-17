import board
import neopixel
from PIL import Image
import time

# ======================================================
# === CONFIGURATION ===
# ======================================================

# Panel configuration
PANEL_WIDTH = 16
PANEL_HEIGHT = 16
PANELS_X = 8
PANELS_Y = 4

# Behavior toggles
SERPENTINE_PANELS = True
SERPENTINE_PIXELS = True

# NeoPixel config
DATA_PIN = board.D21
BRIGHTNESS = 0.05

# Image behavior
SCALE_TO_FIT = True
IMAGE_SOURCE = None  # e.g. "image.png"

# ======================================================
# === AUTO CALCULATED ===
# ======================================================

DISPLAY_WIDTH = PANEL_WIDTH * PANELS_X
DISPLAY_HEIGHT = PANEL_HEIGHT * PANELS_Y
NUM_PIXELS = DISPLAY_WIDTH * DISPLAY_HEIGHT

pixels = neopixel.NeoPixel(
    DATA_PIN,
    NUM_PIXELS,
    brightness=BRIGHTNESS,
    auto_write=False,
    pixel_order=neopixel.GRB
)

# ======================================================
# === PIXEL MAPPING (WITH 90° CCW ROTATION) ===
# ======================================================

def build_pixel_map():
    mapping = []

    for y in range(DISPLAY_HEIGHT):
        for x in range(DISPLAY_WIDTH):

            # --- Panel location ---
            panel_x = x // PANEL_WIDTH
            panel_y = y // PANEL_HEIGHT

            # --- Local position inside panel ---
            local_x = x % PANEL_WIDTH
            local_y = y % PANEL_HEIGHT

            # --- Serpentine across panels ---
            if SERPENTINE_PANELS and (panel_y % 2 == 1):
                panel_x = PANELS_X - 1 - panel_x

            # --- Rotate panel 90° CCW ---
            rot_x = local_y
            rot_y = PANEL_WIDTH - 1 - local_x

            # --- Serpentine inside panel ---
            if SERPENTINE_PIXELS and (rot_y % 2 == 1):
                rot_x = PANEL_WIDTH - 1 - rot_x

            # --- Convert to strip index ---
            panel_index = panel_y * PANELS_X + panel_x
            pixel_index = panel_index * (PANEL_WIDTH * PANEL_HEIGHT) + (rot_y * PANEL_WIDTH + rot_x)

            mapping.append(pixel_index)

    return mapping


# ======================================================
# === IMAGE LOADING ===
# ======================================================

def load_image_data(source, width, height, scale_to_fit=True, pos_x=0, pos_y=0):
    if isinstance(source, str):
        img = Image.open(source).convert("RGB")

        if scale_to_fit:
            img.thumbnail((width, height), Image.LANCZOS)

        canvas = Image.new("RGB", (width, height), (0, 0, 0))
        canvas.paste(img, (pos_x, pos_y))

        return list(canvas.getdata())

    elif isinstance(source, list):
        canvas = [(0, 0, 0)] * (width * height)

        for y in range(len(source)):
            for x in range(len(source[0])):
                dst_x = x + pos_x
                dst_y = y + pos_y

                if 0 <= dst_x < width and 0 <= dst_y < height:
                    canvas[dst_y * width + dst_x] = source[y][x]

        return canvas

    else:
        raise ValueError("Invalid image source")


# ======================================================
# === DISPLAY FUNCTION ===
# ======================================================

def show_image(image_source, mapping, pos_x=0, pos_y=0):
    image_data = load_image_data(
        image_source,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        SCALE_TO_FIT,
        pos_x,
        pos_y
    )

    for i in range(NUM_PIXELS):
        pixels[mapping[i]] = image_data[i]

    pixels.show()


# ======================================================
# === TEST MODES ===
# ======================================================

def test_full_red():
    pixels.fill((255, 0, 0))
    pixels.show()
    print("Full red test")


def test_pixel_walk(mapping):
    print("Walking pixels...")
    for i in range(NUM_PIXELS):
        pixels.fill((0, 0, 0))
        pixels[mapping[i]] = (255, 0, 0)
        pixels.show()
        time.sleep(0.0005)


def debug_mapping(mapping):
    print("Total:", len(mapping))
    print("Unique:", len(set(mapping)))
    print("Max:", max(mapping))


# ======================================================
# === MAIN ===
# ======================================================

if __name__ == "__main__":

    print("Building pixel map...")
    mapping = build_pixel_map()

    debug_mapping(mapping)

    # ---- BASIC TEST ----
    test_full_red()
    time.sleep(2)

    # # ---- PIXEL WALK TEST ----
    # test_pixel_walk(mapping)

    # # ---- IMAGE DISPLAY ----
    # if IMAGE_SOURCE:
    #     show_image(IMAGE_SOURCE, mapping)