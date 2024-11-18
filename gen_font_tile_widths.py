import os
import sys
from PIL import Image
import numpy as np

def calculate_tile_widths(image_path, output_file):
    # Load the image
    img = Image.open(image_path).convert("RGBA")
    img_width, img_height = img.size
    
    if img_width != 128 or img_height != 128:
        raise ValueError("Image must be 128x128 pixels.")
    
    # Convert image to numpy array
    data = np.array(img)
    
    # Initialize an array to store tile widths
    tile_widths = []

    # Loop through 16x16 grid of 8x8 tiles
    for tile_y in range(16):
        for tile_x in range(16):
            # Extract the 8x8 tile
            x_start = tile_x * 8
            y_start = tile_y * 8
            tile = data[y_start:y_start + 8, x_start:x_start + 8, 3]  # Extract alpha channel
            
            # Calculate the bounding width of the tile
            tile_width = 0
            for x in range(8):
                if np.any(tile[:, x] > 0):  # If any pixel in the column is not transparent
                    tile_width = max(tile_width, x + 1)
            
            # Add 1 to the width to account for the space between characters
            tile_width += 1

            # Limit the width to 8 pixels
            tile_width = min(tile_width, 8)
            
            tile_widths.append(tile_width)
    
    # Write to .hpp file
    with open(output_file, "w") as file:
        file.write("#ifndef FONT_TILE_WIDTHS_HPP\n\n")
        file.write("#define FONT_TILE_WIDTHS_HPP\n\n")
        file.write("#include <cstdint>\n\n")
        file.write("static const uint8_t font_tile_widths[256] = {\n")
        for i, width in enumerate(tile_widths):
            file.write(f" {width},")
            if (i + 1) % 16 == 0:
                file.write("\n")
        file.write("};\n")
        file.write("\n#endif\n")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python calc_fontwidth.py <image>")
        exit(1)
    calculate_tile_widths(sys.argv[1], "font_tile_widths.hpp")