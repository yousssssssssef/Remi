#pragma once
// Sample kitchen data and a fake Remi API with realistic delays, shared by the Mac preview (preview.cpp)
// and the browser simulator (sim/wasm_main.cpp). Include after defining nowMs() and `data`.
#include <stdio.h>
#include "app_data.h"

struct Pending { Request r; int arg; uint32_t due; };
inline Pending pending[8];  // plain array: the browser build links no C++ library
inline int pendingCount = 0;
void backendRequest(Request r, int arg) {
  if (r == Request::PutAway) data.putAway = Job::Working;
  if (r == Request::LoadRecipe) data.recipe = Job::Working;
  if (pendingCount < 8) pending[pendingCount++] = {r, arg, nowMs() + (r == Request::PutAway ? 1100u : 700u)};
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
      copy(data.recipeId, 40, data.suggestions[p.arg].id);
      copy(data.recipeName, 56, data.suggestions[p.arg].name);
      data.recipeMinutes = data.suggestions[p.arg].minutes;
      data.stepCount = 5;
      for (int j = 0; j < 5; ++j) { copy(data.steps[j].text, 360, steps[j]); data.steps[j].minutes = mins[j]; }
      data.recipe = Job::Done;
    }
  }
}

