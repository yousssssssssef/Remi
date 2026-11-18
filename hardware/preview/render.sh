#!/bin/zsh
# Renders the host preview: compiles the real UI + synth for the Mac, runs a scripted knob session,
# then muxes frames and audio into remi-device-preview.mp4 and saves PNG stills.
# Needs: a C++17 compiler, and PY = a Python with Pillow + imageio-ffmpeg.
set -euo pipefail
cd "${0:A:h}/.."
PY=${PY:-python3}
OUT=$(mktemp -d)
c++ -std=c++17 -O2 -Ifirmware/Remi_Device preview/preview.cpp firmware/Remi_Device/{gfx,ui,synth}.cpp -o "$OUT/preview"
"$OUT/preview" "$OUT"
FF=$("$PY" -c 'import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())')
"$FF" -loglevel error -y -f rawvideo -pix_fmt rgb24 -s 128x160 -r 30 -i "$OUT/frames.rgb" -i "$OUT/audio.wav" \
  -vf scale=512:640:flags=neighbor -c:v libx264 -pix_fmt yuv420p -crf 14 -c:a aac -b:a 160k -shortest \
  preview/remi-device-preview.mp4
mkdir -p preview/stills
"$PY" - "$OUT" <<'E'
import glob, os, sys
from PIL import Image
out = sys.argv[1]
files = sorted(glob.glob(os.path.join(out, '*.ppm')))
for f in files:
    Image.open(f).resize((384, 480), Image.NEAREST).save(os.path.join('preview/stills', os.path.basename(f)[:-4] + '.png'))
cols = 7
sheet = Image.new('RGB', (cols * 396 - 12, ((len(files) + cols - 1) // cols) * 492 - 12), '#2b2b2b')
for i, f in enumerate(files):
    sheet.paste(Image.open(f).resize((384, 480), Image.NEAREST), ((i % cols) * 396, (i // cols) * 492))
sheet.save('preview/screens.png')
E
rm -rf "$OUT"
echo "wrote preview/remi-device-preview.mp4, preview/screens.png, preview/stills/"
