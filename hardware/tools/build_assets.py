"""Builds every generated asset for the Remi device firmware.

Outputs (firmware/Remi_Device/):
  boot_frames.h   the whole boot: Remi flies in, lands, waves, food pops out, the logo writes itself,
                  then the camera dives into his face and it docks top-left (palette + skip RLE, sound cues)
  ui_assets.h     anti-aliased fonts, icons and the blinking face avatar (4-bit alpha masks)
Previews (preview/): boot contact sheet.

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
BG_HEX = 0xF2C230                          # device background: a deeper "Remi gold" than the app's lemon, which
BG = ((BG_HEX >> 16) & 255, (BG_HEX >> 8) & 255, BG_HEX & 255)   # washes out to near-white on the TN TFT
COBALT = (23, 52, 190)
CROP = (0, 150, 390, 487.5)                # scene units shown on the 128x160 screen (x, y, w, h)
S0 = W / CROP[2]                           # display px per scene unit
PX_PER_UNIT = 4                            # mascot raster resolution; mascot viewBox starts at -12
FACE_CENTER = (340.6, 276.6)               # centre of Remi's screen interior, mascot units (measured by flood fill)
FACE_RECT = (268, 210, 414, 338)           # brows, eyes and smile only; excludes the screen border and glints
SCREEN_RECT = (241, 179, 440, 374)         # inside the screen border: face plus glints, no body
K0 = 0.475 * S0                            # px per mascot unit when Remi stands in the scene (translate(52 184) scale(.475))
C0 = ((52 + 0.475 * FACE_CENTER[0]) * S0, (184 + 0.475 * FACE_CENTER[1] - CROP[1]) * S0)
K_END, C_END = 164 / 202.25, (64.0, 80.0)  # screen interior (202 units tall) slightly overfills the 160 px height
AVATAR_BOX = (5, 3, 31, 25)                # where the face docks on every page (x0, y0, x1, y1)


# ---------- rasterize vector layers ----------
def render_layers(tmp: Path):
    mascot = recolor((VECTORS / 'mascot.svg').read_text())

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


def recolor(svg):
    """The vectors use the app's lemon for the ground and Remi's screen; the device uses BG."""
    svg = re.sub(r'(<rect id="ground"[^>]*fill=")#F8E45C', r'\g<1>#%06X' % BG_HEX, svg)
    return re.sub(r'(<path id="panelFill"[^>]*fill=")#F8E45C', r'\g<1>#%06X' % BG_HEX, svg)


# ---------- phase A: the welcome, re-staged and rendered from the vectors ----------
# Timeline (seconds). Remi flies in from far away, lands with a squash, waves three times while the
# food pops out, the logo writes itself (1.5 s, was 3.6 s) and the tagline settles.
A = dict(fly_end=1.05, wave_start=1.25, foods=(1.70, 1.80, 1.90, 2.00), food_dur=0.60,
         write_start=2.15, write_len=1.50, tagline=3.62, blinks=(1.62, 3.55), end=4.05)
WAVE_KEYS = [[0, 0], [.15, -14], [.32, 10], [.49, -14], [.66, 10], [.83, -11], [1.0, 6], [1.2, 0]]
TAGLINE = 'Cooking made fun'

PAINT_JS = r"""
const clamp=x=>Math.max(0,Math.min(1,x));
function ease(x,x1=.23,y1=1,x2=.32,y2=1){let lo=0,hi=1,t=x;for(let i=0;i<22;i++){t=(lo+hi)/2;let a=1-t,v=3*a*a*t*x1+3*a*t*t*x2+t*t*t;if(v<x)lo=t;else hi=t}let a=1-t;return 3*a*a*t*y1+3*a*t*t*y2+t*t*t}
function disk(x,y,r){return `M${x+r},${y}A${r},${r} 0 1,0 ${x-r},${y}A${r},${r} 0 1,0 ${x+r},${y}Z`}
function star(x,y,r){return `M${x-r} ${y}Q${x} ${y} ${x} ${y-r}Q${x} ${y} ${x+r} ${y}Q${x} ${y} ${x} ${y+r}Q${x} ${y} ${x-r} ${y}Z`}
function wave(t){t-=A.wave_start;const k=WAVE;if(t<=0||t>=k.at(-1)[0])return 0;for(let i=1;i<k.length;i++){const[a,v]=k[i-1],[b,w]=k[i];if(t<=b){const u=(t-a)/(b-a);return v+(w-v)*(1-Math.cos(Math.PI*u))/2}}return 0}
function paint(p,t,hideChar){
  const E=id=>document.getElementById(p+id);
  const ch=E('character');
  // fly in from far away (up and right), banking, then land with a squash
  let tr;
  if(t<A.fly_end){
    const q=clamp(t/A.fly_end),u=1-Math.pow(1-q,3),S0=.028,S1=.475;
    const S=Math.exp(Math.log(S0)+(Math.log(S1)-Math.log(S0))*u);
    const ax=300+(208.75-300)*u, ay=205+(326.5-205)*u-34*Math.sin(Math.PI*u);
    tr=`translate(${ax-S*330} ${ay-S*300}) scale(${S}) rotate(${16*(1-u)} 330 300)`;
    ch.setAttribute('opacity',clamp(t/.12));
  }else{
    const tau=t-A.fly_end,k=Math.exp(-tau*8)*Math.sin(tau*22);
    tr=`translate(52 184) scale(.475) translate(345 582) scale(${1+.05*k} ${1-.085*k}) translate(-345 -582)`;
    ch.setAttribute('opacity',1);
  }
  ch.setAttribute('transform',tr);
  if(hideChar)ch.setAttribute('display','none');
  E('glove').setAttribute('transform',`rotate(${wave(t)} 519 367)`);
  let blink=1;for(const b of A.blinks)blink-=.94*Math.exp(-(((t-b)/.065)**2));
  E('eyes').setAttribute('transform',`translate(0 ${266*(1-blink)}) scale(1 ${blink})`);
  [0,1,2].forEach(i=>E('wisp'+i).setAttribute('transform',`translate(${Math.sin(t*2.2+i*.9)*1.8} ${-Math.sin(t*1.9+i*.6)*2.4})`));
  E('auraField').setAttribute('opacity',.24*clamp((t-.9)/.5));
  // logo writing, same ink guides as the app, compressed to A.write_len
  let tip=null,pen=0;
  GUIDES.forEach((g,i)=>{
    const s=A.write_start+(g.start-.58)*A.write_len/3.6,e=A.write_start+(g.end-.58)*A.write_len/3.6;
    const q=clamp((t-s)/(e-s));let d='';
    if(q>0){if(g.points.length===1){tip=g.points[0];d=g.disks[0]}else{let dist=q*g.length,j=0;
      while(j<g.distances.length-2&&g.distances[j+1]<=dist)j++;
      const a=g.points[j],b=g.points[j+1],u=(dist-g.distances[j])/((g.distances[j+1]-g.distances[j])||1),pt=a.map((v,k)=>v+(b[k]-v)*u);
      d=g.disks.slice(0,j+1).join('')+disk(...pt);if(q<1)tip=pt}}
    E('inkStroke'+i).setAttribute('d',d);E('inkStroke'+i).setAttribute('opacity',q>0?1:0);
    if(q>0&&q<1)pen=1;
  });
  E('penTip').setAttribute('opacity',pen);
  if(tip){const x=57+tip[0]*.405,y=485+tip[1]*.405;E('penGlow').setAttribute('cx',x);E('penGlow').setAttribute('cy',y);E('penStar').setAttribute('d',star(x,y,4))}
  E('subtitle').setAttribute('opacity',0);   // drawn crisp at panel resolution by the builder
  FOODS.forEach((f,i)=>{const q=clamp((t-A.foods[i])/A.food_dur),u=ease(q),[cx,cy]=f.control,[ex,ey]=f.end;
    const bob=q>=1?Math.sin((t-A.foods[i])*3+i)*1.2:0;
    const x=(1-u)**2*195+2*(1-u)*u*cx+u*u*ex,y=(1-u)**2*370+2*(1-u)*u*cy+u*u*ey+bob,g=E('food-'+f.id);
    g.setAttribute('transform',`translate(${x} ${y}) rotate(${f.angle*u}) scale(${.6+.4*u})`);g.setAttribute('opacity',Math.min(1,q*7))});
}
"""


def render_scene_frames(tmp, times, hide_char=False):
    """Paint the welcome scene at each time in headless Chrome, 40 per screenshot. Returns RGB images at 4x."""
    scene = recolor((VECTORS / 'scene.svg').read_text())
    scene = re.sub(r'viewBox="[^"]+"', 'viewBox="%g %g %g %g"' % CROP, scene, count=1)
    scene = re.sub(r'\swidth="\d+" height="\d+"', '', scene, count=1)
    ids = sorted(set(re.findall(r'id="([^"]+)"', scene)), key=len, reverse=True)
    guides = (VECTORS / 'ink-guides.json').read_text()
    foods = __import__('json').loads((VECTORS / 'motion.json').read_text())['foods']
    cols, rows, cw, chh = 8, 5, W * SS, H * SS
    out = []
    for b0 in range(0, len(times), cols * rows):
        batch = times[b0:b0 + cols * rows]
        clones = []
        for k in range(len(batch)):
            svg = scene
            for i in ids:  # unique ids per clone so masks and gradients don't collide
                svg = svg.replace(f'id="{i}"', f'id="c{k}_{i}"').replace(f'url(#{i})', f'url(#c{k}_{i})')
            clones.append(f'<div style="position:absolute;left:{(k % cols) * cw}px;top:{(k // cols) * chh}px;width:{cw}px;height:{chh}px">{svg}</div>')
        js = (f'const A={__import__("json").dumps(A)};const WAVE={WAVE_KEYS};const GUIDES={guides};'
              f'const FOODS={__import__("json").dumps(foods)};{PAINT_JS}'
              + ''.join(f'paint("c{k}_",{t},{str(hide_char).lower()});' for k, t in enumerate(batch)))
        html = (f'<html><head><style>html,body{{margin:0;background:#000}}svg{{width:{cw}px;height:{chh}px;display:block}}</style></head>'
                f'<body>{"".join(clones)}<script>{js}</script></body></html>')
        page = tmp / f'scene{b0}.html'
        page.write_text(html)
        shot = tmp / f'scene{b0}.png'
        subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--force-device-scale-factor=1',
                        f'--window-size={cols * cw},{rows * chh}', f'--screenshot={shot}', page.as_uri()], capture_output=True, check=True)
        sheet = Image.open(shot).convert('RGB')
        for k in range(len(batch)):
            out.append(sheet.crop(((k % cols) * cw, (k // cols) * chh, (k % cols + 1) * cw, (k // cols + 1) * chh)))
    return out


def draw_tagline(img, alpha):
    """Crisp tagline at panel resolution (a vector subtitle blurs at 128 px wide)."""
    if alpha <= 0:
        return img
    font = ImageFont.truetype(str(FONTS / 'DMSans-SemiBold.ttf'), 10)
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    w = d.textlength(TAGLINE, font=font)
    d.text(((W - w) / 2, 145 + 3 * (1 - alpha)), TAGLINE, font=font, fill=(194, 20, 108, int(255 * alpha)))
    out = img.convert('RGBA'); out.alpha_composite(layer)
    return out.convert('RGB')


def tagline_alpha(t):
    return smoothstep(A['tagline'], A['tagline'] + 0.3, t)


def phase_a(tmp):
    n = round(A['end'] * FPS)
    times, groups = [], []
    for i in range(n):
        t = i / FPS
        sub = [t + (j / 3 - .5) / (2 * FPS) for j in range(4)] if t < A['fly_end'] + 0.05 else [t]  # motion blur on the fly-in
        groups.append((len(times), len(sub)))
        times += sub
    shots = render_scene_frames(tmp, times)
    frames = []
    for i, (o, c) in enumerate(groups):
        acc = sum(np.asarray(img, dtype=np.float32) for img in shots[o:o + c]) / c
        small = Image.fromarray(acc.astype(np.uint8)).resize((W, H), Image.LANCZOS)
        frames.append(draw_tagline(small, tagline_alpha(i / FPS)))
    bg = render_scene_frames(tmp, [A['end']], hide_char=True)[0]   # final scene without Remi, for the dive
    tag = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    tag = draw_tagline(Image.new('RGB', (W, H), (0, 0, 0)), 1.0)  # tagline mask, scaled up to the 4x backdrop
    mask = Image.fromarray((np.asarray(tag).max(axis=2) > 0).astype(np.uint8) * 255).resize((W * SS, H * SS), Image.NEAREST)
    bg_rgba = bg.convert('RGBA')
    bg_rgba.paste(Image.new('RGBA', bg.size, (194, 20, 108, 255)), (0, 0), mask)
    return frames, bg_rgba


# ---------- phase B: Remi flies at the viewer, the camera dives into his face, the face docks ----------
FLY_START, FLY_END = 0.12, 1.25            # seconds within phase B
DOCK_START, DOCK_END = 1.36, 1.92
B_LEN = DOCK_END


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


def phase_b(layers, bg_layer, a_last):
    mascot, nofill = mips(layers['mascot']), mips(face_only(layers['nofill']))
    screen = mips(face_only(layers['nofill'], SCREEN_RECT))
    plain = Image.new('RGBA', (W * SS, H * SS), BG + (255,))
    frames = []
    n = round(B_LEN * FPS)
    for i in range(1, n + 1):
        t = i / FPS
        canvas = plain.copy()
        if t < DOCK_START:
            fade = 1 - smoothstep(0.0, 0.18, t)                         # exits stay short so cobalt never reads as grey
            if fade > 0:                                                 # wordmark, foods and tagline drop away
                bg = bg_layer.copy(); bg.putalpha(bg_layer.getchannel('A').point(lambda v: int(v * fade)))
                canvas.alpha_composite(bg, (0, int(16 * SS * (1 - fade) ** 2)))
            subs = [t + (j / 7 - .5) / (2 * FPS) for j in range(8)] if FLY_START < t < FLY_END else [t]   # 1/60 s shutter
            acc = None
            for st in subs:                                              # temporal supersampling = motion blur
                k, c, th = fly_state(st)
                frame = canvas.copy(); frame.alpha_composite(place(mascot, k, c, th))
                arr = np.asarray(frame, dtype=np.float32)
                acc = arr if acc is None else acc + arr
            img = Image.fromarray((acc / len(subs)).astype(np.uint8))
        else:
            p = ease_in_out(min(1, (t - DOCK_START) / (DOCK_END - DOCK_START)))
            k = math.exp(math.log(K_END) + (math.log(avatar_k()) - math.log(K_END)) * p)
            c = (C_END[0] + (avatar_center()[0] - C_END[0]) * p, C_END[1] + (avatar_center()[1] - C_END[1]) * p)
            face = canvas.copy(); face.alpha_composite(place(nofill, k, c))
            glints = smoothstep(0.0, 0.3, p)                             # screen glints fade as the face lifts off
            if glints < 1:
                full = canvas.copy(); full.alpha_composite(place(screen, k, c))
                img = Image.blend(full, face, glints)
            else:
                img = face
        small = img.convert('RGB').resize((W, H), Image.LANCZOS)
        if i <= 3:                                                       # hide tiny raster differences at the hand-off
            small = Image.blend(a_last, small, i / 4)
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


SKIP = 255   # palette index meaning "same as the previous frame"
PALETTE_SIZE = 192


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


def encode_frames(frames):
    picks = frames[::2]
    sheet = Image.new('RGB', (W * len(picks), H))
    for j, f in enumerate(picks):
        sheet.paste(f, (j * W, 0))
    pal_img = sheet.quantize(colors=PALETTE_SIZE, method=Image.Quantize.MAXCOVERAGE)
    palette = pal_img.getpalette()[:PALETTE_SIZE * 3]
    data, offsets, decoded, prev = bytearray(), [0], [], None
    for f in frames:
        q = f.quantize(palette=pal_img, dither=Image.Dither.NONE)
        idx = list(q.get_flattened_data())
        coded = idx if prev is None else [SKIP if a == b else a for a, b in zip(idx, prev)]
        enc = rle_encode(coded)
        back, o = [], 0                                    # round-trip check against the previous frame
        while o < len(enc):
            back += [enc[o + 1]] * enc[o]; o += 2
        assert [p if v == SKIP else v for v, p in zip(back, prev or back)] == idx
        data += enc; offsets.append(len(data)); prev = idx
        decoded.append(q.convert('RGB'))
    pal565 = [rgb565(tuple(palette[i * 3:i * 3 + 3])) for i in range(PALETTE_SIZE)]
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


BIG_BOX = (22, 40, 106, 114)   # Remi's large face for the ambient fridge mode, drawn in three layers


def big_face(layers):
    """Brows+smile, open eyes and shut eyes as separate masks, so the eyes can look around and blink."""
    x0, y0, x1, y1 = BIG_BOX
    k = min((x1 - x0) / (FACE_RECT[2] - FACE_RECT[0]), (y1 - y0) / (FACE_RECT[3] - FACE_RECT[1]))
    rx, ry = (FACE_RECT[0] + FACE_RECT[2]) / 2, (FACE_RECT[1] + FACE_RECT[3]) / 2
    c = ((x0 + x1) / 2 + (FACE_CENTER[0] - rx) * k, (y0 + y1) / 2 + (FACE_CENTER[1] - ry) * k)
    eyes = layers['eyes']
    bbox = eyes.crop(tuple((v + 12) * PX_PER_UNIT for v in FACE_RECT)).getbbox()
    ex0, ey0, ex1, ey1 = [b + (FACE_RECT[i % 2] + 12) * PX_PER_UNIT for i, b in enumerate(bbox)]
    shut = Image.new('RGBA', eyes.size, (0, 0, 0, 0))
    shut.paste(eyes.crop((ex0, ey0, ex1, ey1)).resize((ex1 - ex0, max(6, (ey1 - ey0) // 7)), Image.LANCZOS),
               (ex0, ey0 + (ey1 - ey0) * 3 // 5))
    out = []
    for layer in (layers['noeyes'], eyes, shut):
        out.append(alpha_mask_from(place(mips(face_only(layer)), k, c), BIG_BOX))
    return out


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
    elif name == 'calendar':
        d.rounded_rectangle([p(.12, .2), p(.88, .88)], radius=s * .1, outline=255, width=lw)
        d.line([p(.12, .4), p(.88, .4)], fill=255, width=lw)
        d.line([p(.32, .08), p(.32, .28)], fill=255, width=lw); d.line([p(.68, .08), p(.68, .28)], fill=255, width=lw)
    elif name == 'fridge':
        d.rounded_rectangle([p(.22, .06), p(.78, .94)], radius=s * .1, outline=255, width=lw)
        d.line([p(.22, .4), p(.78, .4)], fill=255, width=lw)
        d.line([p(.34, .18), p(.34, .3)], fill=255, width=lw); d.line([p(.34, .52), p(.34, .66)], fill=255, width=lw)
    elif name == 'chat':
        d.rounded_rectangle([p(.08, .14), p(.92, .7)], radius=s * .2, outline=255, width=lw)
        d.polygon([p(.26, .64), p(.44, .64), p(.24, .9)], fill=255)
    elif name == 'camera':
        d.rounded_rectangle([p(.06, .28), p(.94, .86)], radius=s * .12, outline=255, width=lw)
        d.rectangle([p(.34, .14), p(.66, .3)], fill=255)
        d.ellipse([p(.32, .38), p(.68, .74)], outline=255, width=lw)
    elif name == 'link':
        d.rounded_rectangle([p(.3, .06), p(.7, .94)], radius=s * .1, outline=255, width=lw)
        d.line([p(.44, .82), p(.56, .82)], fill=255, width=lw)
    elif name == 'barcode':
        for x, w in ((.12, .06), (.24, .03), (.32, .08), (.46, .03), (.54, .05), (.66, .03), (.74, .07), (.86, .03)):
            d.rectangle([p(x, .2), p(x + w, .8)], fill=255)
    elif name == 'label':
        d.rounded_rectangle([p(.18, .08), p(.82, .92)], radius=s * .08, outline=255, width=lw)
        for y in (.3, .48, .66):
            d.line([p(.32, y), p(.68, y)], fill=255, width=max(S, lw // 2))
    elif name == 'list':
        for y in (.24, .5, .76):
            d.ellipse([p(.1, y - .07), p(.24, y + .07)], fill=255)
            d.line([p(.36, y), p(.9, y)], fill=255, width=lw)
    elif name == 'receipt':
        d.polygon([p(.2, .06), p(.8, .06), p(.8, .94), p(.7, .86), p(.6, .94), p(.5, .86), p(.4, .94), p(.3, .86), p(.2, .94)], outline=255, width=lw)
        for y in (.3, .48):
            d.line([p(.34, y), p(.66, y)], fill=255, width=max(S, lw // 2))
    return im.resize((size, size), Image.LANCZOS)


ICON_SPECS = [('ICON_HOME', 'home', 13), ('ICON_BAG', 'bag', 13), ('ICON_POT', 'pot', 13), ('ICON_MIC', 'mic', 13),
              ('ICON_BAG_L', 'bag', 30), ('ICON_POT_L', 'pot', 30), ('ICON_MIC_L', 'mic', 34),
              ('ICON_CHECK', 'check', 9), ('ICON_WIFI', 'wifi', 10), ('ICON_CLOCK', 'clock', 10), ('ICON_LEAF', 'leaf', 10),
              # page rail, one per page, in page order
              ('RAIL_HOME', 'home', 10), ('RAIL_TODAY', 'calendar', 10), ('RAIL_KITCHEN', 'fridge', 10), ('RAIL_BAG', 'bag', 10),
              ('RAIL_COOK', 'pot', 10), ('RAIL_ASK', 'chat', 10), ('RAIL_SCAN', 'camera', 10), ('RAIL_LINK', 'link', 10),
              # Scan page
              ('ICON_CAMERA_L', 'camera', 30), ('ICON_BARCODE', 'barcode', 16), ('ICON_LABEL', 'label', 16),
              ('ICON_LIST', 'list', 16), ('ICON_RECEIPT', 'receipt', 16)]


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
        a_frames, bg_final = phase_a(Path(tmp))
    frames = a_frames + phase_b(layers, bg_final, a_frames[-1])
    pal, offsets, data, decoded = encode_frames(frames)
    tb = A['end']   # phase B starts here
    cues = [(0.0, 'Music'), (0.02, 'FlyIn'), (A['fly_end'], 'Boing'), (A['wave_start'] + 0.1, 'Hello')]
    cues += [(t, f'Pop{i}') for i, t in enumerate(A['foods'])]
    cues += [(A['write_start'], 'Scribble'), (A['write_start'] + A['write_len'], 'Chime'),
             (tb + FLY_START, 'Whoosh'), (tb + DOCK_START - 0.13, 'Land'), (tb + DOCK_START + 0.27, 'Sparkle')]
    h = ['#pragma once', '#include <stdint.h>', '// Generated by tools/build_assets.py. Do not edit.',
         '// The boot: Remi flies in, lands, waves, food pops out, the logo writes itself, then the camera',
         '// dives into his face and it docks top-left. Include after app_data.h (cues name Sfx values).',
         '// Each frame: (run length 1..255, palette index) byte pairs; index 255 keeps the previous frame\'s pixel.',
         f'static const uint16_t BOOT_FRAME_COUNT = {len(frames)};',
         f'static const uint16_t BOOT_FPS = {FPS};',
         f'static const uint16_t BOOT_DIVE_FRAME = {round(tb * FPS)};',
         'static const uint8_t BOOT_SKIP = 255;',
         'static const uint16_t BOOT_PALETTE[%d] = {' % PALETTE_SIZE + ', '.join(f'0x{v:04X}' for v in pal) + '};',
         'struct BootCue { uint16_t frame; Sfx sound; };',
         'static const BootCue BOOT_CUES[] = {' + ', '.join(f'{{{round(t * FPS)}, Sfx::{n}}}' for t, n in sorted(cues)) + '};',
         'static const uint32_t BOOT_OFFSETS[] = {' + ', '.join(map(str, offsets)) + '};',
         'static const uint8_t BOOT_DATA[] = {', c_bytes(data), '};']
    (FW / 'boot_frames.h').write_text('\n'.join(h) + '\n')

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
    big_features, big_eyes, big_shut = big_face(layers)
    masks = [(n, icon(k, s)) for n, k, s in ICON_SPECS] + [('AVATAR_OPEN', open_mask), ('AVATAR_BLINK', blink_mask),
             ('BIG_FEATURES', big_features), ('BIG_EYES', big_eyes), ('BIG_EYES_SHUT', big_shut)]
    for name, img in masks:
        out.append(f'static const uint8_t {name}_BITS[] = {{\n{c_bytes(pack4(img))}\n}};')
        out.append(f'static const Mask {name} = {{{name}_BITS, {img.width}, {img.height}}};\n')
    out.append(f'static const int16_t AVATAR_X = {AVATAR_BOX[0]}, AVATAR_Y = {AVATAR_BOX[1]};')
    out.append(f'#define REMI_BG 0x{BG_HEX:06X}  // device background colour (builder and firmware must match)')
    out.append(f'static const int16_t BIG_FACE_X = {BIG_BOX[0]}, BIG_FACE_Y = {BIG_BOX[1]};')
    (FW / 'ui_assets.h').write_text('\n'.join(out) + '\n')

    sheet = Image.new('RGB', (W * 8, H * 3), 'black')
    picks = [round(j * (len(decoded) - 1) / 23) for j in range(24)]
    for j, f in enumerate(picks):
        sheet.paste(decoded[f], ((j % 8) * W, (j // 8) * H))
    sheet.resize((W * 8 * 2, H * 6), Image.NEAREST).save(PREVIEW / 'boot-contact-sheet.png')
    print(f'boot: {len(frames)} frames ({len(frames) / FPS:.2f} s), {len(data)} bytes RLE, largest frame {max(b - a for a, b in zip(offsets, offsets[1:]))} bytes')


if __name__ == '__main__':
    sys.exit(main())
