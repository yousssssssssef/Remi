# Offer: Remi

**Result: the new offer moved the panel from 0 of 20 to 1 of 20 (5%). The one "yes" was for the free shared list, not for Plus.** Nobody on either panel would pay. The offer isn't the main problem: the habit is. Twelve of 20 buyers pass because they won't keep feeding the app, and the person who runs the kitchen (a helper, a mother) isn't the person who installs it.

Same 20 simulated buyers (seed 544233) both times. v1 pitch: `pitch-v1.md`, results in `panel-v1/`. v2 pitch: `pitch.md`, results in `panel/`.

## The problem list (in the buyers' words, from both panels)

| # | obstacle | when | how often |
| ---: | --- | --- | ---: |
| 1 | "I won't keep scanning receipts every shop" | after | 12 of 20 |
| 2 | My helper, mother or spouse runs the kitchen, not me | before | 9 |
| 3 | I buy on Talabat, Noon or Breadfast, so there's no paper receipt | during | 6 |
| 4 | It won't read Arabic receipts properly | before | 6 |
| 5 | Another subscription, when the free version or WhatsApp is enough | before | 6 |
| 6 | The recipes are Western, not what we cook (v1) | before | 4 |
| 7 | I don't trust an app with my family's receipts and data | before | 4 |
| 8 | I pick dinner from a video on the day, I don't plan a week | during | 3 |
| 9 | I already track calories in another app | during | 2 |
| 10 | No Ramadan / iftar and suhoor planning | during | 2 |
| 11 | Why a $79 device when I have a phone? (v1) | before | 2 |
| 12 | The useful part (scanning) sits behind the paywall (v1) | before | 2 |
| 13 | It's a new habit with no proof it saves money | after | 2 |
| 14 | Nobody I know uses it | before | 1 |
| 15 | Market food has no receipt at all | during | 1 |

## Solutions, scored

Value to the buyer and cost to deliver, 1 to 5. Cost comes from the engineering effort and the CFO's per-household costs.

| solution | answers # | value | cost | keep? | status |
| --- | --- | ---: | ---: | --- | --- |
| Arab recipes up front (222 of 400 are Egyptian, Lebanese, Moroccan or Gulf) | 6 | 4 | 1 | **kept, done** | in catalogue; v1 pitch hid it. It removed objection 6 completely |
| Free tier with expiry reminders, shared list, recipes and a few scans | 5, 12 | 4 | 2 | **kept, done** | the free scan cap is new: build it |
| 30-day Plus trial with no card | 4, 13 | 3 | 1 | **kept** | store-native intro offer |
| Privacy line: never sold, delete everything in Settings | 7 | 2 | 1 | **kept, done** | account deletion already exists. It moved no one: trust is earned by people they know (#14) |
| **Helper mode**: invite a helper or spouse by link, simplest screens, their language, one-tap "put away" | 1, 2 | 5 | 3 | **build next** | households exist; the invite flow and a simplified role do not |
| **No-scan logging**: forward a delivery-app order email or paste a shared order, and Remi reads it | 1, 3 | 5 | 3 | **build next** | the receipt parser can read text; needs an inbox or paste flow |
| **Proof demo**: a 20-second video of a real Carrefour/Lulu Arabic receipt going in | 4 | 4 | 1 | **make now** | marketing asset, no code |
| Ramadan planner (iftar and suhoor templates) | 10 | 3 | 2 | seasonal bonus | catalogue already has the dishes |
| Save a recipe from an Instagram/TikTok link | 8 | 3 | 2 | later | link import exists for websites only |
| Countertop device | 11 | 2 | 5 | **cut from the offer** | the board and the panel agree |
| One-time "lifetime" price | 5 | 3 | 3 | test, don't promise | conflicts with AI running costs |

## The stack (v2, as tested)

- **The core:** "Never throw out food you forgot you had", in Arabic and English, for the whole household.
- **Bonus 1:** more than half the recipes are Arab home cooking. This answers objection 6.
- **Bonus 2:** one shared kitchen and list for your spouse or helper. This answers objections 2 and 1.
- **The guarantee:** 30 days of Plus free with no card. If it can't read your receipts, you've paid nothing. Cost: $0 cash; only the trial's hosting and AI, about $0.35 per trial household.
- **Real urgency:** the first 1,000 households keep $19.99 a year for as long as they stay. The cap is real and the CFO can afford it (see pricing).
- **The name:** Remi Plus. The offer could be called "the Founding Kitchen" for the first 1,000.

## Value equation (1 to 10), before → after

| element | v1 | v2 | what moved it |
| --- | ---: | ---: | --- |
| Dream outcome | 4 | 5 | "never throw out food you forgot" is clearer than a feature list |
| Perceived likelihood | 2 | 3 | Arab recipes and the free trial. Still low: nobody believes Arabic receipts work, and there's no proof yet |
| Time to result | 4 | 4 | unchanged. The first win needs a receipt scan |
| Effort and sacrifice | 2 | 2 | **unchanged, and it's the blocker.** Scanning every shop is homework. Helper mode and no-scan logging are the two fixes |

## The re-test

- **v1 → v2 buy rate: 0% → 5%** (0 → 1 of 20). Same seed and same cards.
- **Objections that dropped:** "Western recipes" fell from 4 mentions to 0, and the device dropped out entirely.
- **Objections that didn't move:** logging fatigue (11 → 12), trust (4 → 4) and delivery-app shoppers (still asking for Talabat/Noon import).
- **Reading:** as the skill warns, when nothing big moves, the problem is the product's effort cost and the market, not the price. The next panel should test the helper-mode and no-scan version. That pitch can't be written until those are built, because it would promise something that doesn't exist.
