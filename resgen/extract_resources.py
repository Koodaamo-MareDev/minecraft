import sys
import platform
import os
import json
import shutil
import zipfile
import argparse
from pathlib import Path

def load_index(index_path):
    if index_path.is_file():
        with open(index_path, "r", encoding="utf-8") as f:
            return json.load(f).get("objects", {})
    return {}

def find_version_jar(mc_dir):
    primary = Path(mc_dir) / "versions" / "b1.3_01" / "b1.3_01.jar"
    fallback = Path(mc_dir) / "versions" / "b1.3b" / "b1.3b.jar"

    if primary.is_file():
        return primary
    elif fallback.is_file():
        return fallback
    else:
        return None

def extract_from_objects(mc_dir, rel_path, hash_val):
    subdir = hash_val[:2]
    source = Path(mc_dir) / "assets" / "objects" / subdir / hash_val
    return source if source.is_file() else None

def extract_from_jar(jar_path, rel_path, output_path):
    with zipfile.ZipFile(jar_path, "r") as jar:
        try:
            with jar.open(rel_path) as source_file:
                output_path.parent.mkdir(parents=True, exist_ok=True)
                with open(output_path, "wb") as out_file:
                    shutil.copyfileobj(source_file, out_file)
                print(f"Extracted from jar: {rel_path}")
                return True
        except KeyError:
            return False

def extract_resources(mc_dir, paths_file, output_dir):
    index_file = Path(mc_dir) / "assets" / "indexes" / "pre-1.6.json"
    index_objects = load_index(index_file)

    # Read file list
    with open(paths_file, "r", encoding="utf-8") as f:
        target_paths = [line.strip() for line in f if line.strip()]

    # Locate jar
    jar_path = find_version_jar(mc_dir)
    if jar_path:
        print(f"Using fallback JAR: {jar_path}")
    else:
        print("No fallback JAR found. Only using asset index.")

    for rel_path in target_paths:
        parts = rel_path.split(">", 1)
        rel_path = parts[0]
        if len(parts) < 2:
            dest_path = Path(output_dir) / rel_path
        else:
            dest_path = Path(output_dir) / parts[1]
        if rel_path in index_objects:
            hash_val = index_objects[rel_path]["hash"]
            source_file = extract_from_objects(mc_dir, rel_path, hash_val)
            if source_file:
                dest_path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_file, dest_path)
                print(f"Extracted from index: {rel_path}")
                continue
            else:
                print(f"Hash found but file missing for: {rel_path}")

        # Fallback to jar
        if jar_path:
            success = extract_from_jar(jar_path, rel_path, dest_path)
            if not success:
                print(f"File not found in JAR: {rel_path}")
        else:
            print(f"File not found and no jar to fallback: {rel_path}")

def get_default_minecraft_dir():
    system = platform.system()

    if system == "Windows":
        appdata = os.getenv("APPDATA")
        if appdata:
            return Path(appdata) / ".minecraft"
        else:
            raise EnvironmentError("APPDATA environment variable not found on Windows.")
    elif system == "Darwin":  # macOS
        return Path.home() / "Library/Application Support/minecraft"
    elif system == "Linux":
        return Path.home() / ".minecraft"
    else:
        raise NotImplementedError(f"Unsupported OS: {system}")
        
if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Extract Minecraft Beta 1.3 resources.")
    parser.add_argument("-y", help="Disable confirmation prompt for settings (uses the default settings for unspecified arguments)", action="store_true")
    parser.add_argument("--minecraft_dir", help="Path to Minecraft directory (e.g., ~/.minecraft or %APPDATA%\\.minecraft)", default=get_default_minecraft_dir())
    parser.add_argument("--items_file", help="Text file containing items to extract", default="list.txt")
    parser.add_argument("--output_dir", help="Output directory", default="resources")
    
    args = parser.parse_args()
    
    if not args.y:
        print("Using following settings:")
        print("minecraft_dir =", args.minecraft_dir)
        print("items_file =", args.items_file)
        print("output_dir =", args.output_dir)
        if input("Are these settings correct? Y/N: ")[0].lower() != "y":
            print("Abort")
            exit(1)
    extract_resources(args.minecraft_dir, args.items_file, args.output_dir)

