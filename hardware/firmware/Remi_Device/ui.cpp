#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "app_data.h"
#include "gfx.h"
#include "boot_frames.h"

using namespace gfx;

namespace ui {
namespace {

// ---------------- timing constants (ms) ----------------
constexpr uint32_t BOOT_MS = uint32_t(BOOT_FRAME_COUNT) * 1000 / BOOT_FPS;  // about 6 s, sound cues included
constexpr uint32_t UI_APPEAR_MS = 380;                  // Home content fades/rises in over the docked face
constexpr uint32_t UI_APPEAR_AT = BOOT_MS - 140;
constexpr uint32_t PAGE_SLIDE_MS = 300, MODE_FADE_MS = 200, LONG_TOAST_MS = 2600;

enum Page { HOME, BOUGHT, COOK, TALK, PAGE_COUNT };
enum class Mode { Top, List, Working, Done, Recipe, Steps, Finished, Speaking };

struct Tween {
  float from = 0, to = 0;
  uint32_t start = 0, dur = 1;
  float value(uint32_t now) const { return from + (to - from) * easeOut(float(now - start) / dur); }
  void retarget(float target, uint32_t now, uint32_t d) { from = value(now); to = target; start = now; dur = d; }
};

uint32_t bootMs = 0;
bool intro = true;
uint16_t bootFrame = 0;  // next frame to decode: frames only patch what changed, so they're applied in order
size_t bootCue = 0;
bool colorTest = false;

int page = HOME;
Tween pagePos;
Mode mode = Mode::Top;
uint32_t modeAt = 0;
int sel = 0;
float selY = 0, scrollY = 0;
uint32_t lastFrameMs = 0;

int step = 0, stepPart = 0;  // cooking: step index and text page within the step
int cachedStep = -1, stepLines = 0, stepStarts[28], stepLens[28];
constexpr int STEP_LINES_PER_PAGE = 5;

uint32_t timerEnd = 0, timerTotal = 0;
bool timerOn = false, alarm = false;
uint32_t alarmNext = 0, alarmUntil = 0;

uint32_t nextBlink = 2500, blinkUntil = 0;
uint32_t speakUntil = 0;
int talkLine = 0;
uint32_t refreshAsked = 0;
char toast[48] = "";
uint32_t toastStart = 0, toastUntil = 0;
uint32_t doneAt = 0;

const char *const TALK_LINES[] = {"Hi! I'm Remi, your kitchen buddy.", "Soon you can just ask me what to cook.",
                                  "My microphone is on the way!"};
const char *const DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Backend unit enums -> short labels ("PIECE" -> "pcs"); anything else is shown as-is.
const char *unitShort(const char *u) {
  static const char *const map[][2] = {{"PIECE", "pcs"}, {"GRAM", "g"}, {"KILOGRAM", "kg"}, {"LITER", "L"},
                                       {"MILLILITER", "ml"}, {"CUP", "cup"}, {"TABLESPOON", "tbsp"}, {"TEASPOON", "tsp"}};
  for (auto &m : map)
    if (!strcmp(u, m[0])) return m[1];
  return u;
}

void setMode(Mode m) {
  mode = m;
  modeAt = nowMs();
}

void showToast(const char *msg) {
  snprintf(toast, sizeof toast, "%s", msg);
  toastStart = nowMs();
  toastUntil = toastStart + LONG_TOAST_MS;
}

int checkedCount() {
  int n = 0;
  for (int i = 0; i < data.shopCount; ++i) n += data.shop[i].checked;
  return n;
}

// ---------------- intro ----------------
void decodeBootFrame(uint16_t frame) {
  uint32_t o = BOOT_OFFSETS[frame], end = BOOT_OFFSETS[frame + 1], px = 0;
  while (o + 1 < end && px < uint32_t(W * H)) {
    uint8_t run = BOOT_DATA[o], idx = BOOT_DATA[o + 1];
    if (px + run > uint32_t(W * H)) return;  // corrupt data: stop rather than overrun
    if (idx != BOOT_SKIP) {
      uint16_t c = BOOT_PALETTE[idx];
      for (uint8_t i = 0; i < run; ++i) fb[px + i] = c;
    }
    px += run;
    o += 2;
  }
}

// Swatches for choosing the background on a real panel (Serial key 'c'). TN panels wash out light yellows.
void drawColorTest() {
  static const uint32_t SW[] = {0xF8E45C, 0xF5D33F, 0xF2C230, 0xEDB61E, 0xE8A916, 0xFFD23F};
  static const char *const NAMES[] = {"F8E45C app", "F5D33F", "F2C230 now", "EDB61E", "E8A916", "FFD23F"};
  for (int i = 0; i < 6; ++i) {
    int y = i * 26 + 3;
    fillRect(0, y - 3, W, 26, rgb(SW[i]));
    fillRoundRect(6, y + 2, 20, 16, 5, WHITE);
    text(FONT_LABEL, 34, y + 14, NAMES[i], COBALT);
    drawMask(AVATAR_OPEN, 98, y - 1, COBALT);
  }
}

// ---------------- shared chrome ----------------
uint8_t A(uint8_t base, uint8_t mul) { return uint8_t(base * mul / 255); }

void drawPill(int x, int y, int w, int h, uint16_t bg, const char *label, uint16_t fg, uint8_t a, const Font &f = FONT_LABEL) {
  fillRoundRect(x, y, w, h, h / 2, bg, a);
  textCentered(f, x + w / 2, y + h / 2 + f.ascent / 2 - 2, label, fg, a);
}

void drawCard(int x, int y, int w, int h, uint8_t a) {
  fillRoundRect(x, y + 2, w, h, 12, INK, A(18, a));  // soft shadow
  fillRoundRect(x, y, w, h, 12, WHITE, a);
}

const char *pageTitle(int p, char *buf, size_t n) {
  if (p == HOME) {
    struct tm t;
    const char *g = "Hello";
    if (localTime(&t)) g = t.tm_hour < 5 ? "Good night" : t.tm_hour < 12 ? "Good morning" : t.tm_hour < 18 ? "Good day" : "Good evening";
    snprintf(buf, n, "%s", g);
    return buf;
  }
  if (p == BOUGHT) return mode == Mode::Top || page != BOUGHT ? "Just bought" : "Shopping list";
  if (p == COOK) {
    if (page != COOK || mode == Mode::Top) return "Cook";
    return mode == Mode::List ? "Ideas for you" : "Cooking";
  }
  return "Talk";
}

void drawHeader(uint32_t now, uint8_t a) {
  setOrigin(0, 0);
  int bounce = (now < speakUntil) ? int(2.5f * fabsf(sinf((now - (speakUntil - 2400)) / 95.f))) : 0;
  const Mask &face = now < blinkUntil ? AVATAR_BLINK : AVATAR_OPEN;
  drawMask(face, AVATAR_X, AVATAR_Y - bounce, COBALT, 255);  // the face is already on screen from the intro
  // Titles cross-fade while pages slide.
  float pos = pagePos.value(now);
  char buf[32];
  for (int p = 0; p < PAGE_COUNT; ++p) {
    float d = fabsf(pos - p);
    if (d >= 1) continue;
    uint8_t ta = uint8_t((1 - d) * a);
    textFit(FONT_HEAD, 33 - int((pos - p) * 10), 17, (timerOn || alarm) ? 56 : 84, pageTitle(p, buf, sizeof buf), INK, ta);
  }
  // Right side: running timer, else connection state.
  if (timerOn || alarm) {
    char t[8];
    int left = alarm ? 0 : int((timerEnd - now + 999) / 1000);
    snprintf(t, sizeof t, "%d:%02d", left / 60, left % 60);
    bool flash = alarm && ((now / 300) & 1);
    fillRoundRect(92, 6, 32, 15, 7, flash ? COBALT : PINK, a);
    textCentered(FONT_SMALL, 108, 17, t, WHITE, a);
  } else {
    // Status light: steady cobalt online, breathing while connecting, pink when offline or signed out.
    uint16_t c = data.link == Link::Online ? COBALT : PINK;
    uint8_t la = data.link == Link::Connecting ? uint8_t(140 + 100 * sinf(now / 260.f)) : 255;
    fillCircle(122, 13, 2.4f, data.link == Link::Connecting ? COBALT : c, A(la, a));
  }
}

void drawFooter(uint8_t a) {
  setOrigin(0, 0);
  if (mode != Mode::Top) {
    textCentered(FONT_SMALL, 64, 156, "Hold the knob to go back", INK, A(120, a));
    return;
  }
  float pos = pagePos.value(nowMs());
  int x = 64 - (PAGE_COUNT * 9 + 6) / 2;
  for (int p = 0; p < PAGE_COUNT; ++p) {
    float act = 1 - fminf(1, fabsf(pos - p));  // 1 = this page is showing
    int w = 4 + int(8 * act + .5f);
    fillRoundRect(x, 151, w, 4, 2, COBALT, A(uint8_t(80 + 175 * act), a));
    x += w + 5;
  }
}

// ---------------- pages ----------------
void homePage(uint8_t a, uint32_t now) {
  struct tm t;
  bool clock = localTime(&t);
  char buf[32];
  if (clock) snprintf(buf, sizeof buf, "%02d:%02d", t.tm_hour, t.tm_min);
  else snprintf(buf, sizeof buf, "--:--");
  textCentered(FONT_TIME, 64, 64, buf, COBALT, a);
  if (clock) snprintf(buf, sizeof buf, "%s %d %s", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon]);
  else snprintf(buf, sizeof buf, "Setting the clock...");
  textCentered(FONT_LABEL, 64, 80, buf, INK, A(200, a));

  drawCard(8, 87, 112, 59, a);
  drawMask(ICON_HOME, 16, 93, COBALT, a);
  text(FONT_SMALL, 33, 103, "In your kitchen", INK, A(170, a));
  bool syncing = refreshAsked && now - refreshAsked < 4000 && data.syncedAtMs < refreshAsked;
  if (data.foods >= 0) {
    snprintf(buf, sizeof buf, "%d", data.foods);
    int w = text(FONT_FIGURE, 16, 126, buf, COBALT, a);
    text(FONT_LABEL, 19 + w, 126, data.foods == 1 ? "food" : "foods", INK, a);
    if (syncing) textRight(FONT_SMALL, 112, 126, "Syncing...", INK, A(140, a));
    // Freshness pill on its own row so it never covers the count.
    const char *label = buf;
    bool soon = data.useFirst > 0;
    if (soon) snprintf(buf, sizeof buf, "%d to use first", data.useFirst);
    else label = "All fresh";
    int pw = textWidth(FONT_SMALL, label) + 20;
    fillRoundRect(15, 130, pw, 12, 6, soon ? PALE_PINK : PALE_LEMON, a);
    drawMask(soon ? ICON_CLOCK : ICON_LEAF, 19, 131, soon ? PINK_TEXT : COBALT, a);
    text(FONT_SMALL, 31, 140, label, soon ? PINK_TEXT : COBALT, a);
  } else {
    const char *msg = data.link == Link::Offline ? "Offline" : data.link == Link::AuthFailed ? "Can't sign in" : "Connecting...";
    text(FONT_FIGURE, 16, 126, "--", COBALT, A(110, a));
    text(FONT_SMALL, 16, 140, msg, data.link == Link::Connecting ? INK : PINK_TEXT, A(190, a));
  }
}

void boughtPage(uint8_t a, uint32_t now) {
  char buf[40];
  if (page != BOUGHT || mode == Mode::Top) {
    drawMask(ICON_BAG_L, 49, 30, COBALT, a);
    textCentered(FONT_TITLE, 64, 82, "Just shopped?", INK, a);
    if (!data.shopLoaded) {
      textCentered(FONT_BODY, 64, 99, data.link == Link::Online ? "Loading your list..." : "Waiting for Remi...", INK, A(170, a));
      return;
    }
    if (data.shopCount == 0) {
      textCentered(FONT_BODY, 64, 99, "Your list is empty.", INK, A(190, a));
      textCentered(FONT_SMALL, 64, 113, "Add items in the Remi app", INK, A(140, a));
      return;
    }
    snprintf(buf, sizeof buf, "%d item%s on your list", data.shopCount, data.shopCount == 1 ? "" : "s");
    textCentered(FONT_BODY, 64, 99, buf, INK, A(190, a));
    drawPill(18, 114, 92, 20, COBALT, "Press to put away", WHITE, a, FONT_SMALL);
    return;
  }
  if (mode == Mode::Working) {
    textCentered(FONT_TITLE, 64, 76, "Putting away", INK, a);
    for (int i = 0; i < 3; ++i) fillCircle(52 + i * 12, 94, 3.2f, COBALT, A(uint8_t(90 + 165 * (0.5f + 0.5f * sinf(now / 160.f - i))), a));
    return;
  }
  if (mode == Mode::Done) {
    float p = easeOut((now - doneAt) / 420.f);
    fillCircle(64, 66, 22 * p, COBALT, a);
    drawMask(ICON_CHECK, 60, 62, WHITE, uint8_t(255 * p * a / 255));
    for (int i = 0; i < 8; ++i) {  // little burst
      float ang = i * 0.785f, r = 26 + 12 * p;
      fillCircle(64 + cosf(ang) * r, 66 + sinf(ang) * r, 2.2f * (1 - p), PINK, a);
    }
    snprintf(buf, sizeof buf, "%d food%s added", data.putAwayCount, data.putAwayCount == 1 ? "" : "s");
    textCentered(FONT_TITLE, 64, 112, buf, INK, a);
    textCentered(FONT_SMALL, 64, 126, "Your kitchen is up to date", INK, A(150, a));
    return;
  }
  // List: items + "put away" action row.
  drawCard(6, 26, 116, 120, a);
  const int rowH = 23, top = 30, visible = 5;
  int rows = data.shopCount + 1;
  float target = fminf(fmaxf(0, sel - (visible - 1) / 2.f), fmaxf(0, rows - visible));
  float dt = fminf(0.1f, (now - lastFrameMs) / 1000.f);
  float k = 1 - expf(-dt / 0.05f);
  scrollY += (target - scrollY) * k;
  selY += (sel - selY) * k;
  int hl = top + int(roundf((selY - scrollY) * rowH));
  if (sel < data.shopCount) fillRoundRect(10, hl, 108, 22, 7, PALE_LEMON, a);
  for (int i = 0; i < rows; ++i) {
    int y = top + int(roundf((i - scrollY) * rowH));
    if (y < 26 || y > 26 + 120 - rowH) continue;
    if (i == data.shopCount) {
      int n = checkedCount();
      if (n) snprintf(buf, sizeof buf, "Put away %d", n);
      bool on = n > 0, here = sel == i;
      if (here) fillRoundRect(9, y, 110, 22, 11, COBALT, A(50, a));
      drawPill(12, y + 2, 104, 18, on ? COBALT : PALE_LEMON, on ? buf : "Tick what you bought", on ? WHITE : INK,
               A(here || !on ? 255 : 200, a), FONT_SMALL);
      continue;
    }
    ShopItem &it = data.shop[i];
    if (it.checked) {
      fillCircle(20, y + 11, 5.5f, COBALT, a);
      drawMask(ICON_CHECK, 16, y + 7, WHITE, a);
    } else {
      ring(20, y + 11, 5.5f, 1.3f, INK, A(110, a));
    }
    snprintf(buf, sizeof buf, "%s %s", it.amount, unitShort(it.unit));
    textFit(FONT_LABEL, 31, y + 11, 84, it.name, INK, a);
    text(FONT_SMALL, 31, y + 20, buf, INK, A(140, a));
  }
}

void ensureStepWrap() {
  if (cachedStep == step) return;
  stepLines = wrap(FONT_BODY, data.steps[step].text, 112, stepStarts, stepLens, 28);
  if (stepLines > 28) stepLines = 28;
  cachedStep = step;
}

int stepParts() { return (stepLines + STEP_LINES_PER_PAGE - 1) / STEP_LINES_PER_PAGE; }

void cookPage(uint8_t a, uint32_t now) {
  char buf[48];
  int st[3], ln[3];
  if (page != COOK || mode == Mode::Top) {
    drawMask(ICON_POT_L, 49, 28, COBALT, a);
    if (!data.suggestionsLoaded) {
      textCentered(FONT_TITLE, 64, 80, "What's for dinner?", INK, a);
      textCentered(FONT_BODY, 64, 97, data.link == Link::Online ? "Finding ideas..." : "Waiting for Remi...", INK, A(170, a));
      return;
    }
    if (data.suggestionCount == 0) {
      textCentered(FONT_TITLE, 64, 80, "No ideas yet", INK, a);
      textCentered(FONT_SMALL, 64, 96, "Add food to your kitchen first", INK, A(150, a));
      return;
    }
    Suggestion &s = data.suggestions[0];
    textCentered(FONT_SMALL, 64, 72, "COOK NOW", PINK_TEXT, a);
    int n = wrap(FONT_TITLE, s.name, 112, st, ln, 2);
    for (int i = 0; i < n && i < 2; ++i) {
      char line[64];
      snprintf(line, sizeof line, "%.*s%s", ln[i], s.name + st[i], (i == 1 && n > 2) ? "..." : "");
      textCentered(FONT_TITLE, 64, 88 + i * 15, line, INK, a);
    }
    int y = n > 1 ? 118 : 104;
    snprintf(buf, sizeof buf, "%d min \x7f %d of %d ready", s.minutes, s.have, s.need);
    textCentered(FONT_SMALL, 64, y, buf, INK, A(150, a));
    drawPill(22, 124, 84, 20, COBALT, "More ideas", WHITE, a, FONT_SMALL);
    return;
  }
  if (mode == Mode::List) {
    drawCard(6, 26, 116, 120, a);
    const int rowH = 23, top = 30, visible = 5;
    float target = fminf(fmaxf(0, sel - 2.f), fmaxf(0, data.suggestionCount - visible));
    float dt = fminf(0.1f, (now - lastFrameMs) / 1000.f), k = 1 - expf(-dt / 0.05f);
    scrollY += (target - scrollY) * k;
    selY += (sel - selY) * k;
    fillRoundRect(10, top + int(roundf((selY - scrollY) * rowH)), 108, 22, 7, PALE_LEMON, a);
    for (int i = 0; i < data.suggestionCount; ++i) {
      int y = top + int(roundf((i - scrollY) * rowH));
      if (y < 26 || y > 146 - rowH) continue;
      Suggestion &s = data.suggestions[i];
      textFit(FONT_LABEL, 15, y + 11, 98, s.name, INK, a);
      snprintf(buf, sizeof buf, "%d min \x7f %d/%d ready", s.minutes, s.have, s.need);
      text(FONT_SMALL, 15, y + 20, buf, s.have >= s.need ? COBALT : INK, A(150, a));
    }
    return;
  }
  if (mode == Mode::Recipe) {
    if (data.recipe != Job::Done) {
      textCentered(FONT_TITLE, 64, 80, "Opening recipe", INK, a);
      for (int i = 0; i < 3; ++i) fillCircle(52 + i * 12, 96, 3.2f, COBALT, A(uint8_t(90 + 165 * (0.5f + 0.5f * sinf(now / 160.f - i))), a));
      return;
    }
    drawMask(ICON_POT_L, 49, 28, COBALT, a);
    int n = wrap(FONT_TITLE, data.recipeName, 112, st, ln, 3);
    for (int i = 0; i < n && i < 3; ++i) {
      char line[64];
      snprintf(line, sizeof line, "%.*s", ln[i], data.recipeName + st[i]);
      textCentered(FONT_TITLE, 64, 78 + i * 15, line, INK, a);
    }
    snprintf(buf, sizeof buf, "%d min \x7f %d step%s", data.recipeMinutes, data.stepCount, data.stepCount == 1 ? "" : "s");
    textCentered(FONT_SMALL, 64, 80 + (n < 3 ? n : 3) * 15, buf, INK, A(150, a));
    drawPill(22, 124, 84, 20, COBALT, "Start cooking", WHITE, a, FONT_SMALL);
    return;
  }
  if (mode == Mode::Finished) {
    float p = easeOut((now - modeAt) / 500.f);
    for (int i = 0; i < 10; ++i) {
      float ang = i * 0.628f + now / 900.f, r = 18 + 16 * p;
      fillCircle(64 + cosf(ang) * r, 62 + sinf(ang) * r, 2.5f, i % 2 ? PINK : COBALT, A(uint8_t(255 * p), a));
    }
    drawMask(ICON_POT_L, 49, 47, COBALT, a);
    textCentered(FONT_TITLE, 64, 112, "Enjoy your meal!", INK, a);
    textCentered(FONT_SMALL, 64, 127, "Press to finish", INK, A(150, a));
    return;
  }
  // Steps: segmented progress, wrapped instruction, timer pill.
  ensureStepWrap();
  int seg = data.stepCount > 0 ? data.stepCount : 1, gap = 2, x = 8, total = 112 - (seg - 1) * gap;
  for (int i = 0; i < seg; ++i) {
    int w = total * (i + 1) / seg - total * i / seg;
    fillRoundRect(x, 27, w, 3, 1, i <= step ? PINK : INK, A(i <= step ? 255 : 45, a));
    x += w + gap;
  }
  int parts = stepParts();
  snprintf(buf, sizeof buf, "Step %d of %d", step + 1, data.stepCount);
  text(FONT_LABEL, 8, 45, buf, COBALT, a);
  if (parts > 1) {
    snprintf(buf, sizeof buf, "%d/%d", stepPart + 1, parts);
    textRight(FONT_SMALL, 120, 45, buf, INK, A(130, a));
  }
  for (int i = 0; i < STEP_LINES_PER_PAGE; ++i) {
    int li = stepPart * STEP_LINES_PER_PAGE + i;
    if (li >= stepLines) break;
    textN(FONT_BODY, 8, 62 + i * 13, data.steps[step].text + stepStarts[li], stepLens[li], INK, a);
  }
  int mins = data.steps[step].minutes;
  if (timerOn || alarm) {
    drawPill(14, 126, 100, 18, PALE_PINK, alarm ? "Time's up! Press" : "Press to stop timer", PINK_TEXT, a, FONT_SMALL);
  } else if (mins > 0) {
    snprintf(buf, sizeof buf, "Press: %d min timer", mins);
    drawPill(14, 126, 100, 18, COBALT, buf, WHITE, a, FONT_SMALL);
  } else {
    textCentered(FONT_SMALL, 64, 138, step + 1 < data.stepCount ? "Turn for the next step" : "Turn to finish", INK, A(130, a));
  }
}

void talkPage(uint8_t a, uint32_t now) {
  bool speaking = page == TALK && now < speakUntil;
  float pulse = speaking ? 0.5f + 0.5f * sinf(now / 110.f) : 0;
  if (speaking) ring(64, 66, 30 + 6 * pulse, 2, PINK, A(uint8_t(120 * (1 - pulse)), a));
  fillCircle(64, 66, 27, WHITE, a);
  drawMask(ICON_MIC_L, 47, 49, speaking ? PINK : COBALT, a);
  if (speaking) {  // speech bubble under the header, pointing at the face
    const char *msg = TALK_LINES[talkLine];
    int st[3], ln[3];
    uint8_t ba = A(uint8_t(255 * easeOut((now - (speakUntil - 2400)) / 180.f)), a);
    int n = wrap(FONT_SMALL, msg, 80, st, ln, 3);
    if (n > 3) n = 3;
    int h = 8 + n * 10;
    fillRoundRect(28, 102, 96, h, 8, COBALT, ba);
    for (int i = 0; i < n; ++i) textN(FONT_SMALL, 34, 112 + i * 10, msg + st[i], ln[i], WHITE, ba);
    return;
  }
  textCentered(FONT_TITLE, 64, 112, "Talk to Remi", INK, a);
  textCentered(FONT_SMALL, 64, 125, "Microphone coming soon", INK, A(150, a));
  textCentered(FONT_SMALL, 64, 140, "Press to say hi", COBALT, a);
}

void drawPages(uint32_t now, uint8_t a, int rise) {
  float pos = pagePos.value(now);
  for (int p = 0; p < PAGE_COUNT; ++p) {
    float off = (p - pos) * W;
    if (fabsf(off) >= W) continue;
    uint8_t pa = a;
    int dy = rise;
    if (p == page && mode != Mode::Top) {  // sub-screens fade and rise in
      float m = easeOut(float(now - modeAt) / MODE_FADE_MS);
      pa = A(uint8_t(255 * m), a);
      dy += int(5 * (1 - m));
    }
    setOrigin(int(roundf(off)), dy);
    if (p == HOME) homePage(pa, now);
    else if (p == BOUGHT) boughtPage(pa, now);
    else if (p == COOK) cookPage(pa, now);
    else talkPage(pa, now);
  }
  setOrigin(0, 0);
}

void tickTimers(uint32_t now) {
  if (timerOn && int32_t(now - timerEnd) >= 0) {
    timerOn = false;
    alarm = true;
    alarmNext = now;
    alarmUntil = now + 60000;
  }
  if (alarm) {
    if (int32_t(now - alarmNext) >= 0) {
      playSfx(Sfx::Alarm);
      alarmNext = now + 1500;
    }
    if (int32_t(now - alarmUntil) >= 0) alarm = false;
  }
  if (now >= nextBlink) {
    blinkUntil = now + 130;
    nextBlink = now + 2600 + (now * 7919) % 2600;
  }
  // Background jobs finishing.
  if (page == BOUGHT && mode == Mode::Working) {
    lockData();
    Job j = data.putAway;
    unlockData();
    if (j == Job::Done) { setMode(Mode::Done); doneAt = now; playSfx(Sfx::Success); }
    if (j == Job::Failed) { setMode(Mode::List); playSfx(Sfx::Error); showToast("Couldn't reach Remi"); }
  }
  if (page == BOUGHT && mode == Mode::Done && now - doneAt > 2000) { setMode(Mode::Top); sel = 0; }
  if (page == COOK && mode == Mode::Recipe && data.recipe == Job::Failed) {
    setMode(Mode::List);
    playSfx(Sfx::Error);
    showToast("Couldn't open recipe");
  }
}

void drawToast(uint32_t now) {
  if (!toast[0] || int32_t(now - toastStart) < 0 || int32_t(now - toastUntil) >= 0) return;
  float in = easeOut((now - toastStart) / 200.f), out = clamp01((toastUntil - now) / 250.f);
  uint8_t a = uint8_t(255 * fminf(in, out));
  int w = textWidth(FONT_SMALL, toast) + 20;
  fillRoundRect(64 - w / 2, 128 + int(6 * (1 - in)), w, 18, 9, INK, a);
  textCentered(FONT_SMALL, 64, 140 + int(6 * (1 - in)), toast, WHITE, a);
}

void goPage(int p, uint32_t now) {
  if (p < 0 || p >= PAGE_COUNT) { playSfx(Sfx::Edge); return; }
  page = p;
  pagePos.retarget(float(p), now, PAGE_SLIDE_MS);
  playSfx(Sfx::Page);
}

}  // namespace

void begin() {
  bootMs = nowMs();
  intro = true;
  bootFrame = 0;
  bootCue = 0;
  page = HOME;
  pagePos = Tween();
  mode = Mode::Top;
}

bool inIntro() { return intro; }
void toggleColorTest() { colorTest = !colorTest; }
void notify(const char *msg) {
  showToast(msg);
  toastStart = nowMs() + 6500;  // after the boot
  toastUntil = toastStart + 8000;
}

void input(Input in) {
  uint32_t now = nowMs();
  if (intro) {  // any input skips the intro
    if (in == Input::Press || in == Input::LongPress) {
      bootMs = now - (UI_APPEAR_AT + UI_APPEAR_MS);
      playSfx(Sfx::Hush);  // cut the boot music short with the animation
    }
    return;
  }
  if (alarm && (in == Input::Press || in == Input::LongPress)) { alarm = false; playSfx(Sfx::Back); return; }
  int dir = in == Input::Right ? 1 : in == Input::Left ? -1 : 0;

  if (mode == Mode::Top) {
    if (dir) { goPage(page + dir, now); return; }
    if (in == Input::LongPress) { goPage(HOME, now); return; }
    if (page == HOME) { refreshAsked = now; backendRequest(Request::Refresh); playSfx(Sfx::Select); }
    if (page == BOUGHT) {
      if (data.shopCount > 0) { setMode(Mode::List); sel = 0; selY = scrollY = 0; playSfx(Sfx::Select); }
      else playSfx(Sfx::Edge);
    }
    if (page == COOK) {
      if (data.suggestionCount > 0) { setMode(Mode::List); sel = 0; selY = scrollY = 0; playSfx(Sfx::Select); }
      else playSfx(Sfx::Edge);
    }
    if (page == TALK) {
      talkLine = (now < speakUntil) ? (talkLine + 1) % 3 : talkLine;
      speakUntil = now + 2400;
      playSfx(Sfx::Voice);
    }
    return;
  }

  if (page == BOUGHT) {
    if (mode == Mode::List) {
      int rows = data.shopCount + 1;
      if (dir) {
        int n = sel + dir;
        if (n < 0 || n >= rows) playSfx(Sfx::Edge);
        else { sel = n; playSfx(Sfx::Tick); }
      } else if (in == Input::LongPress) {
        setMode(Mode::Top);
        playSfx(Sfx::Back);
      } else if (sel < data.shopCount) {
        lockData();
        data.shop[sel].checked = !data.shop[sel].checked;
        bool on = data.shop[sel].checked;
        unlockData();
        playSfx(on ? Sfx::Check : Sfx::Uncheck);
      } else if (checkedCount() > 0) {
        setMode(Mode::Working);
        backendRequest(Request::PutAway);
        playSfx(Sfx::Select);
      } else {
        playSfx(Sfx::Edge);
      }
    } else if (mode == Mode::Done && in != Input::Left && in != Input::Right) {
      setMode(Mode::Top);
    }
    return;
  }

  if (page == COOK) {
    if (mode == Mode::List) {
      if (dir) {
        int n = sel + dir;
        if (n < 0 || n >= data.suggestionCount) playSfx(Sfx::Edge);
        else { sel = n; playSfx(Sfx::Tick); }
      } else if (in == Input::LongPress) {
        setMode(Mode::Top);
        playSfx(Sfx::Back);
      } else {
        setMode(Mode::Recipe);
        backendRequest(Request::LoadRecipe, sel);
        playSfx(Sfx::Select);
      }
    } else if (mode == Mode::Recipe) {
      if (in == Input::LongPress) { setMode(Mode::List); playSfx(Sfx::Back); }
      else if (in == Input::Press && data.recipe == Job::Done && data.stepCount > 0) {
        step = stepPart = 0;
        cachedStep = -1;
        setMode(Mode::Steps);
        playSfx(Sfx::Select);
      }
    } else if (mode == Mode::Steps) {
      ensureStepWrap();
      if (in == Input::LongPress) { setMode(Mode::Recipe); playSfx(Sfx::Back); return; }
      if (dir > 0) {
        if (stepPart + 1 < stepParts()) ++stepPart;
        else if (step + 1 < data.stepCount) { ++step; stepPart = 0; modeAt = now; }
        else { setMode(Mode::Finished); playSfx(Sfx::Success); return; }
        playSfx(Sfx::Tick);
      } else if (dir < 0) {
        if (stepPart > 0) --stepPart;
        else if (step > 0) { --step; ensureStepWrap(); stepPart = stepParts() - 1; modeAt = now; }
        else { playSfx(Sfx::Edge); return; }
        playSfx(Sfx::Tick);
      } else {
        int mins = data.steps[step].minutes;
        if (timerOn) { timerOn = false; playSfx(Sfx::Back); }
        else if (mins > 0) {
          timerTotal = uint32_t(mins) * 60000;
          timerEnd = now + timerTotal;
          timerOn = true;
          playSfx(Sfx::TimerStart);
        } else playSfx(Sfx::Edge);
      }
    } else if (mode == Mode::Finished && !dir) {
      setMode(Mode::Top);
      playSfx(Sfx::Select);
    }
  }
}

void render() {
  uint32_t now = nowMs();
  uint32_t t = now - bootMs;
  if (intro) {
    uint32_t f = t * BOOT_FPS / 1000;
    if (f >= BOOT_FRAME_COUNT) f = BOOT_FRAME_COUNT - 1;
    while (bootCue < sizeof BOOT_CUES / sizeof BOOT_CUES[0] && BOOT_CUES[bootCue].frame <= f && t < UI_APPEAR_AT)
      playSfx(BOOT_CUES[bootCue++].sound);
    if (t < UI_APPEAR_AT) {
      while (bootFrame <= f) decodeBootFrame(bootFrame++);
      lastFrameMs = now;
      return;
    }
    // Hand-off: the last intro frame is the Home background with the docked face; fade the UI in over it.
    float p = easeOut(float(t - UI_APPEAR_AT) / UI_APPEAR_MS);
    fill(BG);
    drawHeader(now, uint8_t(255 * p));
    drawPages(now, uint8_t(255 * p), int(6 * (1 - p)));
    drawFooter(uint8_t(255 * p));
    if (p >= 1) intro = false;
    lastFrameMs = now;
    return;
  }
  tickTimers(now);
  if (colorTest) { drawColorTest(); lastFrameMs = now; return; }
  lockData();
  fill(BG);
  drawPages(now, 255, 0);
  drawHeader(now, 255);
  drawFooter(255);
  drawToast(now);
  unlockData();
  lastFrameMs = now;
}

}  // namespace ui
