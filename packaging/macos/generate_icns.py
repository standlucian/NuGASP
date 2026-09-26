#!/usr/bin/env bash
#!/usr/bin/env python3
"""
Generates high-resolution Apple ICNS file for NuTrackN from master icon.
Supports execution on Linux (via Pillow) and macOS (via iconutil or Pillow).
"""

import os
import sys
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "../.."))
SRC_ICON = os.path.join(REPO_ROOT, "NuTrackN/NuTrackN.png")
if not os.path.exists(SRC_ICON):
    SRC_ICON = os.path.join(REPO_ROOT, "NuTrackN/nutrackn.png")

OUTPUT_ICNS = os.path.join(SCRIPT_DIR, "NuTrackN.icns")

def main():
    print(f"==> Generating Apple ICNS icon from: {SRC_ICON}")
    im = Image.open(SRC_ICON).convert("RGBA")
    
    # Trim transparency and center in square
    bbox = im.getbbox()
    if bbox:
        cropped = im.crop(bbox)
        w, h = cropped.size
        max_dim = max(w, h)
        padding = int(max_dim * 0.04)
        canvas_dim = max_dim + 2 * padding
        square = Image.new("RGBA", (canvas_dim, canvas_dim), (0, 0, 0, 0))
        square.paste(cropped, ((canvas_dim - w) // 2, (canvas_dim - h) // 2), cropped)
    else:
        square = im

    # Standard macOS icon sizes
    sizes = [
        (16, 16),
        (32, 32),
        (64, 64),
        (128, 128),
        (256, 256),
        (512, 512),
        (1024, 1024)
    ]
    
    square.save(OUTPUT_ICNS, format="ICNS", sizes=sizes)
    print(f"    ✓ Successfully generated Apple ICNS: {OUTPUT_ICNS}")

if __name__ == "__main__":
    main()
