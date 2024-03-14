import os
import subprocess
import sys

if (
    len(sys.argv) != 5
    or not sys.argv[1].endswith(".svg")
    or not os.path.isfile(sys.argv[1])
    or not os.path.isfile(sys.argv[2])
):
    print('Proper usage "python app.py AA.svg /path/to/svg2png 400 100"')
    print('Proper usage "python app.py SVG_FILE SVG_PNG_PATH SIZE_IMAGE TRYING"')
    raise ValueError("Missing or invalid input file or missing path to svg2png (panicked)")

try_number = int(sys.argv[4])
image_size = sys.argv[3]
svg2png_path = sys.argv[2]
image_input = sys.argv[1]
image_output = image_input.replace(".svg", ".png")

args = [svg2png_path, image_input, "-r", f"{image_size}x{image_size}"]
sizes: dict = {}
for i in range(try_number + 1):
    # if i % 100 == 0:
    #    print(f"{i + 1}/{try_number + 1}")
    subprocess.call(args, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    new_size = os.path.getsize(image_output)
    if new_size in sizes:
        sizes[new_size] += 1
    else:
        sizes[new_size] = 1

sizes = dict(sorted(sizes.items(), key=lambda item: item[1], reverse=True))

if len(sizes) == 1:
    print(f"Image size {str(sizes)}")
else:
    print(
        f"POSSIBLE_PROBLEM - Converting svg to png is not reproducible - file sizes {str(sizes)}"
    )
