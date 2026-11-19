#pragma once
// Portable 128x160 RGB565 framebuffer renderer: anti-aliased text, masks and rounded shapes.
// No Arduino dependencies, so the host preview (preview/preview.cpp) renders identical frames.
#include <stdint.h>
#include "ui_assets.h"

namespace gfx {

constexpr int W = 128, H = 160;
extern uint16_t *const fb;  // W * H pixels, RGB565 big-endian; on the heap so it stays out of static DRAM (the CAM needs that room)

constexpr uint16_t rgb(uint32_t hex) {
  return uint16_t(((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex >> 3) & 0x001F));
}
// Remi "Sip Happy" palette (DESIGN.md in the Remi repo).
constexpr uint16_t LEMON = rgb(0xF8E45C), COBALT = rgb(0x1734BE), PINK = rgb(0xED2B8C), PINK_TEXT = rgb(0xC2146C),
                   INK = rgb(0x0E1440), WHITE = rgb(0xFFFFFF), PALE_LEMON = rgb(0xFCF4C4), PALE_PINK = rgb(0xFDE7F1),
                   PAPER = rgb(0xFFFDF4);
constexpr uint16_t BG = rgb(REMI_BG);  // device background (a deeper gold reads better on the TFT than LEMON)

uint16_t blend(uint16_t dst, uint16_t src, uint8_t alpha);  // alpha 0..255

// Everything below draws relative to the origin (used to slide pages) and clips to the screen.
void setOrigin(int x, int y);
void fill(uint16_t c);
void fillRect(int x, int y, int w, int h, uint16_t c, uint8_t alpha = 255);
void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c, uint8_t alpha = 255);  // anti-aliased corners
void fillCircle(float cx, float cy, float r, uint16_t c, uint8_t alpha = 255);
void ring(float cx, float cy, float r, float width, uint16_t c, uint8_t alpha = 255);
void drawMask(const Mask &m, int x, int y, uint16_t c, uint8_t alpha = 255);

// Text: y is the baseline. Returns the advance width.
int text(const Font &f, int x, int y, const char *s, uint16_t c, uint8_t alpha = 255);
int textWidth(const Font &f, const char *s, int len = -1);
int textCentered(const Font &f, int cx, int y, const char *s, uint16_t c, uint8_t alpha = 255);
int textRight(const Font &f, int right, int y, const char *s, uint16_t c, uint8_t alpha = 255);
// Draws s cut to maxW with an ellipsis when needed.
void textFit(const Font &f, int x, int y, int maxW, const char *s, uint16_t c, uint8_t alpha = 255);
// Greedy word wrap: fills starts/lens with up to maxLines lines, returns the number of lines needed in total.
int wrap(const Font &f, const char *s, int maxW, int *starts, int *lens, int maxLines);
int textN(const Font &f, int x, int y, const char *s, int len, uint16_t c, uint8_t alpha = 255);

// Animation helpers.
float easeOut(float t);    // cubic ease-out
float easeInOut(float t);  // cubic ease-in-out
float clamp01(float t);

}  // namespace gfx
