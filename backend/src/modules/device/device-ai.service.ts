import Anthropic from '@anthropic-ai/sdk';
import { zodOutputFormat } from '@anthropic-ai/sdk/helpers/zod';
import { Injectable, Logger, ServiceUnavailableException } from '@nestjs/common';
import { z } from 'zod';
import { TransactionRunner } from '@/platform/db/transaction-runner.js';
import { InventoryLedgerService } from '@/modules/inventory/application/inventory-ledger.service.js';
import { ReceiptService } from '@/modules/receipts/application/receipt.service.js';
import { RecipeSuggestionsService } from '@/modules/recipes/application/recipe-suggestions.service.js';
import { ShoppingService } from '@/modules/shopping/application/shopping.service.js';

const MODEL = 'claude-opus-5-5';
// Opus 5.5 can decline a request on safety grounds; the default fallback re-runs it on a model chosen by category.
const FALLBACK = { betas: ['server-side-fallback-2026-07-01'], fallbacks: 'default' as const };

export type ScanMode = 'barcode' | 'label' | 'list' | 'receipt';
export const SCAN_MODES: readonly ScanMode[] = ['barcode', 'label', 'list', 'receipt'];
const UNITS = ['PIECE', 'GRAM', 'KILOGRAM', 'LITER', 'MILLILITER', 'CUP', 'TABLESPOON', 'TEASPOON'] as const;

const ScanSchema = z.object({
  title: z.string().describe('A short heading for a 128 px wide screen, at most 30 characters'),
  barcode: z.string().nullable().describe('Digits of a barcode if one is visible, else null'),
  facts: z.array(z.string()).describe('Up to 5 short lines to show, each at most 32 characters'),
  items: z
    .array(z.object({ name: z.string(), amount: z.number().nullable(), unit: z.enum([...UNITS, 'UNKNOWN']) }))
    .describe('Foods found: the product, the listed groceries, or the food lines of the receipt'),
});

const SCAN_PROMPTS: Record<ScanMode, string> = {
  barcode:
    'This photo shows a grocery product. Read the barcode digits if visible. Identify the product (brand, name, size). ' +
    'facts: size, then calories and protein per 100 g if printed. items: the product once, with its package amount and unit.',
  label:
    'This photo shows a nutrition facts label. title: "Per 100 g" or the serving it states. ' +
    'facts: energy in kcal, protein, carbs (with sugar), fat, salt or sodium, each on one short line. items: empty.',
  list:
    'This photo shows a handwritten or printed shopping list. title: "Shopping list: N items". ' +
    'facts: the first five items as written. items: every food on the list, with amount and unit when written, else null and UNKNOWN.',
  receipt:
    'This photo shows a grocery receipt. Ignore the shop header, totals, tax, payment, bags, fees and non-food goods. ' +
    'title: "Receipt: N foods". facts: the first five food lines with amount. items: every food line, with amount and unit when printed.',
};

export interface ScanItem { foodId: string; name: string; amount: string; unit: string }
export interface ScanResult { title: string; lines: string[]; action: 'kitchen' | 'list' | 'none'; items: ScanItem[] }

// Device screens draw plain ASCII; keep replies short and clean.
const ascii = (s: string, max: number) =>
  s.replace(/[‘’]/g, "'").replace(/[“”]/g, '"').replace(/[–—]/g, '-').replace(/[^\x20-\x7E]/g, '').slice(0, max);

@Injectable()
export class DeviceAiService {
  private readonly log = new Logger(DeviceAiService.name);
  private anthropic?: Anthropic;
  // Created on first use so the API starts without Claude credentials (ANTHROPIC_API_KEY, ANTHROPIC_AUTH_TOKEN or an ant profile).
  private get client(): Anthropic {
    return (this.anthropic ??= new Anthropic());
  }
  private readonly recent = new Map<string, number[]>();

  constructor(
    private readonly transactions: TransactionRunner,
    private readonly inventory: InventoryLedgerService,
    private readonly shopping: ShoppingService,
    private readonly suggestions: RecipeSuggestionsService,
    private readonly receipts: ReceiptService,
  ) {}

  /** A spoken-style answer about this household's kitchen, at most two short sentences. */
  async ask(householdId: string, userId: string, question: string): Promise<string> {
    this.rateLimit(householdId);
    const context = await this.householdContext(householdId, userId);
    const response = await this.call(() =>
      this.client.beta.messages.create({
        model: MODEL,
        max_tokens: 2048,
        ...FALLBACK,
        output_config: { effort: 'low' },
        system: [
          {
            type: 'text',
            text:
              "You are Remi, a cheerful kitchen buddy who lives on a small screen on the household's fridge. " +
              'Answer in at most two short sentences (under 220 characters), as if speaking. Plain text only: no markdown, ' +
              'lists or emoji. Use the household data below; when it does not answer the question, say so briefly and suggest ' +
              'what the person could do in the Remi app.',
          },
          { type: 'text', text: context },
        ],
        messages: [{ role: 'user', content: question }],
      }),
    );
    if (response.stop_reason === 'refusal') return "I can't help with that one, but ask me about your kitchen!";
    const text = response.content.flatMap((b) => (b.type === 'text' ? [b.text] : [])).join(' ').trim();
    return ascii(text || "I'm not sure. Try asking me what you can cook.", 280);
  }

  /** Reads a photo (barcode, nutrition label, shopping list or receipt) and matches the foods to the catalogue. */
  async scan(householdId: string, userId: string, mode: ScanMode, image: Buffer, mediaType: 'image/jpeg' | 'image/png'): Promise<ScanResult> {
    this.rateLimit(householdId);
    const response = await this.call(() =>
      this.client.beta.messages.parse({
        model: MODEL,
        max_tokens: 4096,
        ...FALLBACK,
        output_config: { effort: 'low', format: zodOutputFormat(ScanSchema) },
        messages: [
          {
            role: 'user',
            content: [
              { type: 'image', source: { type: 'base64', media_type: mediaType, data: image.toString('base64') } },
              { type: 'text', text: SCAN_PROMPTS[mode] },
            ],
          },
        ],
      }),
    );
    const parsed = response.stop_reason === 'refusal' ? null : response.parsed_output;
    if (!parsed) return { title: "Couldn't read that", lines: ['Try a closer, brighter photo'], action: 'none', items: [] };

    const items: ScanItem[] = [];
    if (mode !== 'label') {
      const byBarcode = parsed.barcode ? await this.productByBarcode(parsed.barcode) : null;
      const names = parsed.items.map((i) => i.name).slice(0, 40);
      const matches = names.length ? await this.receipts.match(householdId, userId, names) : [];
      parsed.items.slice(0, 40).forEach((item, i) => {
        const food = i === 0 && byBarcode ? byBarcode : matches[i]?.confidence !== 'NONE' ? matches[i]?.food : null;
        if (!food) return;
        const unit = item.unit !== 'UNKNOWN' ? item.unit : food.defaultUnit;
        const amount = item.amount && item.amount > 0 ? item.amount : 1;
        items.push({ foodId: food.foodId, name: ascii(food.name, 40), amount: amount.toFixed(3), unit });
      });
    }
    return {
      title: ascii(parsed.title, 40),
      lines: parsed.facts.slice(0, 5).map((f) => ascii(f, 40)),
      action: mode === 'list' ? 'list' : mode === 'label' || !items.length ? 'none' : 'kitchen',
      items,
    };
  }

  private async productByBarcode(barcode: string) {
    const digits = barcode.replace(/\D/g, '');
    if (digits.length < 8) return null;
    const rows = await this.transactions.run((tx) =>
      tx.$queryRaw<Array<{ foodId: string; name: string; defaultUnit: string }>>`
        SELECT p."foodId", COALESCE(p."brand" || ' ', '') || p."name" AS "name", f."defaultUnit"::text AS "defaultUnit"
        FROM "Product" p JOIN "Food" f ON f."id" = p."foodId"
        WHERE p."barcode" = ${digits} AND p."archivedAt" IS NULL LIMIT 1`,
    );
    return rows[0] ?? null;
  }

  // Compact text snapshot: what's in the kitchen (soonest expiry first), the shopping list, and what can be cooked.
  private async householdContext(householdId: string, userId: string): Promise<string> {
    const [stock, lists, ideas] = await Promise.all([
      this.inventory.list(householdId, 50),
      this.shopping.shoppingLists(householdId),
      this.suggestions.list(householdId, userId, 5).catch(() => ({ nodes: [] })),
    ]);
    const now = Date.now();
    const kitchen = (stock.nodes as Array<Record<string, unknown>>)
      .map((n) => {
        const exp = n.estimatedExpiryAt ? Math.round((new Date(String(n.estimatedExpiryAt)).getTime() - now) / 86_400_000) : null;
        return { line: `${n.foodName}: ${Number(n.quantity)} ${String(n.unit).toLowerCase()}${exp === null ? '' : `, expires in ${exp} days`}`, exp };
      })
      .sort((a, b) => (a.exp ?? 9999) - (b.exp ?? 9999))
      .map((k) => k.line);
    const needed = lists
      .filter((l) => l.status === 'ACTIVE')
      .flatMap((l) => (l.items as Array<{ foodName: string; status: string }>).filter((i) => i.status === 'NEEDED').map((i) => i.foodName));
    const cook = ideas.nodes.map((s) => `${s.recipe.name} (${s.haveCount} of ${s.requiredCount} ingredients in the kitchen)`);
    return [
      `Today is ${new Date().toDateString()}.`,
      `Kitchen (${kitchen.length} items, soonest expiry first):\n${kitchen.join('\n') || 'empty'}`,
      `Shopping list: ${needed.join(', ') || 'empty'}`,
      `Recipes they can cook now: ${cook.join('; ') || 'none suggested'}`,
    ].join('\n\n');
  }

  private rateLimit(householdId: string) {  // the device can be poked repeatedly; keep model spend bounded
    const now = Date.now(), window = 10 * 60_000, max = 40;
    const hits = (this.recent.get(householdId) ?? []).filter((t) => now - t < window);
    if (hits.length >= max) throw new ServiceUnavailableException('Remi needs a short break. Try again in a few minutes.');
    hits.push(now);
    this.recent.set(householdId, hits);
  }

  private async call<T>(fn: () => Promise<T>): Promise<T> {
    try {
      return await fn();
    } catch (error) {
      if (error instanceof Anthropic.AuthenticationError || error instanceof Anthropic.PermissionDeniedError) {
        this.log.error('Claude API credentials are missing or invalid; set ANTHROPIC_API_KEY');
        throw new ServiceUnavailableException('Remi AI is not configured on the server.');
      }
      if (error instanceof Anthropic.RateLimitError) throw new ServiceUnavailableException('Remi is busy. Try again soon.');
      if (error instanceof Anthropic.APIError) {
        this.log.error(`Claude API error ${error.status}: ${error.message}`);
        throw new ServiceUnavailableException('Remi AI is unavailable right now.');
      }
      if (error instanceof Anthropic.AnthropicError) {  // e.g. no credentials found when the client was created
        this.log.error(`Claude client error: ${error.message}`);
        throw new ServiceUnavailableException('Remi AI is not configured on the server.');
      }
      throw error;
    }
  }
}
