# apply_mask_pil.py

from PIL import Image
import numpy as np
import sys

def apply_mask(orig_image_path, edit_image_path, mask_path, output_path):
    orig = Image.open(orig_image_path).convert("RGBA")
    edit = Image.open(edit_image_path).convert("RGBA")
    mask = Image.open(mask_path).convert("L")

    if orig.size != edit.size or orig.size != mask.size:
        raise ValueError("All inputs must be the same size")

    # Convert to NumPy arrays
    orig_np = np.array(orig)
    edit_np = np.array(edit)
    mask_np = np.array(mask) // 255  # Convert to 0 or 1

    # Expand mask to match RGB shape
    mask_4ch = np.stack([mask_np]*4, axis=-1)

    # Blend: where mask == 1, use edited; else use the original
    blended_np = orig_np * (1 - mask_4ch) + edit_np * mask_4ch
    blended_img = Image.fromarray(np.uint8(blended_np))

    blended_img.save(output_path)
    print(f"Saved blended image to: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python apply_mask.py <original_image.png> <edited_image.png> <mask.png>")
    else:
        apply_mask(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[1])
