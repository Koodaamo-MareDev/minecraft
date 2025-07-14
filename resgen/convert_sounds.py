import os
import subprocess

def convert_ogg_to_aiff(directory):
    """
    Recursively converts all .ogg files in a directory to .aiff using ffmpeg,
    then deletes the original .ogg files.
    """
    for root, _, files in os.walk(directory):
        for file in files:
            if file.lower().endswith('.ogg'):
                ogg_path = os.path.join(root, file)
                aiff_path = os.path.splitext(ogg_path)[0] + '.aiff'
                
                # Build the ffmpeg command
                command = [
                    'ffmpeg',
                    '-hide_banner',
                    '-loglevel', 'error',
                    '-i', ogg_path,
                    '-f', 'aiff',
                    '-bitexact',
                    '-acodec', 'pcm_s16be',
                    '-ar', '16000',
                    '-ac', '1',
                    aiff_path
                ]

                try:
                    # Run the command
                    subprocess.run(command, check=True)
                    print(f"Converted: {ogg_path} -> {aiff_path}")
                    
                    # Remove the original .ogg file
                    os.remove(ogg_path)
                    print(f"Deleted: {ogg_path}")
                except subprocess.CalledProcessError as e:
                    print(f"Failed to convert {ogg_path}: {e}")
                except OSError as e:
                    print(f"Failed to delete {ogg_path}: {e}")