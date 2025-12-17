import random
import array

width = 16
height = 16

header = f"P6\n{width} {height}\n255\n"
pixels = []

for _ in range(height):
    for _ in range(width):
        # Greyscale noise between 180 and 250
        val = random.randint(180, 250)
        pixels.extend([val, val, val])

with open("assets/textures/block.ppm", "wb") as f:
    f.write(header.encode('ascii'))
    f.write(array.array('B', pixels).tobytes())
