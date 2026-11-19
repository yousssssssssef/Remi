// Wi-Fi, clock and Remi API sync. Runs as its own FreeRTOS task on core 0 so the UI never waits on the network.
// Without saved Wi-Fi it starts the "Remi-Setup" network so a phone can configure it (see remote.cpp).
#include <Arduino.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include "app_data.h"
#include "backend.h"
#include "camera.h"
#include "config.h"
#include "queries.h"
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID  // no secrets.h: everything comes from the setup page
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define REMI_API_URL ""
#define REMI_EMAIL ""
#define REMI_PASSWORD ""
#define REMI_TZ "EET-2EEST,M4.5.5/0,M10.5.4/24"
#endif

static const char SETUP_SSID[] = "Remi-Setup", SETUP_PASS[] = "cookwithremi";

DeviceConfig loadConfig() {
  Preferences p;
  p.begin("remi", true);
  DeviceConfig c{p.getString("ssid", WIFI_SSID), p.getString("wpass", WIFI_PASSWORD), p.getString("api", REMI_API_URL),
                 p.getString("email", REMI_EMAIL), p.getString("pass", REMI_PASSWORD), p.getString("tz", REMI_TZ)};
  p.end();
  return c;
}

void saveConfig(const DeviceConfig &c) {
  Preferences p;
  p.begin("remi", false);
  p.putString("ssid", c.ssid);
  if (c.wifiPass.length()) p.putString("wpass", c.wifiPass);
  p.putString("api", c.apiUrl);
  if (c.email != p.getString("email", "")) p.remove("refresh");  // a different account starts a new session
  p.putString("email", c.email);
  if (c.password.length()) { p.putString("pass", c.password); p.remove("refresh"); }
  p.putString("tz", c.tz);
  p.end();
}

namespace {

enum class Result { Ok, Auth, Net, Failed };
struct Cmd { Request r; int arg; };
QueueHandle_t commands;
Preferences prefs;
DeviceConfig cfg;
String accessToken, refreshToken, householdId;
uint32_t accessValidUntil = 0;
bool setupMode = false;

void setLink(Link l) { lockData(); data.link = l; unlockData(); }

// Copies UTF-8 text into dst as plain ASCII the device fonts can draw.
void ascii(char *dst, size_t n, const char *src) {
  size_t o = 0;
  for (const uint8_t *s = (const uint8_t *)src; *s && o + 1 < n;) {
    if (*s < 0x80) { dst[o++] = char(*s == '\t' || *s == '\r' ? ' ' : *s); ++s; continue; }
    const char *rep = "";
    if (s[0] == 0xE2 && s[1] == 0x80 && (s[2] == 0x93 || s[2] == 0x94)) rep = "-";
    else if (s[0] == 0xE2 && s[1] == 0x80 && (s[2] == 0x98 || s[2] == 0x99)) rep = "'";
    else if (s[0] == 0xE2 && s[1] == 0x80 && (s[2] == 0x9C || s[2] == 0x9D)) rep = "\"";
    else if (s[0] == 0xE2 && s[1] == 0x80 && s[2] == 0xA6) rep = "...";
    else if (s[0] == 0xC2 && s[1] == 0xBD) rep = "1/2";
    else if (s[0] == 0xC2 && s[1] == 0xBC) rep = "1/4";
    else if (s[0] == 0xC2 && s[1] == 0xB0) rep = " deg";
    else if (s[0] == 0xC3 && s[1] >= 0xA0 && s[1] <= 0xA5) rep = "a";
    else if (s[0] == 0xC3 && s[1] >= 0xA8 && s[1] <= 0xAB) rep = "e";
    for (; *rep && o + 1 < n; ++rep) dst[o++] = *rep;
    int len = (*s >= 0xF0) ? 4 : (*s >= 0xE0) ? 3 : (*s >= 0xC0) ? 2 : 1;  // skip the whole code point
    for (int i = 0; i < len && *s; ++i) ++s;
  }
  dst[o] = 0;
}

// "2.000" -> "2", "0.500" -> "0.5"
void amount(char *dst, size_t n, const char *decimal) {
  snprintf(dst, n, "%s", decimal ? decimal : "");
  char *dot = strchr(dst, '.');
  if (!dot) return;
  char *end = dst + strlen(dst) - 1;
  while (end > dot && *end == '0') *end-- = 0;
  if (end == dot) *end = 0;
}

bool clockSet() { return time(nullptr) > 1700000000; }

void today(char *buf, size_t n) {  // local date, YYYY-MM-DD
  time_t now = time(nullptr);
  strftime(buf, n, "%Y-%m-%d", localtime(&now));
}

// Whole days from today (local) to an ISO date or date-time; NO_EXPIRY if missing.
int daysUntil(const char *iso) {
  if (!iso || strlen(iso) < 10 || !clockSet()) return NO_EXPIRY;
  struct tm d = {};
  d.tm_year = atoi(iso) - 1900; d.tm_mon = atoi(iso + 5) - 1; d.tm_mday = atoi(iso + 8); d.tm_hour = 12;
  time_t now = time(nullptr);
  struct tm t = *localtime(&now);
  t.tm_hour = 12; t.tm_min = t.tm_sec = 0;
  return int(lround(difftime(mktime(&d), mktime(&t)) / 86400.0));
}

Result post(const char *query, JsonDocument &vars, JsonDocument &out, bool auth) {
  if (WiFi.status() != WL_CONNECTED || !cfg.apiUrl.length()) return Result::Net;
  JsonDocument body;
  body["query"] = query;
  body["variables"] = vars;
  String payload;
  serializeJson(body, payload);
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(cfg.apiUrl)) return Result::Net;
  http.addHeader("Content-Type", "application/json");
  http.setUserAgent("Remi-Device/1.1 (ESP32)");
  if (auth) {
    http.addHeader("Authorization", "Bearer " + accessToken);
    if (householdId.length()) http.addHeader("x-household-id", householdId);
  }
  int code = http.POST(payload);
  if (code <= 0) { http.end(); return Result::Net; }
  DeserializationError err = deserializeJson(out, http.getStream());
  http.end();
  if (code == 401) return Result::Auth;
  if (err) return Result::Failed;
  JsonArray errors = out["errors"];
  if (!errors.isNull() && errors.size()) {
    for (JsonObject e : errors)
      if (e["extensions"]["code"] == "UNAUTHENTICATED") return Result::Auth;
    Serial.printf("GraphQL error: %s\n", errors[0]["message"] | "?");
    return Result::Failed;
  }
  return Result::Ok;
}

// The device's REST endpoints sit beside /graphql on the same server (POST /device/ask, /device/scan).
String restUrl(const char *path) {
  String base = cfg.apiUrl;
  if (base.endsWith("/graphql")) base.remove(base.length() - 8);
  return base + path;
}

Result rest(const char *path, const char *contentType, const uint8_t *body, size_t len, JsonDocument &out) {
  if (WiFi.status() != WL_CONNECTED || !cfg.apiUrl.length()) return Result::Net;
  HTTPClient http;
  http.setTimeout(45000);  // reading a photo takes a few seconds
  if (!http.begin(restUrl(path))) return Result::Net;
  http.addHeader("Content-Type", contentType);
  http.addHeader("Authorization", "Bearer " + accessToken);
  if (householdId.length()) http.addHeader("x-household-id", householdId);
  http.setUserAgent("Remi-Device/1.2 (ESP32)");
  int code = http.POST(const_cast<uint8_t *>(body), len);
  if (code <= 0) { http.end(); return Result::Net; }
  DeserializationError err = deserializeJson(out, http.getStream());
  http.end();
  if (code == 401) return Result::Auth;
  if (code >= 400 || err) { Serial.printf("REST %s -> %d\n", path, code); return Result::Failed; }
  return Result::Ok;
}

bool storeTokens(JsonObject t) {
  if (t.isNull() || !t["accessToken"].is<const char *>()) return false;
  accessToken = t["accessToken"].as<const char *>();
  refreshToken = t["refreshToken"].as<const char *>();
  accessValidUntil = millis() + (uint32_t(t["expiresInSeconds"] | 900) - 60) * 1000;
  prefs.putString("refresh", refreshToken);  // rotation: the old token is already dead, save the new one at once
  return true;
}

bool signIn() {
  JsonDocument vars, out;
  if (refreshToken.length()) {
    vars["t"] = refreshToken;
    Result r = post(Q_REFRESH, vars, out, false);
    if (r == Result::Ok && storeTokens(out["data"]["refreshAccessToken"])) return true;
    if (r == Result::Net) return false;
    refreshToken = "";
    prefs.remove("refresh");
  }
  if (!cfg.email.length() || !cfg.password.length()) { setLink(Link::AuthFailed); return false; }
  vars.clear(); out.clear();
  vars["e"] = cfg.email;
  vars["p"] = cfg.password;
  if (post(Q_LOGIN, vars, out, false) == Result::Ok && storeTokens(out["data"]["login"])) {
    prefs.remove("pass");  // signed in: keep only the rotating refresh token, not the password
    cfg.password = "";
    return true;
  }
  setLink(Link::AuthFailed);
  return false;
}

// Authenticated request with one silent re-sign-in when the access token has expired.
Result call(const char *query, JsonDocument &vars, JsonDocument &out) {
  if (!accessToken.length() || int32_t(millis() - accessValidUntil) > 0) {
    if (!signIn()) return Result::Auth;
  }
  Result r = post(query, vars, out, true);
  if (r != Result::Auth) return r;
  accessToken = "";
  if (!signIn()) return Result::Auth;
  out.clear();
  return post(query, vars, out, true);
}

Result restCall(const char *path, const char *contentType, const uint8_t *body, size_t len, JsonDocument &out) {
  if (!accessToken.length() || int32_t(millis() - accessValidUntil) > 0) {
    if (!signIn()) return Result::Auth;
  }
  Result r = rest(path, contentType, body, len, out);
  if (r != Result::Auth) return r;
  accessToken = "";
  if (!signIn()) return Result::Auth;
  out.clear();
  return rest(path, contentType, body, len, out);
}

bool ok(Result r, JsonDocument &out, const char *field, const char *success) {
  return r == Result::Ok && out["data"][field]["__typename"] == success;
}

bool loadIdentity() {
  JsonDocument vars, out;
  if (call(Q_WHOAMI, vars, out) != Result::Ok) return false;
  JsonArray hh = out["data"]["myHouseholds"];
  if (hh.isNull() || !hh.size()) { Serial.println("This account has no household yet"); return false; }
  householdId = hh[0]["household"]["id"].as<const char *>();
  prefs.putString("hh", householdId);
  char first[24];
  ascii(first, sizeof first, out["data"]["me"]["name"] | "");
  if (char *sp = strchr(first, ' ')) *sp = 0;
  lockData();
  snprintf(data.firstName, sizeof data.firstName, "%s", first);
  unlockData();
  return true;
}

// Inventory, grouped by food: total amount (same unit), soonest expiry. Feeds Home and the Kitchen page.
bool syncKitchen() {
  static KitchenItem items[MAX_KITCHEN];
  int count = 0, useFirst = 0;
  JsonDocument vars, out;
  String after;
  for (int page = 0; page < 10; ++page) {
    vars.clear(); out.clear();
    if (after.length()) vars["after"] = after;
    if (call(Q_INVENTORY, vars, out) != Result::Ok) return false;
    JsonObject inv = out["data"]["inventory"];
    for (JsonObject n : inv["nodes"].as<JsonArray>()) {
      double qty = atof(n["quantity"] | "0");
      if (qty <= 0) continue;
      const char *id = n["foodId"] | "";
      int i = 0;
      while (i < count && strcmp(items[i].foodId, id)) ++i;
      int days = daysUntil(n["estimatedExpiryAt"] | "");
      if (i == count) {
        if (count == MAX_KITCHEN) continue;
        KitchenItem &k = items[count++];
        snprintf(k.foodId, sizeof k.foodId, "%s", id);
        ascii(k.name, sizeof k.name, n["foodName"] | "");
        amount(k.amount, sizeof k.amount, n["quantity"] | "");
        snprintf(k.unit, sizeof k.unit, "%s", n["unit"] | "");
        k.daysLeft = days;
      } else {
        KitchenItem &k = items[i];
        if (!strcmp(k.unit, n["unit"] | "")) {  // same unit: add up the batches
          char sum[16];
          snprintf(sum, sizeof sum, "%.3f", atof(k.amount) + qty);
          amount(k.amount, sizeof k.amount, sum);
        }
        if (days < k.daysLeft) k.daysLeft = days;
      }
    }
    if (!(inv["hasNextPage"] | false)) break;
    after = inv["endCursor"].as<const char *>();
  }
  std::sort(items, items + count, [](const KitchenItem &a, const KitchenItem &b) {
    return a.daysLeft != b.daysLeft ? a.daysLeft < b.daysLeft : strcmp(a.name, b.name) < 0;
  });
  for (int i = 0; i < count; ++i) useFirst += items[i].daysLeft <= 3;
  lockData();
  memcpy(data.kitchen, items, sizeof(KitchenItem) * count);
  data.kitchenCount = count;
  data.kitchenLoaded = true;
  data.foods = count;
  data.useFirst = useFirst;
  data.syncedAtMs = millis();
  unlockData();
  return true;
}

bool syncShopping() {
  JsonDocument vars, out;
  if (call(Q_SHOPPING, vars, out) != Result::Ok) return false;
  ShopItem items[MAX_SHOP];
  int count = 0;
  char listId[40] = "";
  for (JsonObject list : out["data"]["shoppingLists"].as<JsonArray>()) {
    if (list["status"] != "ACTIVE") continue;
    if (!listId[0]) snprintf(listId, sizeof listId, "%s", list["id"] | "");
    for (JsonObject it : list["items"].as<JsonArray>()) {
      if (it["status"] != "NEEDED" || count == MAX_SHOP) continue;
      ShopItem &s = items[count++];
      snprintf(s.id, sizeof s.id, "%s", it["id"] | "");
      snprintf(s.foodId, sizeof s.foodId, "%s", it["foodId"] | "");
      ascii(s.name, sizeof s.name, it["foodName"] | "");
      amount(s.amount, sizeof s.amount, it["quantity"] | "");
      snprintf(s.unit, sizeof s.unit, "%s", it["unit"] | "");  // raw enum, needed for addStock
      s.checked = false;
    }
  }
  lockData();
  for (int i = 0; i < count; ++i)  // keep ticks the user already made
    for (int j = 0; j < data.shopCount; ++j)
      if (!strcmp(items[i].id, data.shop[j].id)) items[i].checked = data.shop[j].checked;
  memcpy(data.shop, items, sizeof(ShopItem) * count);
  data.shopCount = count;
  data.shopLoaded = true;
  snprintf(data.shoppingListId, sizeof data.shoppingListId, "%s", listId);
  unlockData();
  return true;
}

bool syncIdeas() {
  JsonDocument vars, out;
  if (call(Q_IDEAS, vars, out) != Result::Ok) return false;
  Suggestion list[MAX_SUGGESTIONS];
  int count = 0;
  for (JsonObject n : out["data"]["recipeSuggestions"]["nodes"].as<JsonArray>()) {
    if (count == MAX_SUGGESTIONS) break;
    Suggestion &s = list[count++];
    JsonObject r = n["recipe"];
    snprintf(s.id, sizeof s.id, "%s", r["id"] | "");
    ascii(s.name, sizeof s.name, r["name"] | "");
    s.minutes = int(r["cookTimeMin"] | 0) + int(r["prepTimeMin"] | 0);
    s.have = n["haveCount"] | 0;
    s.need = n["requiredCount"] | 0;
  }
  lockData();
  memcpy(data.suggestions, list, sizeof(Suggestion) * count);
  data.suggestionCount = count;
  data.suggestionsLoaded = true;
  unlockData();
  return true;
}

uint8_t mealIndex(const char *type) {
  if (!strcmp(type, "BREAKFAST")) return 0;
  if (!strcmp(type, "LUNCH")) return 1;
  if (!strcmp(type, "DINNER")) return 2;
  if (!strcmp(type, "SNACK")) return 3;
  return 4;
}

bool syncToday() {
  if (!clockSet()) return false;
  char d[12];
  today(d, sizeof d);
  JsonDocument vars, out;
  vars["d"] = d;
  if (call(Q_TODAY, vars, out) != Result::Ok) return false;
  PlannedMeal meals[MAX_PLANNED];
  int count = 0;
  for (JsonObject plan : out["data"]["mealPlans"].as<JsonArray>())
    for (JsonObject m : plan["meals"].as<JsonArray>()) {
      if (count == MAX_PLANNED || strcmp(m["mealDate"] | "", d)) continue;
      PlannedMeal &p = meals[count++];
      snprintf(p.recipeId, sizeof p.recipeId, "%s", m["recipeId"] | "");
      ascii(p.name, sizeof p.name, m["recipeName"] | "");
      p.mealType = mealIndex(m["mealType"] | "");
    }
  std::sort(meals, meals + count, [](const PlannedMeal &a, const PlannedMeal &b) { return a.mealType < b.mealType; });
  JsonArray days = out["data"]["dailyNutrition"];
  JsonObject goal = out["data"]["myProfile"]["goal"];
  lockData();
  memcpy(data.planned, meals, sizeof(PlannedMeal) * count);
  data.plannedCount = count;
  data.todayLoaded = true;
  JsonObject n = days.size() ? days[0].as<JsonObject>() : JsonObject();
  data.kcal = n.isNull() ? 0 : int(atof(n["calories"] | "0") + 0.5);
  data.protein = n.isNull() ? 0 : int(atof(n["proteinG"] | "0") + 0.5);
  data.mealsLogged = n.isNull() ? 0 : int(n["mealCount"] | 0);
  data.kcalTarget = goal.isNull() ? 0 : int(goal["calorieTarget"] | 0);
  data.proteinTarget = goal.isNull() ? 0 : int(goal["proteinTargetG"] | 0);
  unlockData();
  return true;
}

void syncAll() {
  if (setupMode) return;
  if (WiFi.status() != WL_CONNECTED) { setLink(Link::Offline); return; }
  if (!householdId.length() && !loadIdentity()) {
    if (data.link != Link::AuthFailed) setLink(Link::Offline);
    return;
  }
  bool good = syncKitchen();
  good = syncShopping() && good;
  good = syncIdeas() && good;
  good = syncToday() && good;
  if (data.link != Link::AuthFailed) setLink(good ? Link::Online : Link::Offline);
}

String key(const char *what, const char *id) { return String("remi-device-") + what + "-" + id + "-" + String(uint32_t(time(nullptr) / 60)); }

void putAway() {
  ShopItem picked[MAX_SHOP];
  int n = 0;
  lockData();
  for (int i = 0; i < data.shopCount; ++i)
    if (data.shop[i].checked) picked[n++] = data.shop[i];
  unlockData();
  char d[12] = "";
  if (clockSet()) today(d, sizeof d);
  int added = 0;
  bool failed = false;
  for (int i = 0; i < n; ++i) {
    JsonDocument vars, out;
    JsonObject in = vars["i"].to<JsonObject>();
    in["foodId"] = picked[i].foodId;
    in["amount"] = picked[i].amount;
    in["unit"] = picked[i].unit;
    in["idempotencyKey"] = String("remi-device-") + picked[i].id;  // a retry never adds the same purchase twice
    if (d[0]) in["purchaseDate"] = d;
    if (!ok(call(Q_ADD_STOCK, vars, out), out, "addStock", "InventoryMutationSuccessType")) { failed = true; continue; }
    vars.clear(); out.clear();
    JsonObject mark = vars["i"].to<JsonObject>();
    mark["id"] = picked[i].id;
    mark["status"] = "PURCHASED";
    call(Q_MARK_BOUGHT, vars, out);  // stock is in; a failed tick just leaves the item on the list
    ++added;
  }
  syncShopping();
  syncKitchen();
  lockData();
  data.putAwayCount = added;
  data.putAway = failed && !added ? Job::Failed : Job::Done;
  unlockData();
}

void kitchenAction(int index, bool used) {
  KitchenItem it;
  lockData();
  bool valid = index >= 0 && index < data.kitchenCount;
  if (valid) it = data.kitchen[index];
  unlockData();
  bool good = false;
  if (valid) {
    JsonDocument vars, out;
    JsonObject in = vars["i"].to<JsonObject>();
    in["foodId"] = it.foodId;
    in["amount"] = it.amount;
    in["unit"] = it.unit;
    in["idempotencyKey"] = key(used ? "use" : "toss", it.foodId);
    good = used ? ok(call(Q_CONSUME, vars, out), out, "consumeStock", "InventoryMutationSuccessType")
                : ok(call(Q_DISCARD, vars, out), out, "discardStock", "InventoryMutationSuccessType");
    if (good) syncKitchen();
  }
  lockData();
  data.kitchenAction = good ? Job::Done : Job::Failed;
  unlockData();
}

void loadRecipe() {
  char id[40];
  lockData();
  snprintf(id, sizeof id, "%s", data.recipeId);
  data.recipe = Job::Working;
  unlockData();
  JsonDocument vars, out;
  vars["id"] = id;
  if (!id[0] || call(Q_RECIPE, vars, out) != Result::Ok) {
    lockData(); data.recipe = Job::Failed; unlockData();
    return;
  }
  JsonObject r = out["data"]["recipe"];
  JsonArray steps = r["steps"];
  lockData();
  ascii(data.recipeName, sizeof data.recipeName, r["name"] | "");
  data.recipeMinutes = int(r["cookTimeMin"] | 0) + int(r["prepTimeMin"] | 0);
  int count = 0;
  for (int want = 1; count < MAX_STEPS; ++want) {  // order by stepNumber
    JsonObject found;
    for (JsonObject s : steps)
      if ((s["stepNumber"] | 0) == want) found = s;
    if (found.isNull()) break;
    ascii(data.steps[count].text, sizeof data.steps[count].text, found["instruction"] | "");
    data.steps[count].minutes = found["durationMin"] | 0;
    ++count;
  }
  data.stepCount = count;
  int ing = 0;
  for (JsonObject g : r["ingredients"].as<JsonArray>()) {
    if (ing == 24 || (g["optional"] | false)) continue;
    Ingredient &x = data.ingredients[ing++];
    snprintf(x.foodId, sizeof x.foodId, "%s", g["foodId"] | "");
    snprintf(x.amount, sizeof x.amount, "%s", g["quantity"] | "0");
    snprintf(x.unit, sizeof x.unit, "%s", g["unit"] | "");
  }
  data.ingredientCount = ing;
  data.recipe = count ? Job::Done : Job::Failed;
  unlockData();
}

// "I cooked it": log the recipe as a meal, which deducts its ingredients from the kitchen.
void cookedIt() {
  JsonDocument vars, out;
  JsonObject in = vars["i"].to<JsonObject>();
  time_t now = time(nullptr);
  char iso[24];
  strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  int hour = localtime(&now)->tm_hour;
  lockData();
  in["mealType"] = hour < 11 ? "BREAKFAST" : hour < 16 ? "LUNCH" : hour < 22 ? "DINNER" : "SNACK";
  in["eatenAt"] = iso;
  in["name"] = data.recipeName;
  in["idempotencyKey"] = key("meal", data.recipeId);
  JsonArray items = in["items"].to<JsonArray>();
  for (int i = 0; i < data.ingredientCount; ++i) {
    JsonObject x = items.add<JsonObject>();
    x["foodId"] = data.ingredients[i].foodId;
    x["amount"] = data.ingredients[i].amount;
    x["unit"] = data.ingredients[i].unit;
  }
  unlockData();
  bool good = clockSet() && ok(call(Q_LOG_MEAL, vars, out), out, "logMeal", "MealLogSuccessType");
  if (good) { syncKitchen(); syncToday(); syncIdeas(); }
  lockData();
  data.cooked = good ? Job::Done : Job::Failed;
  unlockData();
}

void searchFoods() {
  char q[32];
  lockData();
  snprintf(q, sizeof q, "%s", data.pickQuery);
  unlockData();
  JsonDocument vars, out;
  vars["q"] = q;
  Result r = call(Q_FOODS, vars, out);
  FoodPick picks[MAX_PICKS];
  int count = 0;
  if (r == Result::Ok)
    for (JsonObject f : out["data"]["foods"]["nodes"].as<JsonArray>()) {
      if (count == MAX_PICKS) break;
      FoodPick &p = picks[count++];
      snprintf(p.foodId, sizeof p.foodId, "%s", f["id"] | "");
      ascii(p.name, sizeof p.name, f["name"] | "");
      snprintf(p.unit, sizeof p.unit, "%s", f["defaultUnit"] | "PIECE");
    }
  lockData();
  memcpy(data.picks, picks, sizeof(FoodPick) * count);
  data.pickCount = count;
  data.search = r == Result::Ok ? Job::Done : Job::Failed;
  unlockData();
}

void addToList(int index) {
  FoodPick p;
  char listId[40];
  lockData();
  bool valid = index >= 0 && index < data.pickCount && data.shoppingListId[0];
  if (valid) p = data.picks[index];
  snprintf(listId, sizeof listId, "%s", data.shoppingListId);
  unlockData();
  bool good = false;
  if (valid) {
    JsonDocument vars, out;
    JsonObject in = vars["i"].to<JsonObject>();
    in["foodId"] = p.foodId;
    in["quantity"] = "1";
    in["unit"] = p.unit;
    in["shoppingListId"] = listId;
    in["idempotencyKey"] = key("list", p.foodId);
    good = call(Q_ADD_TO_LIST, vars, out) == Result::Ok;
    if (good) syncShopping();
  }
  lockData();
  data.addItem = good ? Job::Done : Job::Failed;
  unlockData();
}

// ---- AI: replies and photo reading (backend/src/modules/device) ----
void ask() {
  JsonDocument body, out;
  lockData();
  body["question"] = data.askQuestion;
  unlockData();
  String json;
  serializeJson(body, json);
  bool good = restCall("/device/ask", "application/json", (const uint8_t *)json.c_str(), json.length(), out) == Result::Ok;
  lockData();
  if (good) ascii(data.askReply, sizeof data.askReply, out["reply"] | "");
  data.ask = good && data.askReply[0] ? Job::Done : Job::Failed;  // Failed: the UI keeps its local answer
  unlockData();
}

struct ScanItemRow { char foodId[40], amount[16], unit[12]; };
ScanItemRow scanItems[24];
int scanItemCount = 0;
char scanActionKind[8] = "";

void scan(int mode) {
  static const char *const MODES[] = {"barcode", "label", "list", "receipt"};
  Photo photo{nullptr, 0, false};
  bool have = takePhoto(photo);                                   // a photo sent from the phone wins
  if (!have) have = cameraCaptureJpeg(&photo.bytes, &photo.len);  // else the CAM's own camera
  JsonDocument out;
  Result r = Result::Failed;
  if (have) {
    char path[48];
    snprintf(path, sizeof path, "/device/scan?mode=%s", MODES[mode & 3]);
    r = restCall(path, photo.png ? "image/png" : "image/jpeg", photo.bytes, photo.len, out);
  }
  free(photo.bytes);
  lockData();
  if (r == Result::Ok) {
    ascii(data.scanTitle, sizeof data.scanTitle, out["title"] | "");
    data.scanLineCount = 0;
    for (JsonVariant l : out["lines"].as<JsonArray>()) {
      if (data.scanLineCount == 6) break;
      ascii(data.scanLines[data.scanLineCount++], sizeof data.scanLines[0], l | "");
    }
    const char *action = out["action"] | "none";
    snprintf(scanActionKind, sizeof scanActionKind, "%s", action);
    scanItemCount = 0;
    for (JsonObject it : out["items"].as<JsonArray>()) {
      if (scanItemCount == 24) break;
      ScanItemRow &row = scanItems[scanItemCount++];
      snprintf(row.foodId, sizeof row.foodId, "%s", it["foodId"] | "");
      snprintf(row.amount, sizeof row.amount, "%s", it["amount"] | "1");
      snprintf(row.unit, sizeof row.unit, "%s", it["unit"] | "PIECE");
    }
    data.scanAction = !scanItemCount ? 0 : !strcmp(action, "list") ? 2 : !strcmp(action, "kitchen") ? 1 : 0;
    data.scan = Job::Done;
  } else {
    data.scan = Job::Failed;
  }
  unlockData();
}

void scanApply() {
  char d[12] = "";
  if (clockSet()) today(d, sizeof d);
  int done = 0;
  for (int i = 0; i < scanItemCount; ++i) {
    JsonDocument vars, out;
    JsonObject in = vars["i"].to<JsonObject>();
    in["foodId"] = scanItems[i].foodId;
    in["unit"] = scanItems[i].unit;
    in["idempotencyKey"] = key("scan", scanItems[i].foodId);
    bool good;
    if (!strcmp(scanActionKind, "list")) {
      in["quantity"] = scanItems[i].amount;
      in["shoppingListId"] = data.shoppingListId;
      good = data.shoppingListId[0] && call(Q_ADD_TO_LIST, vars, out) == Result::Ok;
    } else {
      in["amount"] = scanItems[i].amount;
      if (d[0]) in["purchaseDate"] = d;
      good = ok(call(Q_ADD_STOCK, vars, out), out, "addStock", "InventoryMutationSuccessType");
    }
    done += good;
  }
  if (done) { syncKitchen(); syncShopping(); }
  lockData();
  data.scanApply = done ? Job::Done : Job::Failed;
  unlockData();
}

// ---- Pairing with the Remi app: the device shows a code, the person types it in the app ----
bool pairWithApp() {
  JsonDocument vars, out;
  vars["n"] = "Remi kitchen";
  if (post(Q_START_PAIRING, vars, out, false) != Result::Ok) return false;
  String id = out["data"]["startDevicePairing"]["pairingId"] | "";
  String secret = out["data"]["startDevicePairing"]["secret"] | "";
  lockData();
  snprintf(data.pairCode, sizeof data.pairCode, "%s", (const char *)(out["data"]["startDevicePairing"]["code"] | ""));
  data.link = Link::AuthFailed;
  unlockData();
  Serial.printf("Pair this Remi in the app with code %s\n", data.pairCode);
  uint32_t until = millis() + uint32_t(out["data"]["startDevicePairing"]["expiresInSeconds"] | 600) * 1000;
  while (int32_t(until - millis()) > 0) {
    delay(3000);
    JsonDocument v, o;
    v["id"] = id;
    v["s"] = secret;
    if (post(Q_CLAIM_PAIRING, v, o, false) != Result::Ok) continue;
    JsonObject c = o["data"]["claimDevicePairing"];
    if (c["status"] == "EXPIRED") break;
    if (c["status"] == "PAIRED") {
      refreshToken = c["refreshToken"].as<const char *>();
      householdId = c["householdId"].as<const char *>();
      prefs.putString("refresh", refreshToken);
      prefs.putString("hh", householdId);
      lockData(); data.pairCode[0] = 0; data.link = Link::Connecting; unlockData();
      return true;
    }
  }
  lockData(); data.pairCode[0] = 0; unlockData();
  return false;
}

void startSetupNetwork() {
  setupMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SETUP_SSID, SETUP_PASS);
  lockData();
  data.link = Link::Setup;
  snprintf(data.setupSsid, sizeof data.setupSsid, "%s", SETUP_SSID);
  snprintf(data.setupPass, sizeof data.setupPass, "%s", SETUP_PASS);
  snprintf(data.remoteUrl, sizeof data.remoteUrl, "http://%s", WiFi.softAPIP().toString().c_str());
  unlockData();
  Serial.printf("Setup Wi-Fi \"%s\" (password %s), open %s\n", SETUP_SSID, SETUP_PASS, data.remoteUrl);
}

bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  setLink(Link::Connecting);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("remi");
  WiFi.begin(cfg.ssid.c_str(), cfg.wifiPass.c_str());
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) delay(500);
  if (WiFi.status() != WL_CONNECTED) { setLink(Link::Offline); return false; }
  MDNS.begin("remi");
  MDNS.addService("http", "tcp", 80);
  lockData();
  snprintf(data.remoteUrl, sizeof data.remoteUrl, "http://%s", WiFi.localIP().toString().c_str());
  snprintf(data.wifiName, sizeof data.wifiName, "%s", cfg.ssid.c_str());
  data.setupSsid[0] = 0;
  unlockData();
  Serial.printf("Wi-Fi connected, remote at %s (or http://remi.local)\n", data.remoteUrl);
  return true;
}

void task(void *) {
  prefs.begin("remi", false);
  refreshToken = prefs.getString("refresh", "");
  householdId = prefs.getString("hh", "");
  if (!cfg.ssid.length() || !connectWifi()) {
    startSetupNetwork();  // no Wi-Fi saved, or it can't be reached: let a phone set it up
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  configTzTime(cfg.tz.c_str(), "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 20 && !clockSet(); ++i) delay(250);  // Today needs the date
  if (!cfg.apiUrl.length()) setLink(Link::AuthFailed);
  else {
    while (!refreshToken.length() && !cfg.password.length())  // no account on the device: pair from the app
      if (!pairWithApp()) delay(5000);
    syncAll();
  }
  uint32_t lastSync = millis();
  for (;;) {
    Cmd cmd;
    uint32_t wait = REFRESH_EVERY_MS - min<uint32_t>(REFRESH_EVERY_MS, millis() - lastSync);
    if (xQueueReceive(commands, &cmd, pdMS_TO_TICKS(wait)) != pdTRUE) cmd = {Request::Refresh, 0};
    if (!connectWifi()) continue;
    switch (cmd.r) {
      case Request::Refresh: syncAll(); lastSync = millis(); break;
      case Request::PutAway: putAway(); break;
      case Request::LoadRecipe: loadRecipe(); break;
      case Request::CookedIt: cookedIt(); break;
      case Request::KitchenUse: kitchenAction(cmd.arg, true); break;
      case Request::KitchenToss: kitchenAction(cmd.arg, false); break;
      case Request::SearchFoods: searchFoods(); break;
      case Request::AddToList: addToList(cmd.arg); break;
      case Request::Scan: scan(cmd.arg); break;
      case Request::ScanApply: scanApply(); break;
      case Request::Ask: ask(); break;
    }
  }
}

}  // namespace

bool backendInSetupMode() { return setupMode; }

void backendRequest(Request r, int arg) {
  if (r == Request::PutAway) { lockData(); data.putAway = Job::Working; unlockData(); }
  if (r == Request::LoadRecipe) { lockData(); data.recipe = Job::Working; unlockData(); }
  if (setupMode) {  // nothing to talk to yet: fail fast so the UI doesn't spin
    lockData();
    if (r == Request::PutAway) data.putAway = Job::Failed;
    if (r == Request::LoadRecipe) data.recipe = Job::Failed;
    if (r == Request::CookedIt) data.cooked = Job::Failed;
    if (r == Request::KitchenUse || r == Request::KitchenToss) data.kitchenAction = Job::Failed;
    if (r == Request::SearchFoods) data.search = Job::Failed;
    if (r == Request::AddToList) data.addItem = Job::Failed;
    if (r == Request::Scan) data.scan = Job::Failed;
    if (r == Request::ScanApply) data.scanApply = Job::Failed;
    if (r == Request::Ask) data.ask = Job::Failed;
    unlockData();
    return;
  }
  Cmd cmd{r, arg};
  xQueueSend(commands, &cmd, 0);
}

void backendBegin() {
  cfg = loadConfig();
  commands = xQueueCreate(8, sizeof(Cmd));
  xTaskCreatePinnedToCore(task, "remi-net", 12288, nullptr, 1, nullptr, 0);
}
