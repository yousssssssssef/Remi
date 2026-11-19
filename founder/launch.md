# Launch: Remi

**Bottom line: test with 30 real households before launching to the public.** The simulated panel can't answer the one question that matters: do real households keep their kitchen up to date after four weeks? A paid founding-household offer answers whether they'll pay.

Dates assume today, 19 November 2026, and a team working part time. Ramadan 2027 starts around 8 February 2027 (an estimate: confirm the date nearer the time). That gives a natural launch moment for an iftar planner, and a deadline for the test.

## Step 1: the real test (before any spend)

**Pilot: 30 households for 4 weeks, then a founding-household pre-sale.**

| | |
| --- | --- |
| Who | 30 Arabic-speaking households that cook at home. Start with Egyptian families (in Cairo, and Egyptians living in the Gulf), because 190 of the 400 recipes are Egyptian |
| How to find them | Classmates' families, a university cooking or family group, one or two local food creators, and Facebook groups for Egyptian mums in Dubai and Riyadh (with the admins' permission) |
| What they get | Plus free for the pilot; the helper/spouse invite if it's built in time |
| Success line, set now | **At least 40%** of households still log something in week 4. **At least 60%** get a "use first" list in their first session. **At least 8 of 30** pay the $19.99 founding price at the end |
| Panel comparison | The panel said 1 of 20 (5%), and only for the free tier. If real households beat the success line, trust them over the panel |
| If it fails | Week-4 logging under 25%: go back to the product (helper mode, no-scan logging) before any marketing. Pay rate under 10%: go back to `/founder-offer` and `/founder-pricing` |

## Step 2: the countdown

The critical path is marked ★. If a ★ task slips, the launch date moves.

| week of | task | owner | deadline |
| --- | --- | --- | --- |
| 23 Nov 2026 | ★ Test 20 real Arabic receipts (Carrefour, Lulu, Panda, Hyper One, Spinneys); write down the match rate; fix the top misses | dev + catalogue | 29 Nov |
| 23 Nov | Free scan cap and paywall with a 30-day no-card trial (RevenueCat or store intro offer) | dev | 6 Dec |
| 30 Nov | ★ Helper/spouse invite by WhatsApp link with a simple role (put away, used, threw out) | dev | 13 Dec |
| 30 Nov | Recruit the 30 pilot households; collect consent; set up a WhatsApp support group | outreach | 6 Dec |
| 7 Dec | Register a company or sole trader for store payouts; enrol in Apple's Small Business Program | founder | 20 Dec |
| 7 Dec | Expert review of allergen tags; label expiry dates as estimates | catalogue | 31 Dec |
| 14 Dec | ★ **Pilot starts** (TestFlight and Play internal testing) | all | 14 Dec |
| 14 Dec → 10 Jan | Weekly check-in with each pilot household (5 questions, 10 minutes) | founder | weekly |
| 21 Dec | Paste a delivery order (Talabat/Noon/Breadfast text) → kitchen | dev | 3 Jan 2027 |
| 4 Jan 2027 | Add 30–40 Ramadan and Gulf dishes; build the iftar/suhoor planner template | catalogue | 24 Jan |
| 11 Jan | ★ **Pilot readout**: compare against the success line; decide go or fix | founder | 13 Jan |
| 11 Jan | Founding-household pre-sale to the pilot and its waitlist ($19.99 a year, first 1,000) | founder | 24 Jan |
| 18 Jan | Store listing in Arabic and English, screenshots, the 20-second receipt video; submit for review | brand + dev | 25 Jan |
| 25 Jan | Marketing calendar starts (launch minus 14 days; see marketing.md) | outreach | 25 Jan |
| 1 Feb | Soft launch: Egypt store plus UAE and Saudi Arabia, no ads | all | 1 Feb |
| **8 Feb 2027** | **Public launch, timed to the start of Ramadan** (date to confirm) | all | 8 Feb |

## Step 3: launch day run sheet (8 Feb 2027, Cairo time)

| time | what | who |
| --- | --- | --- |
| 08:00 | Check that the store listings are live in EG, AE and SA; do a smoke test (sign up, scan, invite, pay) | dev |
| 09:00 | Launch post on Instagram, TikTok and Facebook in Arabic: the receipt video and the founding price | outreach |
| 10:00 | Personal message to every pilot household and the waitlist with the founding link | founder |
| 12:00 | Creator posts go live (disclosed as paid or gifted) | outreach |
| all day | Reply to every review and DM within 2 hours; watch crash and scan-failure reports | founder, dev |
| 17:30 | Iftar-planner post ("tonight's iftar from what's in your kitchen") | outreach |
| 22:00 | Day-one numbers: installs, sign-ups, first scans, invites sent, trials started | founder |
| If something breaks | Store rejection: ship the fix and keep the listing up. Scan outage: pin a status post and extend trials by a week | dev |

## Step 4: the first 30 days

Track these weekly. If a number falls below its trigger, change something.

| number | target | trigger |
| --- | --- | --- |
| Week-4 logging retention (households still adding or using food) | 40% | < 25% → stop paid marketing, fix effort |
| Households with 2 or more members (helper or spouse invited) | 35% | < 15% → rework the invite |
| Trial → paid | 10% | < 5% → revisit offer and price |
| Paying households (CFO break-even: 54 at $2.99) | 54 by month 4 | behind the ramp 2 months running → re-run the CFO |

Reviews on day 7, 14 and 30. Questions for each: Which households stopped, and why (ask them)? Which shop's receipts fail most? Did the helper invite get used? What did people say in their own words? Is the device still parked? (It should be.)
