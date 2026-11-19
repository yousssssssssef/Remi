#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace gfx {

uint16_t *const fb = static_cast<uint16_t *>(calloc(W * H, sizeof(uint16_t)));
static int ox = 0, oy = 0;

void setOrigin(int x, int y) { ox = x; oy = y; }

uint16_t blend(uint16_t d, uint16_t s, uint8_t a) {
  if (a == 255) return s;
  if (a == 0) return d;
  uint32_t a5 = (a + 4) >> 3;  // 0..32
  uint32_t dw = (d | (uint32_t(d) << 16)) & 0x07E0F81F, sw = (s | (uint32_t(s) << 16)) & 0x07E0F81F;
  uint32_t r = (dw + (((sw - dw) * a5) >> 5)) & 0x07E0F81F;
  return uint16_t(r | (r >> 16));
}

static inline void plot(int x, int y, uint16_t c, uint8_t a) {
  x += ox; y += oy;
  if (unsigned(x) >= unsigned(W) || unsigned(y) >= unsigned(H) || !a) return;
  uint16_t &p = fb[y * W + x];
  p = blend(p, c, a);
}

void fill(uint16_t c) {
  for (int i = 0; i < W * H; ++i) fb[i] = c;
}

void fillRect(int x, int y, int w, int h, uint16_t c, uint8_t alpha) {
  int x0 = x + ox, y0 = y + oy, x1 = x0 + w, y1 = y0 + h;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > W) x1 = W;
  if (y1 > H) y1 = H;
  for (int yy = y0; yy < y1; ++yy)
    for (int xx = x0; xx < x1; ++xx) fb[yy * W + xx] = blend(fb[yy * W + xx], c, alpha);
}

void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c, uint8_t alpha) {
  if (r * 2 > w) r = w / 2;
  if (r * 2 > h) r = h / 2;
  fillRect(x + r, y, w - 2 * r, h, c, alpha);
  fillRect(x, y + r, r, h - 2 * r, c, alpha);
  fillRect(x + w - r, y + r, r, h - 2 * r, c, alpha);
  for (int cy = 0; cy < r; ++cy)
    for (int cx = 0; cx < r; ++cx) {
      // coverage of the pixel centre against the corner circle, 1px soft edge
      float dx = r - cx - 0.5f, dy = r - cy - 0.5f;
      float cov = r + 0.5f - sqrtf(dx * dx + dy * dy);
      if (cov <= 0) continue;
      uint8_t a = uint8_t((cov >= 1 ? 1.f : cov) * alpha);
      plot(x + cx, y + cy, c, a);
      plot(x + w - 1 - cx, y + cy, c, a);
      plot(x + cx, y + h - 1 - cy, c, a);
      plot(x + w - 1 - cx, y + h - 1 - cy, c, a);
    }
}

static void disc(float cx, float cy, float rOuter, float rInner, uint16_t c, uint8_t alpha) {
  int x0 = int(floorf(cx - rOuter - 1)), x1 = int(ceilf(cx + rOuter + 1));
  int y0 = int(floorf(cy - rOuter - 1)), y1 = int(ceilf(cy + rOuter + 1));
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
      float cov = rOuter + 0.5f - d;
      if (rInner > 0) {
        float inner = d - (rInner - 0.5f);
        if (inner < cov) cov = inner;
      }
      if (cov <= 0) continue;
      plot(x, y, c, uint8_t((cov >= 1 ? 1.f : cov) * alpha));
    }
}

void fillCircle(float cx, float cy, float r, uint16_t c, uint8_t alpha) { disc(cx, cy, r, 0, c, alpha); }
void ring(float cx, float cy, float r, float width, uint16_t c, uint8_t alpha) { disc(cx, cy, r, r - width, c, alpha); }

void drawMask(const Mask &m, int x, int y, uint16_t c, uint8_t alpha) {
  for (int j = 0; j < m.h; ++j)
    for (int i = 0; i < m.w; ++i) {
      int idx = j * m.w + i;
      uint8_t byte = m.data[idx >> 1];
      uint8_t v = (idx & 1) ? (byte & 15) : (byte >> 4);
      if (v) plot(x + i, y + j, c, uint8_t(v * 17 * alpha / 255));
    }
}

static const Glyph *find(const Font &f, char ch) {
  uint8_t code = uint8_t(ch);
  if (code >= 32 && code - 32 < f.count && f.glyphs[code - 32].code == code) return &f.glyphs[code - 32];  // ASCII fonts
  for (uint16_t i = 0; i < f.count; ++i)
    if (f.glyphs[i].code == uint8_t(ch)) return &f.glyphs[i];
  return nullptr;
}

int textN(const Font &f, int x, int y, const char *s, int len, uint16_t c, uint8_t alpha) {
  int start = x;
  for (int n = 0; n < len && s[n]; ++n) {
    const Glyph *g = find(f, s[n]);
    if (!g) g = find(f, '?');
    if (!g) continue;
    // Glyph bitmaps are positioned relative to the ascent line.
    int gx = x + g->x, gy = y - f.ascent + g->y;
    for (int j = 0; j < g->h; ++j)
      for (int i = 0; i < g->w; ++i) {
        int idx = j * g->w + i;
        uint8_t byte = f.bitmap[g->offset + (idx >> 1)];
        uint8_t v = (idx & 1) ? (byte & 15) : (byte >> 4);
        if (v) plot(gx + i, gy + j, c, uint8_t(v * 17 * alpha / 255));
      }
    x += g->adv;
  }
  return x - start;
}

int text(const Font &f, int x, int y, const char *s, uint16_t c, uint8_t alpha) {
  return textN(f, x, y, s, int(strlen(s)), c, alpha);
}

int textWidth(const Font &f, const char *s, int len) {
  if (len < 0) len = int(strlen(s));
  int w = 0;
  for (int n = 0; n < len && s[n]; ++n) {
    const Glyph *g = find(f, s[n]);
    if (!g) g = find(f, '?');
    if (g) w += g->adv;
  }
  return w;
}

int textCentered(const Font &f, int cx, int y, const char *s, uint16_t c, uint8_t alpha) {
  int w = textWidth(f, s);
  text(f, cx - w / 2, y, s, c, alpha);
  return w;
}

int textRight(const Font &f, int right, int y, const char *s, uint16_t c, uint8_t alpha) {
  int w = textWidth(f, s);
  text(f, right - w, y, s, c, alpha);
  return w;
}

void textFit(const Font &f, int x, int y, int maxW, const char *s, uint16_t c, uint8_t alpha) {
  int len = int(strlen(s));
  if (textWidth(f, s, len) <= maxW) { textN(f, x, y, s, len, c, alpha); return; }
  int dots = textWidth(f, "...");
  while (len > 0 && textWidth(f, s, len) + dots > maxW) --len;
  while (len > 0 && s[len - 1] == ' ') --len;
  int w = textN(f, x, y, s, len, c, alpha);
  text(f, x + w, y, "...", c, alpha);
}

int wrap(const Font &f, const char *s, int maxW, int *starts, int *lens, int maxLines) {
  int lines = 0, i = 0, n = int(strlen(s));
  while (i < n) {
    while (i < n && s[i] == ' ') ++i;
    if (i >= n) break;
    int lineStart = i, lastBreak = -1, j = i;
    while (j < n && s[j] != '\n') {
      if (s[j] == ' ') lastBreak = j;
      if (textWidth(f, s + lineStart, j - lineStart + 1) > maxW) break;
      ++j;
    }
    int end;
    if (j >= n || s[j] == '\n') end = j;
    else if (lastBreak > lineStart) end = lastBreak;
    else end = j > lineStart ? j : lineStart + 1;  // a single word wider than the line: hard break
    if (lines < maxLines) { starts[lines] = lineStart; lens[lines] = end - lineStart; }
    ++lines;
    i = (end < n && s[end] == '\n') ? end + 1 : end;
  }
  return lines;
}

float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
float easeOut(float t) { t = clamp01(t); float u = 1 - t; return 1 - u * u * u; }
float easeInOut(float t) {
  t = clamp01(t);
  return t < .5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2;
}

}  // namespace gfx
