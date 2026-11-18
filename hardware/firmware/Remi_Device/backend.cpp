// Wi-Fi, clock and Remi API sync. Runs as its own FreeRTOS task on core 0 so the UI never waits on the network.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include "app_data.h"
#include "backend.h"
#include "config.h"
#include "queries.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h not found: using secrets.example.h (copy it to secrets.h and fill it in)"
#include "secrets.example.h"
#endif

namespace {

enum class Result { Ok, Auth, Net, Failed };

struct Cmd { Request r; int arg; };
QueueHandle_t commands;
Preferences prefs;
String accessToken, refreshToken, householdId;
uint32_t accessValidUntil = 0;

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

Result post(const char *query, JsonDocument &vars, JsonDocument &out, bool auth) {
  if (WiFi.status() != WL_CONNECTED) return Result::Net;
  JsonDocument body;
  body["query"] = query;
  body["variables"] = vars;
  String payload;
  serializeJson(body, payload);
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(REMI_API_URL)) return Result::Net;
  http.addHeader("Content-Type", "application/json");
  http.setUserAgent("Remi-Device/1.0 (ESP32)");
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
  if (!strlen(REMI_EMAIL)) return false;
  vars.clear(); out.clear();
  vars["e"] = REMI_EMAIL;
  vars["p"] = REMI_PASSWORD;
  if (post(Q_LOGIN, vars, out, false) == Result::Ok && storeTokens(out["data"]["login"])) return true;
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

bool loadIdentity() {
  JsonDocument vars, out;
  if (call(Q_WHOAMI, vars, out) != Result::Ok) return false;
  JsonArray hh = out["data"]["myHouseholds"];
  if (hh.isNull() || !hh.size()) { Serial.println("This account has no household yet"); return false; }
  householdId = hh[0]["household"]["id"].as<const char *>();
  char first[24];
  ascii(first, sizeof first, out["data"]["me"]["name"] | "");
  if (char *sp = strchr(first, ' ')) *sp = 0;
  lockData();
  snprintf(data.firstName, sizeof data.firstName, "%s", first);
  unlockData();
  return true;
}

bool syncKitchen() {
  // Count distinct foods in stock, and those expiring within three days ("use first").
  constexpr int MAX_FOODS = 400;
  static char ids[MAX_FOODS][37];
  static bool soon[MAX_FOODS];
  int foods = 0;
  char cutoff[24] = "";
  time_t now = time(nullptr);
  if (now > 1700000000) {
    now += 3 * 86400;
    strftime(cutoff, sizeof cutoff, "%Y-%m-%dT%H:%M:%S", gmtime(&now));
  }
  JsonDocument vars, out;
  String after;
  for (int page = 0; page < 10; ++page) {
    vars.clear(); out.clear();
    if (after.length()) vars["after"] = after;
    if (call(Q_INVENTORY, vars, out) != Result::Ok) return false;
    JsonObject inv = out["data"]["inventory"];
    for (JsonObject n : inv["nodes"].as<JsonArray>()) {
      if (atof(n["quantity"] | "0") <= 0) continue;
      const char *id = n["foodId"] | "";
      const char *exp = n["estimatedExpiryAt"] | "";
      int i = 0;
      while (i < foods && strcmp(ids[i], id)) ++i;
      if (i == foods) {
        if (foods == MAX_FOODS) continue;
        snprintf(ids[foods], 37, "%s", id);
        soon[foods++] = false;
      }
      if (cutoff[0] && exp[0] && strcmp(exp, cutoff) < 0) soon[i] = true;
    }
    if (!(inv["hasNextPage"] | false)) break;
    after = inv["endCursor"].as<const char *>();
  }
  int useFirst = 0;
  for (int i = 0; i < foods; ++i) useFirst += soon[i];
  lockData();
  data.foods = foods;
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
  for (JsonObject list : out["data"]["shoppingLists"].as<JsonArray>()) {
    if (list["status"] != "ACTIVE") continue;
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

void syncAll() {
  if (WiFi.status() != WL_CONNECTED) { setLink(Link::Offline); return; }
  if (!householdId.length() && !loadIdentity()) {
    if (data.link != Link::AuthFailed) setLink(Link::Offline);
    return;
  }
  bool ok = syncKitchen();
  ok = syncShopping() && ok;
  ok = syncIdeas() && ok;
  if (data.link != Link::AuthFailed) setLink(ok ? Link::Online : Link::Offline);
}

void putAway() {
  ShopItem picked[MAX_SHOP];
  int n = 0;
  lockData();
  for (int i = 0; i < data.shopCount; ++i)
    if (data.shop[i].checked) picked[n++] = data.shop[i];
  unlockData();
  char today[12] = "";
  time_t now = time(nullptr);
  if (now > 1700000000) strftime(today, sizeof today, "%Y-%m-%d", localtime(&now));
  int added = 0;
  bool failed = false;
  for (int i = 0; i < n; ++i) {
    JsonDocument vars, out;
    JsonObject in = vars["i"].to<JsonObject>();
    in["foodId"] = picked[i].foodId;
    in["amount"] = picked[i].amount;
    in["unit"] = picked[i].unit;
    in["idempotencyKey"] = String("remi-device-") + picked[i].id;  // a retry never adds the same purchase twice
    if (today[0]) in["purchaseDate"] = today;
    if (call(Q_ADD_STOCK, vars, out) != Result::Ok || out["data"]["addStock"]["__typename"] != "InventoryMutationSuccessType") {
      failed = true;
      continue;
    }
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

void loadRecipe(int index) {
  char id[40];
  lockData();
  snprintf(id, sizeof id, "%s", index < data.suggestionCount ? data.suggestions[index].id : "");
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
  snprintf(data.recipeId, sizeof data.recipeId, "%s", id);
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
  data.recipe = count ? Job::Done : Job::Failed;
  unlockData();
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  setLink(Link::Connecting);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) delay(500);
  if (WiFi.status() != WL_CONNECTED) { setLink(Link::Offline); return; }
  Serial.printf("Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
}

void task(void *) {
  prefs.begin("remi", false);
  refreshToken = prefs.getString("refresh", "");
  connectWifi();
  configTzTime(REMI_TZ, "pool.ntp.org", "time.google.com");
  syncAll();
  uint32_t lastSync = millis();
  for (;;) {
    Cmd cmd;
    uint32_t wait = REFRESH_EVERY_MS - min<uint32_t>(REFRESH_EVERY_MS, millis() - lastSync);
    if (xQueueReceive(commands, &cmd, pdMS_TO_TICKS(wait)) != pdTRUE) cmd = {Request::Refresh, 0};
    connectWifi();
    if (cmd.r == Request::Refresh) { syncAll(); lastSync = millis(); }
    else if (cmd.r == Request::PutAway) putAway();
    else if (cmd.r == Request::LoadRecipe) loadRecipe(cmd.arg);
  }
}

}  // namespace

void backendRequest(Request r, int arg) {
  if (r == Request::PutAway) { lockData(); data.putAway = Job::Working; unlockData(); }
  if (r == Request::LoadRecipe) { lockData(); data.recipe = Job::Working; unlockData(); }
  Cmd cmd{r, arg};
  xQueueSend(commands, &cmd, 0);
}

void backendBegin() {
  commands = xQueueCreate(8, sizeof(Cmd));
  xTaskCreatePinnedToCore(task, "remi-net", 12288, nullptr, 1, nullptr, 0);
}
