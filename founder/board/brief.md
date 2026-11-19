# Board brief: Remi

## The idea (from the founder)
- What it is: a kitchen companion that tracks the food in your home, plans and guides cooking, and builds the shopping list, as a phone app plus an optional countertop device.
- Who it is for: Arabic- and English-speaking households across MENA (Gulf and North Africa) who cook at home and manage groceries, meals and nutrition.
- Business model: a paid subscription that unlocks features, plus a one-time purchase of the Remi countertop device. Prices are not set yet.
- Where and how: online. The app goes on the App Store and Google Play; the device would be sold direct. No acquisition channel chosen yet.
- Budget and constraints: started as a university graduation project. No budget. Student team.

## What is already built (facts from the project's own docs)
**App (Expo / React Native, iOS + Android, English and Arabic; NestJS GraphQL backend, PostgreSQL):**
- Kitchen inventory with probable expiry dates for fresh produce (USDA FoodKeeper averages) and a "use first" view.
- Catalogue: 851 foods with 2,074 English/Arabic aliases, 400 recipes (each with a photo), 79 packaged products, 707 foods with USDA nutrition data.
- Receipt scanning: a receipt photo is read line by line, matched to the catalogue, and confirmed foods go into the kitchen in one step.
- Save recipes from a pasted link.
- Meal planner, shopping list, full-screen cook mode with timers; cooking a meal deducts ingredients from the kitchen.
- Nutrition page (daily/weekly/monthly dashboards, streaks, awards), push reminders, offline reading, home-screen widget.
- Households: multiple members share one kitchen, with authentication and household isolation.
- App Store readiness work done (account deletion, privacy policy, terms). 157 backend tests and 441 app tests pass.
- Strong visual identity ("Sip Happy": lemon, cobalt, pink; a mascot character named Remi).
- Open items the team itself lists: allergen tags and recipe data need expert review; production needs load testing, recovery validation, and real email/storage setup.

**Device (prototype, ESP32 with a 1.8" colour screen, one knob, a speaker; camera on the bench build):**
- Sits on the counter, talks to the same backend, pairs with the app by a 6-digit code.
- Pages: home (time, foods in the kitchen, what to use first), today's calories/protein and planned meals, kitchen list with "used it up" / "threw it out", "just bought" (tick off the shopping list and put groceries away), cook mode with step timers, "Ask Remi" (answers from kitchen data; typed questions go to Claude, an AI model, with the kitchen as context), scan (barcode, nutrition label, shopping list, receipt via camera or phone photo).
- Becomes an ambient fridge clock with the animated mascot when idle; works offline from cache.
- Controlled by the knob or by a phone web remote. A microphone is planned.
- Built from hobby parts; no enclosure in production, no manufacturing, certification or unit-cost work done yet.

## Unknowns (no one has numbers yet)
Subscription price, device price and unit cost, market size, competitors, cost to acquire a customer, AI/API running costs per user, whether households will keep logging food over time.
