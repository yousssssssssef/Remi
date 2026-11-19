#pragma once
// Data shared between the UI and the backend task, plus the hooks each platform implements
// (the ESP32 sketch, the host preview, or the browser simulator).
#include <stdint.h>
#include <time.h>

constexpr int MAX_SHOP = 12, MAX_SUGGESTIONS = 6, MAX_STEPS = 16, MAX_KITCHEN = 40, MAX_PLANNED = 6, MAX_PICKS = 6;

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
struct Ingredient {
  char foodId[40], amount[16], unit[12];
};
struct KitchenItem {             // one food in the kitchen, soonest expiry first
  char foodId[40], name[32], amount[16], unit[12];
  int daysLeft;                  // NO_EXPIRY when unknown
};
constexpr int NO_EXPIRY = 9999;
struct PlannedMeal {
  char recipeId[40], name[48];
  uint8_t mealType;              // 0 breakfast, 1 lunch, 2 dinner, 3 snack, 4 other
};
struct FoodPick {                // catalogue search result for "add to list"
  char foodId[40], name[32], unit[12];
};

enum class Link : uint8_t { Offline, Connecting, Online, AuthFailed, Setup };
enum class Job : uint8_t { Idle, Working, Done, Failed };

struct AppData {
  Link link = Link::Connecting;
  char firstName[24] = "";
  // Home
  int foods = -1, useFirst = -1;  // -1 = not loaded yet
  uint32_t syncedAtMs = 0;
  // Today: planned meals and nutrition against the profile goal
  PlannedMeal planned[MAX_PLANNED];
  int plannedCount = 0;
  bool todayLoaded = false;
  int kcal = -1, kcalTarget = 0, protein = -1, proteinTarget = 0, mealsLogged = 0;
  // Kitchen: inventory by food, soonest expiry first
  KitchenItem kitchen[MAX_KITCHEN];
  int kitchenCount = 0;
  bool kitchenLoaded = false;
  Job kitchenAction = Job::Idle;
  // Just bought (the household's active shopping list, NEEDED items)
  char shoppingListId[40] = "";
  ShopItem shop[MAX_SHOP];
  int shopCount = 0;
  bool shopLoaded = false;
  Job putAway = Job::Idle;
  int putAwayCount = 0;
  // Add to list: a search typed on the phone remote, picked with the knob
  char pickQuery[32] = "";
  FoodPick picks[MAX_PICKS];
  int pickCount = 0;
  Job search = Job::Idle, addItem = Job::Idle;
  // Cook
  Suggestion suggestions[MAX_SUGGESTIONS];
  int suggestionCount = 0;
  bool suggestionsLoaded = false;
  char recipeId[40] = "", recipeName[56] = "";  // the UI sets recipeId before asking for LoadRecipe
  int recipeMinutes = 0, stepCount = 0, ingredientCount = 0;
  RecipeStep steps[MAX_STEPS];
  Ingredient ingredients[24];
  Job recipe = Job::Idle, cooked = Job::Idle;
  // Connection: the phone/Mac remote and setup
  char remoteUrl[48] = "";   // e.g. http://remi.local, shown as a QR code on the Connect page
  char setupSsid[24] = "";   // set while the device runs its own setup Wi-Fi
  char setupPass[24] = "";
  char wifiName[33] = "";
  // Scan: the ESP32-CAM camera, or a photo sent from the phone remote
  bool hasCamera = false;
  uint8_t scanMode = 0;              // 0 barcode, 1 nutrition label, 2 shopping list, 3 receipt
  Job scan = Job::Idle, scanApply = Job::Idle;
  char scanTitle[48] = "";
  char scanLines[6][44] = {};
  int scanLineCount = 0;
  uint8_t scanAction = 0;            // 0 nothing to do, 1 add to kitchen, 2 add to shopping list
  // AI replies from the backend (POST /device/ask); the UI falls back to local answers
  char pairCode[8] = "";              // shown on Connect while waiting to be paired from the Remi app
  char askQuestion[64] = "";
  char askReply[300] = "";
  Job ask = Job::Idle;
};

extern AppData data;

// ---- platform hooks ----
uint32_t nowMs();
bool localTime(struct tm *out);  // false until the clock is synced
void lockData();
void unlockData();
// Live camera preview into a region of the framebuffer; false when there's no camera (draws a placeholder).
bool cameraPreview(uint16_t *fb, int x, int y, int w, int h);

enum class Request : uint8_t {
  Refresh,      // everything: kitchen, list, ideas, today
  PutAway,      // checked shopping items -> kitchen
  LoadRecipe,   // data.recipeId -> steps and ingredients
  CookedIt,     // log the loaded recipe as a meal: deducts its ingredients
  KitchenUse,   // arg = kitchen index: used it up
  KitchenToss,  // arg = kitchen index: threw it out
  SearchFoods,  // data.pickQuery -> data.picks
  AddToList,    // arg = picks index -> active shopping list
  Scan,         // capture with the camera and read it (data.scanMode)
  ScanApply,    // do data.scanAction with the scan's items
  Ask           // data.askQuestion -> data.askReply (AI, with household context)
};
void backendRequest(Request r, int arg = 0);  // non-blocking; results land in `data`

enum class Sfx : uint8_t {
  Boot, Whoosh, Land, Hello, Tick, Page, Select, Back, Check, Uncheck, Success, Error, TimerStart, Alarm, Voice, Edge,
  Music, FlyIn, Boing, Pop0, Pop1, Pop2, Pop3, Scribble, Chime, Sparkle, Hush,  // boot sequence
  TestTone  // 10 s steady 440 Hz for checking the amp wiring with a meter
};
void playSfx(Sfx s);
