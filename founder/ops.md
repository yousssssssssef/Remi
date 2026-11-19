# Operations: Remi (app first, device parked)

**Bottom line: running Remi day to day costs about $108 a month and a few hours a week. The real operational risk isn't cost. It's catalogue accuracy (allergens, expiry) and keeping receipt matching right as new shops and products appear.**

## Daily and weekly cycle

| step | customer sees it? | who | how often |
| --- | --- | --- | --- |
| Check crash reports, failed scans and API errors | no | dev on call | daily, 10 min |
| Answer support email and store reviews (Arabic and English) | **yes** | founder | daily, 20 min |
| Review unmatched receipt lines; add aliases and products to the catalogue | no (better matches next week) | catalogue owner | weekly, 1–2 h |
| Push expiry reminders and the weekly "use first" nudge | **yes** | automatic (background jobs) | daily |
| Look at the weekly numbers: week-4 logging retention, scans per household, trial-to-paid | no | founder | weekly |
| Ship a release (EAS build, store review) | **yes** | dev | every 2 weeks |
| Back up the database and test a restore | no | dev | weekly backup, monthly restore test |

## Suppliers (software)

Prices from the vendors' own public pages, or a quote to get. Check each before you commit.

| need | options | cost | note |
| --- | --- | --- | --- |
| App stores | Apple Developer Program; Google Play Console | $99/yr; $25 once | Apple's 15% Small Business Program and Google's 15% tier both need enrolling |
| Subscriptions and paywall | RevenueCat, or the store APIs directly | RevenueCat is free below its revenue threshold (check its pricing page) | handles trials, intro offers and the founding price |
| API and Postgres hosting | Railway, Render, Fly.io, Supabase, or a single VPS (Hetzner) | about $20–60/mo at this scale (**get a quote**) | Postgres needs RLS and backups; the README requires point-in-time recovery testing |
| Object storage (receipt photos, images) | Cloudflare R2, AWS S3 | a few dollars a month | delete receipt photos after parsing to cut cost and privacy risk |
| Email | Postmark, Resend, Amazon SES | free–$15/mo | verification and password reset |
| Push | FCM and APNs (already wired) | free | — |
| AI (Ask Remi, fallbacks) | Claude Haiku-class model for short answers | **measure it** | the CFO assumes $0.20 per paying household a month |
| Expert review of allergen and nutrition data | a registered dietitian, freelance | **open quote** (CFO estimate $300) | required before claiming allergen safety |

## People

| role | who | hours a week, slow first month → steady | pay |
| --- | --- | --- | --- |
| Product and support lead | founder | 8 → 10 | $0 (year 1, student) |
| Mobile and backend dev | team member | 10 → 6 | $0 |
| Catalogue and Arabic content | team member | 4 → 3 | $0 |
| Community and creator outreach | team member | 4 → 6 | $0 |

Unpaid time is the hidden cost. The CFO shows that paying one person $1,000 a month moves break-even from 36 to 365 paying households. An accountant should confirm the structure and any payroll rules before anyone is paid.

## Routines (one page each)

1. **A receipt that didn't match.** Open the unmatched-lines report. For each line, add an alias (Arabic and English) or a product. Re-run the match on the stored lines. Note the shop name and add it to the shop list.
2. **A complaint (store review or email).** Reply within 24 hours in the language they used. Say what happened in one line and what you'll do. Never ask for a better rating. Log it in the risk register if it repeats.
3. **A release.** Run the tests (157 backend, 441 app). Do the phone smoke test (docs/phone-testing.md): sign up, scan a receipt, cook a meal, delete an account. Use a staged rollout at 10% and watch crashes for 24 hours.
4. **Weekly numbers.** Use the four numbers in `launch.md`. If week-4 logging retention falls below 25%, stop marketing and fix effort first.
5. **Data deletion request.** Deletion runs from Settings. If the request comes by email, verify it's the account owner, delete, and confirm in writing.

## Tools (smallest stack)

Expo EAS for builds, RevenueCat for subscriptions, Sentry (free tier) for crashes, PostHog or Expo's analytics (free tier) for retention, Google Workspace or Zoho for support email, and a spreadsheet for the weekly numbers.

## Licences, permits, compliance (check each with the authority; not legal advice)

- **Company or sole-trader registration** in the founders' country, so you can receive store payouts. Apple and Google need a legal entity or individual with tax details.
- **Tax on digital services**: Apple and Google collect and remit VAT in most MENA markets they sell in, including the UAE, Saudi Arabia and Egypt. Confirm for each country.
- **Data protection**: Egypt's Personal Data Protection Law (Law 151 of 2020), the UAE's PDPL (Federal Decree-Law 45 of 2021), and Saudi Arabia's PDPL (SDAIA). The privacy policy and where data is stored have to match. Receipt photos are personal data.
- **Health and nutrition claims**: don't make medical or weight-loss claims. Allergen tags need expert review before they're shown as safe.

## Risk register

| risk | likelihood | impact | plan |
| --- | --- | --- | --- |
| Households stop logging after 2–3 weeks | **high** | **high** | helper mode, no-scan logging, weekly "use first" nudge; measure week-4 retention before any spend |
| Arabic receipt matching fails on a big chain | high | high | test 20 real receipts from Carrefour, Lulu, Panda, Hyper One, Spinneys before launch; weekly alias routine |
| A wrong allergen or expiry estimate harms someone | low | **severe** | expert review; label estimates as estimates; no "safe to eat" wording |
| Store rejects the build or the paywall | medium | medium | follow the store's guidelines for subscriptions and trials; keep account deletion visible |
| AI costs spike from abuse of Ask Remi | low | medium | per-household rate limit; cache answers |
| Server or database outage | medium | medium | offline reading already exists; weekly restore test |
| Key team member leaves after graduation | **high** | high | docs are strong already; give each of the 4 roles a written owner and a backup |
| A large app (grocery delivery, Samsung Food) adds Arabic inventory | medium | high | win on Arab catalogue depth and household/helper features; move fast in one segment |
| Privacy complaint or data leak | low | severe | RLS and household isolation exist; delete receipt photos after parsing; access review |
| Founders run out of time (exams, jobs) | high | high | narrow scope to the one promise; park the device |
