#pragma once
// Data shared between the UI and the backend task, plus the hooks each platform implements
// (the ESP32 sketch, or the host preview).
#include <stdint.h>
#include <time.h>

constexpr int MAX_SHOP = 12, MAX_SUGGESTIONS = 6, MAX_STEPS = 16;

struct ShopItem {
  char id[40], foodId[40], name[32], amount[16], unit[12];
  bool checked;
};
struct Suggestion {
  char id[40], name[56];
  int minutes, have, need;
};
struct RecipeStep {
  char text[360];
  int minutes;
};

enum class Link : uint8_t { Offline, Connecting, Online, AuthFailed };
enum class Job : uint8_t { Idle, Working, Done, Failed };

struct AppData {
  Link link = Link::Connecting;
  char firstName[24] = "";
  // Home
  int foods = -1, useFirst = -1;  // -1 = not loaded yet
  uint32_t syncedAtMs = 0;
  // Just bought (the household's active shopping list, NEEDED items)
  ShopItem shop[MAX_SHOP];
  int shopCount = 0;
  bool shopLoaded = false;
  Job putAway = Job::Idle;
  int putAwayCount = 0;
  // Cook
  Suggestion suggestions[MAX_SUGGESTIONS];
  int suggestionCount = 0;
  bool suggestionsLoaded = false;
  char recipeId[40] = "", recipeName[56] = "";
  int recipeMinutes = 0, stepCount = 0;
  RecipeStep steps[MAX_STEPS];
  Job recipe = Job::Idle;
};

extern AppData data;

// ---- platform hooks ----
uint32_t nowMs();
bool localTime(struct tm *out);  // false until the clock is synced
void lockData();
void unlockData();

enum class Request : uint8_t { Refresh, PutAway, LoadRecipe };
void backendRequest(Request r, int arg = 0);  // non-blocking; results land in `data`

enum class Sfx : uint8_t {
  Boot, Whoosh, Land, Hello, Tick, Page, Select, Back, Check, Uncheck, Success, Error, TimerStart, Alarm, Voice, Edge,
  Music, FlyIn, Boing, Pop0, Pop1, Pop2, Pop3, Scribble, Chime, Sparkle, Hush  // boot sequence
};
void playSfx(Sfx s);
