"""Builds every generated asset for the Remi device firmware.

Outputs (firmware/Remi_Device/):
  intro_frames.h  fly-in + zoom + face-dock frames (palette RLE), played after the welcome animation
  ui_assets.h     anti-aliased fonts, icons and the blinking face avatar (4-bit alpha masks)
Previews (preview/): intro contact sheet.

Run with a Python that has Pillow and numpy:
  python tools/build_assets.py
Sources: the reviewed Remi welcome vectors (mascot.svg, scene.svg) and the Remi app's DM Sans fonts.
Vectors are rasterized with headless Google Chrome.
"""
import math, os, re, struct, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / 'firmware' / 'Remi_Device'
PREVIEW = ROOT / 'preview'
VECTORS = ROOT / 'assets' / 'vectors'               # Remi welcome vectors (mascot.svg, scene.svg)
FONTS = ROOT.parent / 'mobile-app' / 'assets' / 'fonts'   # the app's DM Sans cuts
CHROME = os.environ.get('CHROME', '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')

W, H, SS, FPS = 128, 160, 4, 30           # display size, supersampling, frame rate
LEMON, COBALT = (248, 228, 92), (23, 52, 190)
PX_PER_UNIT = 4                            # mascot raster resolution; mascot viewBox starts at -12
FACE_CENTER = (340.6, 276.6)               # centre of Remi's screen interior, mascot units (measured by flood fill)
FACE_RECT = (268, 210, 414, 338)           # brows, eyes and smile only; excludes the screen border and glints
SCREEN_RECT = (241, 179, 440, 374)         # inside the screen border: face plus glints, no body
K0 = 0.475 * 128 / 390                     # px per unit in the welcome animation (scene scale x video crop scale)
C0 = ((52 + 0.475 * FACE_CENTER[0]) * 128 / 390, 2 + (9 + 0.475 * FACE_CENTER[1]) * 128 / 390)
K_END, C_END = 164 / 202.25, (64.0, 80.0)  # screen interior (202 units tall) slightly overfills the 160 px height
AVATAR_BOX = (5, 3, 31, 25)                # where the face docks on every page (x0, y0, x1, y1)


# ---------- rasterize vector layers ----------
def render_layers(tmp: Path):
    mascot = (VECTORS / 'mascot.svg').read_text()
    scene = (VECTORS / 'scene.svg').read_text()

    def shot(name, svg, hide, w, h, viewbox=None, bg='transparent', extra_css=''):
        if viewbox:
            svg = re.sub(r'viewBox="[^"]+"', f'viewBox="{viewbox}"', svg, count=1)
        css = (','.join('#' + i for i in hide) + '{display:none}') if hide else ''
        html = (f'<html><head><style>html,body{{margin:0;background:{bg}}}svg{{width:{w}px;height:{h}px;display:block}}'
                f'{css}{extra_css}</style></head><body>{svg}</body></html>')
        page = tmp / f'{name}.html'
        page.write_text(html)
        subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--default-background-color=00000000',
                        f'--window-size={w},{h}', f'--screenshot={tmp / name}.png', page.as_uri()], capture_output=True, check=True)
        return Image.open(tmp / f'{name}.png').convert('RGBA')

    size = 624 * PX_PER_UNIT
    layers = {
        'mascot': shot('mascot', mascot, [], size, size - 48),
        'nofill': shot('nofill', mascot, ['panelFill'], size, size - 48),
        'noeyes': shot('noeyes', mascot, ['panelFill', 'eyes'], size, size - 48),
        'eyes': shot('eyes', mascot, ['panelFill', 'chassis', 'repairedLowerLeftCorner', 'wristUnderlap', 'glove',
                                      'wisp0', 'wisp1', 'wisp2'], size, size - 48),
        # Welcome scene without Remi, cropped exactly like the welcome video (scene y 175..592 -> 137 px at y=2).
        'scene_bg': shot('scene_bg', scene, ['character', 'subtitle'], W * SS, 137 * SS, viewbox='0 175 390 417.333', bg='#F8E45C'),
    }
    return layers


def mips(img):
    levels = [img]
    while levels[-1].width > 300:
        levels.append(levels[-1].resize((levels[-1].width // 2, levels[-1].height // 2), Image.LANCZOS))
    return levels


def place(levels, k, c_disp, theta=0.0, anchor=FACE_CENTER):
    """Affine-render a mascot layer so `anchor` (units) lands at c_disp (display px), k px/unit, rotated theta."""
    out_scale = k * SS                                      # output px per unit
    lvl = 0
    while lvl + 1 < len(levels) and PX_PER_UNIT / (2 ** (lvl + 1)) >= out_scale * 1.2:
        lvl += 1
    src = levels[lvl]
    ppu = PX_PER_UNIT / (2 ** lvl)
    cos, sin = math.cos(theta), math.sin(theta)
    a, b = cos * ppu / out_scale, sin * ppu / out_scale
    d, e = -sin * ppu / out_scale, cos * ppu / out_scale
    cx, cy = c_disp[0] * SS, c_disp[1] * SS
    ax, ay = (anchor[0] + 12) * ppu, (anchor[1] + 12) * ppu
    data = (a, b, ax - a * cx - b * cy, d, e, ay - d * cx - e * cy)
    return src.transform((W * SS, H * SS), Image.AFFINE, data, resample=Image.BICUBIC)


def face_only(layer, rect=FACE_RECT):
    """Keep only one rectangle (mascot units) of a mascot layer."""
    x0, y0, x1, y1 = [(v + 12) * PX_PER_UNIT for v in rect]
    out = Image.new('RGBA', layer.size, (0, 0, 0, 0))
    out.paste(layer.crop((x0, y0, x1, y1)), (x0, y0))
    return out


def ease_in_out(t):
    return 4 * t ** 3 if t < .5 else 1 - (-2 * t + 2) ** 3 / 2


def bezier_ease(x1, y1, x2, y2):
    def f(t):
        lo, hi = 0.0, 1.0
        for _ in range(40):
            m = (lo + hi) / 2
            x = 3 * (1 - m) ** 2 * m * x1 + 3 * (1 - m) * m ** 2 * x2 + m ** 3
            lo, hi = (m, hi) if x < t else (lo, m)
        m = (lo + hi) / 2
        return 3 * (1 - m) ** 2 * m * y1 + 3 * (1 - m) * m ** 2 * y2 + m ** 3
    return f


def smoothstep(a, b, x):
    t = min(1, max(0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


# ---------- welcome animation's last frame (to dissolve from) ----------
def last_welcome_frame():
    src = (FW / 'remi_animation.h').read_text()
    offsets = [int(v) for v in re.search(r'FRAME_OFFSETS\[\] PROGMEM = \{(.*?)\};', src, re.S).group(1).replace('\n', '').split(',') if v.strip()]
    data = bytes(int(v, 16) for v in re.findall(r'0x([0-9A-F]{2})', re.search(r'FRAME_DATA\[\] PROGMEM = \{(.*?)\};', src, re.S).group(1)))
    px = [0] * (W * H)
    for f in range(len(offsets) - 1):
        o, end, cur = offsets[f], offsets[f + 1], 0
        while o < end:
            skip, n = struct.unpack_from('<HH', data, o); o += 4; cur += skip
            px[cur:cur + n] = struct.unpack_from('<' + 'H' * n, data, o); o += 2 * n; cur += n
    rgb = [(((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31) for v in px]
    img = Image.new('RGB', (W, H)); img.putdata(rgb)
    return img


# ---------- intro frames ----------
FLY_START, FLY_END = 0.20, 1.40            # seconds within the intro
DOCK_START, DOCK_END = 1.55, 2.12
INTRO_LEN = DOCK_END


def fly_state(t):
    fly = bezier_ease(0.6, 0.0, 0.18, 1.0)
    p = fly(min(1, max(0, (t - FLY_START) / (FLY_END - FLY_START))))
    k = math.exp(math.log(K0) + (math.log(K_END) - math.log(K0)) * p)
    cx = C0[0] + (C_END[0] - C0[0]) * p
    cy = C0[1] + (C_END[1] - C0[1]) * p - 10 * math.sin(math.pi * p)   # arc up a little on the way in
    theta = math.radians(6) * math.sin(math.pi * p) * (1 - p)           # bank, settling level
    if t < FLY_START:                                                   # anticipation dip before take-off
        q = math.sin(math.pi * t / FLY_START)
        k *= 1 - 0.045 * q
        cy += 2.5 * q
    return k, (cx, cy), theta


def intro_frames(layers, welcome_last):
    mascot, nofill = mips(layers['mascot']), mips(face_only(layers['nofill']))
    screen = mips(face_only(layers['nofill'], SCREEN_RECT))
    bg_layer = layers['scene_bg']
    lemon = Image.new('RGBA', (W * SS, H * SS), LEMON + (255,))
    frames = []
    n = round(INTRO_LEN * FPS)
    for i in range(n + 1):
        t = i / FPS
        canvas = lemon.copy()
        if t < DOCK_START:
            fade = 1 - smoothstep(0.0, 0.3, t)
            if fade > 0:                                                 # wordmark, foods and aura drop away
                bg = bg_layer.copy(); bg.putalpha(int(255 * fade))
                canvas.alpha_composite(bg, (0, int(2 * SS + 10 * SS * (1 - fade) ** 2)))
            subs = [t + (j / 7 - .5) / (2 * FPS) for j in range(8)] if FLY_START < t < FLY_END else [t]   # 1/60 s shutter
            acc = None
            for st in subs:                                              # temporal supersampling = motion blur
                k, c, th = fly_state(st)
                layer = place(mascot, k, c, th)
                frame = canvas.copy(); frame.alpha_composite(layer)
                arr = np.asarray(frame, dtype=np.float32)
                acc = arr if acc is None else acc + arr
            img = Image.fromarray((acc / len(subs)).astype(np.uint8))
        else:
            p = ease_in_out(min(1, (t - DOCK_START) / (DOCK_END - DOCK_START)))
            k = math.exp(math.log(K_END) + (math.log(avatar_k()) - math.log(K_END)) * p)
            c = (C_END[0] + (avatar_center()[0] - C_END[0]) * p, C_END[1] + (avatar_center()[1] - C_END[1]) * p)
            face = canvas.copy(); face.alpha_composite(place(nofill, k, c))
            glints = smoothstep(0.0, 0.3, p)                            # screen glints fade as the face lifts off
            if glints < 1:
                full = canvas.copy(); full.alpha_composite(place(screen, k, c))   # glints, never the body
                img = Image.blend(full, face, glints)
            else:
                img = face
        small = img.convert('RGB').resize((W, H), Image.LANCZOS)
        if i < 4:                                                        # hand-off from the real welcome frame
            small = Image.blend(welcome_last, small, i / 4)
        frames.append(small)
    return frames


def avatar_k():
    x0, y0, x1, y1 = AVATAR_BOX
    return min((x1 - x0) / (FACE_RECT[2] - FACE_RECT[0]), (y1 - y0) / (FACE_RECT[3] - FACE_RECT[1]))


def avatar_center():
    x0, y0, x1, y1 = AVATAR_BOX
    k = avatar_k()
    # FACE_CENTER sits slightly off the rectangle centre; keep the rectangle centred in the box.
    rx, ry = (FACE_RECT[0] + FACE_RECT[2]) / 2, (FACE_RECT[1] + FACE_RECT[3]) / 2
    return ((x0 + x1) / 2 + (FACE_CENTER[0] - rx) * k, (y0 + y1) / 2 + (FACE_CENTER[1] - ry) * k)


def rle_encode(indices):
    out, i = bytearray(), 0
    while i < len(indices):
        v, j = indices[i], i
        while j < len(indices) and indices[j] == v and j - i < 255:
            j += 1
        out += bytes((j - i, v)); i = j
    return out


def rgb565(c):
    r, g, b = c
    return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)


def encode_intro(frames):
    picks = frames[:2] * 3 + frames[::3]          # weight the hand-off frames so the pink tagline keeps its colour
    sheet = Image.new('RGB', (W * len(picks), H))
    for j, f in enumerate(picks):
        sheet.paste(f, (j * W, 0))
    pal_img = sheet.quantize(colors=64, method=Image.Quantize.MAXCOVERAGE)
    palette = pal_img.getpalette()[:64 * 3]
    data, offsets, decoded = bytearray(), [0], []
    for f in frames:
        q = f.quantize(palette=pal_img, dither=Image.Dither.NONE)
        idx = list(q.get_flattened_data())
        enc = rle_encode(idx)
        # round-trip check
        back, o = [], 0
        while o < len(enc):
            back += [enc[o + 1]] * enc[o]; o += 2
        assert back == idx
        data += enc; offsets.append(len(data))
        decoded.append(q.convert('RGB'))
    pal565 = [rgb565(tuple(palette[i * 3:i * 3 + 3])) for i in range(64)]
    return pal565, offsets, data, decoded


# ---------- avatar sprites ----------
def alpha_mask_from(img_rgba_ss, box):
    """Downscale a supersampled RGBA layer and return the alpha of `box` (display px) as 0..15 values."""
    small = img_rgba_ss.resize((W, H), Image.LANCZOS)
    a = small.getchannel('A').crop(box)
    return a


def avatar_sprites(layers):
    k, c = avatar_k(), avatar_center()
    opened = place(mips(face_only(layers['nofill'])), k, c)
    eyes = layers['eyes']
    bbox = eyes.crop(tuple((v + 12) * PX_PER_UNIT for v in FACE_RECT)).getbbox()
    ex0, ey0, ex1, ey1 = [b + (FACE_RECT[i % 2] + 12) * PX_PER_UNIT for i, b in enumerate(bbox)]
    squashed = Image.new('RGBA', eyes.size, (0, 0, 0, 0))
    region = eyes.crop((ex0, ey0, ex1, ey1))
    hh = max(6, (ey1 - ey0) // 7)
    squashed.paste(region.resize((ex1 - ex0, hh), Image.LANCZOS), (ex0, ey0 + (ey1 - ey0) * 3 // 5))
    blink_layer = layers['noeyes'].copy(); blink_layer.alpha_composite(squashed)
    blinked = place(mips(face_only(blink_layer)), k, c)
    return alpha_mask_from(opened, AVATAR_BOX), alpha_mask_from(blinked, AVATAR_BOX)


# ---------- fonts ----------
FONT_SPECS = [  # name, file, px size, characters
    ('FONT_TIME', 'DMSans-Display.ttf', 40, '0123456789:-'),
    ('FONT_FIGURE', 'DMSans-Display.ttf', 26, '0123456789-+'),
    ('FONT_TITLE', 'DMSans-Heading.ttf', 14, None),
    ('FONT_HEAD', 'DMSans-Heading.ttf', 11, None),
    ('FONT_LABEL', 'DMSans-SemiBold.ttf', 11, None),
    ('FONT_BODY', 'DMSans.ttf', 11, None),
    ('FONT_SMALL', 'DMSans-SemiBold.ttf', 9, None),
]
ASCII = ''.join(chr(c) for c in range(32, 127)) + '\u00b7'   # '\x7f' in firmware strings draws a middle dot


def build_font(file, size, chars):
    font = ImageFont.truetype(str(FONTS / file), size)
    ascent, descent = font.getmetrics()
    chars = chars or ASCII
    glyphs, bitmap = [], bytearray()
    for ch in chars:
        mask, (bx0, by0) = font.getmask2(ch, mode='L')   # offset is relative to the ascent line
        w, h = mask.size
        adv = round(font.getlength(ch))
        pix = bytes(mask)
        nib = [min(15, (v + 8) // 17) for v in pix]
        if len(nib) % 2:
            nib.append(0)
        off = len(bitmap)
        bitmap += bytes((nib[i] << 4) | nib[i + 1] for i in range(0, len(nib), 2))
        glyphs.append((127 if ch == '\u00b7' else ord(ch), off, w, h, bx0, by0, adv))
    return glyphs, bitmap, ascent, descent


# ---------- icons (drawn at 8x, downsampled) ----------
def icon(name, size):
    S = 8; s = size * S
    im = Image.new('L', (s, s), 0); d = ImageDraw.Draw(im)
    lw = max(S * 2, int(s * 0.11))
    def p(x, y): return (x * s, y * s)
    if name == 'home':
        d.line([p(.12, .5), p(.5, .14), p(.88, .5)], fill=255, width=lw, joint='curve')
        d.rounded_rectangle([p(.24, .44), p(.76, .88)], radius=s * .08, outline=255, width=lw)
    elif name == 'bag':
        d.rounded_rectangle([p(.16, .34), p(.84, .9)], radius=s * .1, outline=255, width=lw)
        d.arc([p(.32, .1), p(.68, .52)], 180, 360, fill=255, width=lw)
    elif name == 'pot':
        d.rounded_rectangle([p(.14, .38), p(.86, .86)], radius=s * .12, outline=255, width=lw)
        d.line([p(.04, .46), p(.14, .46)], fill=255, width=lw); d.line([p(.86, .46), p(.96, .46)], fill=255, width=lw)
        for x in (.36, .5, .64):
            d.arc([p(x - .06, .06), p(x + .06, .3)], 90, 270, fill=255, width=max(S, lw // 2))
    elif name == 'mic':
        d.rounded_rectangle([p(.36, .08), p(.64, .6)], radius=s * .14, outline=255, width=lw)
        d.arc([p(.2, .26), p(.8, .76)], 0, 180, fill=255, width=lw)
        d.line([p(.5, .76), p(.5, .92)], fill=255, width=lw); d.line([p(.32, .92), p(.68, .92)], fill=255, width=lw)
    elif name == 'check':
        d.line([p(.18, .52), p(.42, .76), p(.84, .26)], fill=255, width=int(lw * 1.3), joint='curve')
    elif name == 'wifi':
        for r in (.42, .28):
            d.arc([p(.5 - r, .78 - r), p(.5 + r, .78 + r)], 225, 315, fill=255, width=lw)
        d.ellipse([p(.42, .7), p(.58, .86)], fill=255)
    elif name == 'clock':
        d.ellipse([p(.1, .1), p(.9, .9)], outline=255, width=lw)
        d.line([p(.5, .26), p(.5, .52), p(.68, .62)], fill=255, width=lw, joint='curve')
    elif name == 'leaf':
        d.ellipse([p(.14, .14), p(.86, .86)], outline=255, width=lw)
        d.line([p(.3, .7), p(.72, .3)], fill=255, width=lw)
    return im.resize((size, size), Image.LANCZOS)


ICON_SPECS = [('ICON_HOME', 'home', 13), ('ICON_BAG', 'bag', 13), ('ICON_POT', 'pot', 13), ('ICON_MIC', 'mic', 13),
              ('ICON_BAG_L', 'bag', 30), ('ICON_POT_L', 'pot', 30), ('ICON_MIC_L', 'mic', 34),
              ('ICON_CHECK', 'check', 9), ('ICON_WIFI', 'wifi', 10), ('ICON_CLOCK', 'clock', 10), ('ICON_LEAF', 'leaf', 10)]


def pack4(img):
    vals = [min(15, (v + 8) // 17) for v in img.get_flattened_data()]
    if len(vals) % 2:
        vals.append(0)
    return bytes((vals[i] << 4) | vals[i + 1] for i in range(0, len(vals), 2))


def c_bytes(b, per=24):
    return '\n'.join('  ' + ', '.join(f'0x{v:02X}' for v in b[i:i + per]) + ',' for i in range(0, len(b), per))


def main():
    PREVIEW.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        layers = render_layers(Path(tmp))
    frames = intro_frames(layers, last_welcome_frame())
    pal, offsets, data, decoded = encode_intro(frames)
    dock_frame = round(DOCK_START * FPS)
    h = ['#pragma once', '#include <stdint.h>', '// Generated by tools/build_assets.py. Do not edit.',
         '// Remi flies at the viewer, the camera dives into his screen face, and the face docks top-left.',
         '// Each frame: (run length 1..255, palette index) byte pairs covering 128x160 pixels.',
         f'static const uint16_t INTRO_FRAME_COUNT = {len(frames)};',
         f'static const uint16_t INTRO_FPS = {FPS};',
         f'static const uint16_t INTRO_DOCK_FRAME = {dock_frame};   // face starts docking here; fade the UI in from this point',
         f'static const uint16_t INTRO_ZOOM_FRAME = {round(FLY_START * FPS)};   // take-off',
         'static const uint16_t INTRO_PALETTE[64] = {' + ', '.join(f'0x{v:04X}' for v in pal) + '};',
         'static const uint32_t INTRO_OFFSETS[] = {' + ', '.join(map(str, offsets)) + '};',
         'static const uint8_t INTRO_DATA[] = {', c_bytes(data), '};']
    (FW / 'intro_frames.h').write_text('\n'.join(h) + '\n')

    open_mask, blink_mask = avatar_sprites(layers)
    out = ['#pragma once', '#include <stdint.h>', '// Generated by tools/build_assets.py. Do not edit.',
           'struct Glyph { uint16_t code; uint32_t offset; uint8_t w, h; int8_t x, y; uint8_t adv; };',
           'struct Font { const Glyph *glyphs; uint16_t count; const uint8_t *bitmap; uint8_t ascent, descent; };',
           'struct Mask { const uint8_t *data; uint8_t w, h; };  // 4-bit alpha, two pixels per byte, high nibble first', '']
    for name, file, size, chars in FONT_SPECS:
        glyphs, bitmap, asc, desc = build_font(file, size, chars)
        out.append(f'static const uint8_t {name}_BITS[] = {{\n{c_bytes(bitmap)}\n}};')
        out.append(f'static const Glyph {name}_GLYPHS[] = {{' + ', '.join('{%d,%d,%d,%d,%d,%d,%d}' % g for g in glyphs) + '};')
        out.append(f'static const Font {name} = {{{name}_GLYPHS, {len(glyphs)}, {name}_BITS, {asc}, {desc}}};\n')
    masks = [(n, icon(k, s)) for n, k, s in ICON_SPECS] + [('AVATAR_OPEN', open_mask), ('AVATAR_BLINK', blink_mask)]
    for name, img in masks:
        out.append(f'static const uint8_t {name}_BITS[] = {{\n{c_bytes(pack4(img))}\n}};')
        out.append(f'static const Mask {name} = {{{name}_BITS, {img.width}, {img.height}}};\n')
    out.append(f'static const int16_t AVATAR_X = {AVATAR_BOX[0]}, AVATAR_Y = {AVATAR_BOX[1]};')
    (FW / 'ui_assets.h').write_text('\n'.join(out) + '\n')

    sheet = Image.new('RGB', (W * 8, H * 2), 'black')
    picks = [round(j * (len(decoded) - 1) / 15) for j in range(16)]
    for j, f in enumerate(picks):
        sheet.paste(decoded[f], ((j % 8) * W, (j // 8) * H))
    sheet.resize((W * 8 * 2, H * 4), Image.NEAREST).save(PREVIEW / 'intro-contact-sheet.png')
    print(f'intro: {len(frames)} frames, {len(data)} bytes RLE, largest frame {max(b - a for a, b in zip(offsets, offsets[1:]))} bytes')


if __name__ == '__main__':
    sys.exit(main())
