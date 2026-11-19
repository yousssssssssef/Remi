# CFO's note

**How to read the table:** the tool counts in "units a day". Here one unit is a paying household for one month and `days_per_month` is 1, so "200 a day" means **200 paying households that month**. Price: **$2.99 a month** (from `pricing.md`).

- **The margin.** Each paying household leaves **$2.19 a month** (73%) after the 15% store fee, AI and hosting. Running costs are about $108 a month, and break-even is **50 paying households**.
- **The line to watch: founder time, then the ramp.** Year 1 makes **$782** only because the team is paid **$0**. Pay one founder $1,000 a month (what-if) and break-even jumps to **507 paying households**, year 1 loses **$11,218**, and the business needs **$11,842** in cash. The ramp (5 → 180 paying households) is an estimate. On the offer re-test, 1 of 20 simulated buyers said yes, and only to the free tier. At 30% of the ramp, year 1 loses $675.
- **Cash.** As an unpaid student project it needs about **$924** before it pays for itself (developer accounts, a small test, an expert data review). The startup money is earned back in month 12.
- **Three ways to improve the margin:**
  1. **More paying households, not a different price.** Every 100 paying households adds about $219 a month.
  2. **Default to the yearly plan** ($24.99). Cash comes in up front and churn falls; confirm with real retention.
  3. **Don't drop to $1.99.** That what-if moves break-even to 91 and year 1 to **-$168**.
- **The board's money conditions:**
  - AI cost per household: **estimated, not measured.**
  - Device unit cost: **left out on purpose.**
  - A tested channel and a cost per household: **not met.**

An accountant should check the company structure, store payouts, VAT and tax before money moves. Not financial advice.

# Unit economics: Remi Plus (household subscription, app only; device excluded until it has a unit cost)

Every number below comes from the input file. Nothing is looked up or guessed.

## One paying household-month

| line | per paying household-month |
| --- | ---: |
| Price | $2.99 |
| App Store / Google Play fee (15%) | -$0.45 |
| AI calls for Ask Remi and label/receipt fallback (estimate) | -$0.20 |
| Hosting, storage and push per active household (estimate) | -$0.15 |
| **Contribution** (what each paying household-month leaves to pay the fixed costs) | **$2.19** (73%) |

## The margin that matters

Fixed costs: $108 a month (Base cloud: API server, Postgres, object storage, email (estimate) $60, Free-tier users' hosting and AI (estimate) $30, Apple Developer Program ($99 a year) $8, Domain, transactional email, misc tools (estimate) $10, Founder salaries (student team, unpaid in year 1) $0).

- **Break-even: 50 paying household-months a day.** Below that you lose money every month.
- **Profit margin at your plan** (200 a day): **55%** of every sale, after every cost.

## Year 1, month by month

| month | paying household-months a day | revenue | profit | cumulative (after $624 startup) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 5 | $15 | -$97 | -$721 |
| 2 | 10 | $30 | -$86 | -$808 |
| 3 | 20 | $60 | -$64 | -$872 |
| 4 | 30 | $90 | -$43 | -$915 |
| 5 | 45 | $135 | -$10 | -$924 |
| 6 | 60 | $179 | $23 | -$901 |
| 7 | 80 | $239 | $67 | -$834 |
| 8 | 100 | $299 | $111 | -$723 |
| 9 | 120 | $359 | $155 | -$569 |
| 10 | 140 | $419 | $198 | -$371 |
| 11 | 160 | $478 | $242 | -$128 |
| 12 | 180 | $538 | $286 | $158 |

- **Year 1 operating profit: $782** on $2,840 of revenue.
- After the $624 startup spend: $158.
- Startup money earned back: month 12.
- Cash you need before it pays for itself: **$924**.

## What if

| scenario | margin at plan | break-even a day | year 1 profit |
| --- | ---: | ---: | ---: |
| Base plan | 55% | 50 | $782 |
| Price -10% | 50% | 58 | $497 |
| Volume -20% | 51% | 50 | $365 |
| Unit costs +15% | 51% | 53 | $668 |

No red flags in these numbers. They are only as good as the inputs: check every cost against a real quote.
