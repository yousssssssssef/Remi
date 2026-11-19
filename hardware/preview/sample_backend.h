#pragma once
// Sample kitchen data and a fake Remi API with realistic delays, shared by the Mac preview (preview.cpp)
// and the browser simulator (sim/wasm_main.cpp). Include after defining nowMs() and `data`.
#include <stdio.h>
#include <string.h>
#include "app_data.h"

struct Pending { Request r; int arg; uint32_t due; };
inline Pending pending[8];  // plain array: the browser build links no C++ library
inline int pendingCount = 0;
void backendRequest(Request r, int arg) {
  if (r == Request::PutAway) data.putAway = Job::Working;
  if (r == Request::LoadRecipe) data.recipe = Job::Working;
  if (pendingCount < 8) pending[pendingCount++] = {r, arg, nowMs() + (r == Request::PutAway ? 1100u : 700u)};
  if (r == Request::CookedIt) data.cooked = Job::Working;
}

inline void copy(char *dst, size_t n, const char *s) { snprintf(dst, n, "%s", s); }

inline void sampleData() {
  data.link = Link::Online;
  copy(data.firstName, sizeof data.firstName, "Sam");
  data.foods = 42; data.useFirst = 3; data.syncedAtMs = 1;
  const char *shop[][3] = {{"Milk", "1", "LITER"}, {"Eggs", "12", "PIECE"}, {"Tomatoes", "6", "PIECE"},
                           {"Baby spinach", "200", "GRAM"}, {"Chicken breast", "500", "GRAM"}, {"Greek yogurt", "2", "PIECE"}};
  data.shopCount = 6;
  for (int i = 0; i < 6; ++i) {
    snprintf(data.shop[i].id, 40, "item-%d", i);
    copy(data.shop[i].name, 32, shop[i][0]); copy(data.shop[i].amount, 16, shop[i][1]); copy(data.shop[i].unit, 12, shop[i][2]);
    data.shop[i].checked = false;
  }
  data.shopLoaded = true;
  struct { const char *n; int m, h, r; } s[] = {{"Shakshuka", 25, 5, 6}, {"Tomato basil pasta", 20, 6, 6},
                                                  {"Chicken shawarma bowl", 35, 4, 7}, {"Spinach omelette", 10, 3, 3},
                                                  {"Red lentil soup", 40, 5, 5}};
  data.suggestionCount = 5;
  for (int i = 0; i < 5; ++i) {
    snprintf(data.suggestions[i].id, 40, "recipe-%d", i);
    copy(data.suggestions[i].name, 56, s[i].n);
    data.suggestions[i].minutes = s[i].m; data.suggestions[i].have = s[i].h; data.suggestions[i].need = s[i].r;
  }
  data.suggestionsLoaded = true;
  struct { const char *n, *a, *u; int d; } k[] = {
      {"Baby spinach", "150", "GRAM", 1}, {"Greek yogurt", "1", "PIECE", 2}, {"Tomatoes", "4", "PIECE", 3},
      {"Milk", "1", "LITER", 5}, {"Eggs", "8", "PIECE", 12}, {"Cheddar", "200", "GRAM", 20}, {"Red lentils", "500", "GRAM", NO_EXPIRY},
      {"Rice", "1", "KILOGRAM", NO_EXPIRY}};
  data.kitchenCount = 8;
  for (int i = 0; i < 8; ++i) {
    snprintf(data.kitchen[i].foodId, 40, "food-%d", i);
    copy(data.kitchen[i].name, 32, k[i].n); copy(data.kitchen[i].amount, 16, k[i].a); copy(data.kitchen[i].unit, 12, k[i].u);
    data.kitchen[i].daysLeft = k[i].d;
  }
  data.kitchenLoaded = true;
  struct { const char *id, *n; uint8_t t; } m[] = {{"recipe-3", "Spinach omelette", 0}, {"recipe-0", "Shakshuka", 2}};
  data.plannedCount = 2;
  for (int i = 0; i < 2; ++i) { copy(data.planned[i].recipeId, 40, m[i].id); copy(data.planned[i].name, 48, m[i].n); data.planned[i].mealType = m[i].t; }
  data.todayLoaded = true;
  data.kcal = 1240; data.kcalTarget = 2000; data.protein = 62; data.proteinTarget = 110; data.mealsLogged = 2;
  copy(data.shoppingListId, 40, "list-1");
  copy(data.remoteUrl, 48, "http://192.168.1.42");
  data.hasCamera = true;  // simulated: the Scan page shows a placeholder viewfinder
}

inline int recipeIndex(const char *id) {
  for (int i = 0; i < data.suggestionCount; ++i)
    if (!strcmp(data.suggestions[i].id, id)) return i;
  return 0;
}

inline void finishPending() {
  for (int i = 0; i < pendingCount;) {
    Pending p = pending[i];
    if (nowMs() < p.due) { ++i; continue; }
    for (int k = i + 1; k < pendingCount; ++k) pending[k - 1] = pending[k];
    --pendingCount;
    if (p.r == Request::Refresh) data.syncedAtMs = nowMs();
    if (p.r == Request::PutAway) {
      int n = 0, k = 0;
      for (int j = 0; j < data.shopCount; ++j) {
        if (data.shop[j].checked) { ++n; continue; }
        data.shop[k++] = data.shop[j];
      }
      data.shopCount = k; data.putAwayCount = n; data.foods += n; data.putAway = Job::Done;
    }
    if (p.r == Request::LoadRecipe) {
      const char *steps[] = {
          "Warm 2 tbsp olive oil in a wide pan over medium heat. Add the onion and pepper and cook until soft.",
          "Stir in the garlic, cumin and paprika. Cook for one minute until fragrant.",
          "Add the chopped tomatoes with a pinch of salt. Simmer until thick and saucy, stirring now and then.",
          ("Make four wells in the sauce and crack an egg into each. Cover and cook until the whites are set but the "
           "yolks are still soft. Check after five minutes; the pan keeps cooking the eggs after you take it off the heat."),
          "Scatter with parsley and serve straight from the pan with warm bread."};
      int mins[] = {6, 1, 10, 6, 0};
      int s = recipeIndex(data.recipeId);
      copy(data.recipeName, 56, data.suggestions[s].name);
      data.recipeMinutes = data.suggestions[s].minutes;
      data.stepCount = 5;
      for (int j = 0; j < 5; ++j) { copy(data.steps[j].text, 360, steps[j]); data.steps[j].minutes = mins[j]; }
      data.recipe = Job::Done;
    }
    if (p.r == Request::CookedIt) { data.cooked = Job::Done; data.kcal += 420; data.protein += 24; ++data.mealsLogged; }
    if (p.r == Request::KitchenUse || p.r == Request::KitchenToss) {
      for (int j = p.arg; j + 1 < data.kitchenCount; ++j) data.kitchen[j] = data.kitchen[j + 1];
      --data.kitchenCount; data.foods = data.kitchenCount; data.kitchenAction = Job::Done;
    }
    if (p.r == Request::SearchFoods) {
      const char *found[] = {"Whole milk", "Skimmed milk", "Oat milk", "Coconut milk"};
      data.pickCount = 4;
      for (int j = 0; j < 4; ++j) { snprintf(data.picks[j].foodId, 40, "pick-%d", j); copy(data.picks[j].name, 32, found[j]); copy(data.picks[j].unit, 12, "LITER"); }
      data.search = Job::Done;
    }
    if (p.r == Request::Scan) {
      static const char *titles[] = {"Greek yogurt, 500 g", "Per 100 g", "Shopping list: 4 items", "Receipt: 5 foods"};
      static const char *lines[4][5] = {{"Barcode 5201054017845", "97 kcal \x7f 10 g protein", "Fat 2 g \x7f carbs 4 g", "Good with your spinach", ""},
                                        {"245 kcal", "Protein 12 g", "Carbs 30 g \x7f sugar 6 g", "Fat 8 g \x7f salt 0.9 g", ""},
                                        {"Eggs", "Milk", "Basil", "Lemons", ""},
                                        {"Tomatoes 6 pcs", "Chicken breast 500 g", "Milk 1 L", "Baby spinach 200 g", "Greek yogurt 2 pcs"}};
      static const int counts[] = {4, 4, 4, 5};
      static const uint8_t actions[] = {1, 0, 2, 1};
      int m = data.scanMode & 3;
      copy(data.scanTitle, 48, titles[m]);
      data.scanLineCount = counts[m];
      for (int j = 0; j < counts[m]; ++j) copy(data.scanLines[j], 44, lines[m][j]);
      data.scanAction = actions[m];
      data.scan = Job::Done;
    }
    if (p.r == Request::ScanApply) data.scanApply = Job::Done;
    if (p.r == Request::Ask) {
      copy(data.askReply, 300, "You've got spinach that needs using and eggs, so a spinach omelette is perfect tonight: ten minutes, and it uses the spinach before it turns.");
      data.ask = Job::Done;
    }
    if (p.r == Request::AddToList) {
      if (data.shopCount < MAX_SHOP) {
        ShopItem &it = data.shop[data.shopCount++];
        snprintf(it.id, 40, "item-new-%d", p.arg); copy(it.foodId, 40, data.picks[p.arg].foodId);
        copy(it.name, 32, data.picks[p.arg].name); copy(it.amount, 16, "1"); copy(it.unit, 12, data.picks[p.arg].unit); it.checked = false;
      }
      data.addItem = Job::Done;
    }
  }
}

