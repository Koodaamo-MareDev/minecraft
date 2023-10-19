import os
import sys
from PIL import Image
argv = sys.argv[1:]
for file in argv:
    try:
        name = os.path.basename(file).split(".")[0]
        output = 'const unsigned char ' + name + '_alpha[] = {\n'
        img = Image.open(file)
        alphaimg = img.getchannel("A")
        alpha = alphaimg.load()
        w, h = alphaimg.size
        tw, th = int(w / 16), int(h / 16)
        print(str(tw) + "," + str(th))
        for yo in (range(th)):
            output += '    \n'
            for xo in range(tw):
                hasAlpha = False
                for y in range(16):
                    for x in range(16):
                        if (alpha[(x + (xo * 16), y + (yo * 16))] & 255) < 255:
                            hasAlpha = True
                            break
                    if hasAlpha: break
                output += str(0 if hasAlpha else 1) + ','
            output += '\n'
        output += '};\n'
        with open(name + '_alpha.c', 'w') as f:
            f.write('#include "' + name + '_alpha.h"\n')
            f.write(output)
        with open(name + '_alpha.h', 'w') as f:
            f.write('#ifndef ' + name.upper() + '_H\n#define ' + name.upper() + '_H\nconst extern unsigned char ' + name + '_alpha[];\n#endif')
    except KeyboardInterrupt:
        print("Ctrl+C")
        exit(0)
    except Exception as e:
        print("Error processing " + file)
        print(e)