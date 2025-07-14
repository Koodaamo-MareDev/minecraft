import os
import sys
import shutil
from PIL import Image

from resgen.extract_resources import extract_resources, get_default_minecraft_dir
from resgen.apply_mask import apply_mask
from resgen.convert_sounds import convert_ogg_to_aiff

def get_script_path():
    return os.path.dirname(os.path.realpath(sys.argv[0]))

def fix_rgba_image(img_file):
    Image.open(img_file).convert("RGBA").save(img_file)

if __name__ == "__main__":
    os.chdir(get_script_path())
    extract_resources(get_default_minecraft_dir(), "resourcelist.txt", "resources")
    shutil.copytree("resgen/edited", "resources", dirs_exist_ok=True)
    apply_mask("resources/textures/terrain.png", "resources/textures/terrain_masked.png", "resources/textures/terrain_mask.png", "resources/textures/terrain.png")
    apply_mask("resources/textures/gui/icons.png", "resources/textures/gui/icons_masked.png", "resources/textures/gui/icons_mask.png", "resources/textures/gui/icons.png")
    fix_rgba_image("resources/textures/particles.png")
    convert_ogg_to_aiff("resources/newsound")