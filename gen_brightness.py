from PIL import Image
try:
    output = 'unsigned char brightness_values[] = {\n'
    for b in range(256):
        output += f"{b},{b},{b},255,"
    output += '\n};\n'
    with open('brightness_values.c', 'w') as f:
        f.write('#include "brightness_values.h"\n')
        f.write(output)
    with open('brightness_values.h', 'w') as f:
        f.write('#ifndef BRIGHTNESS_VALUES_H\n#define BRIGHTNESS_VALUES_H\nextern unsigned char brightness_values[];\n#endif')
except KeyboardInterrupt:
    print("Ctrl+C")
    exit(0)
except Exception as e:
    print("Error processing brightness")
    print(e)