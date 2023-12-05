import os
import sys
from PIL import Image
argv = sys.argv[1:]
for file in argv:
    try:
        name = os.path.basename(file).split(".")[0]
        output = 'const unsigned char ' + name + '_rgba[] = {\n'
        img = Image.open(file)
        rgba_arr = list(img.getdata())
        for r, g, b, a, in rgba_arr:
            output += f"{r},{g},{b},{a},"
        output += '\n};\n'
        with open(name + '_rgba.c', 'w') as f:
            f.write('#include "' + name + '_rgba.h"\n')
            f.write(output)
        with open(name + '_rgba.h', 'w') as f:
            f.write('#ifndef ' + name.upper() + '_H\n#define ' + name.upper() + '_H\nconst extern unsigned char ' + name + '_rgba[];\n#endif')
    except KeyboardInterrupt:
        print("Ctrl+C")
        exit(0)
    except Exception as e:
        print("Error processing " + file)
        print(e)