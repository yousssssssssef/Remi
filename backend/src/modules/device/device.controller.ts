import { BadRequestException, Body, Controller, HttpCode, Post, Query, Req, UseGuards } from '@nestjs/common';
import { Throttle } from '@nestjs/throttler';
import { IsString, MaxLength, MinLength } from 'class-validator';
import { UPLOAD_THROTTLE } from '@/platform/graphql/throttle.js';
import type { RequestContext } from '@/platform/tenancy/request-context.js';
import { DeviceAiService, SCAN_MODES, type ScanMode } from './device-ai.service.js';
import { DeviceHttpAuthGuard } from './device-http-auth.guard.js';

class AskBody {
  @IsString() @MinLength(1) @MaxLength(300) question!: string;
}

type DeviceRequest = { ctx: RequestContext; body: unknown; headers: Record<string, string | undefined> };

/**
 * Endpoints for the Remi kitchen device (hardware/). Same bearer token and x-household-id header as GraphQL.
 *   POST /device/ask              {"question": "..."}           -> {"reply": "..."}
 *   POST /device/scan?mode=barcode|label|list|receipt  (body: JPEG or PNG bytes)
 *        -> {"title", "lines": [...], "action": "kitchen"|"list"|"none", "items": [{foodId, name, amount, unit}]}
 * The device applies a scan's items with the existing addStock / addToShoppingList mutations.
 */
@Controller('device')
@UseGuards(DeviceHttpAuthGuard)
@Throttle(UPLOAD_THROTTLE)
export class DeviceController {
  constructor(private readonly ai: DeviceAiService) {}

  @Post('ask')
  @HttpCode(200)
  async ask(@Req() req: DeviceRequest, @Body() body: AskBody) {
    return { reply: await this.ai.ask(req.ctx.householdId!, req.ctx.userId, body.question) };
  }

  @Post('scan')
  @HttpCode(200)
  scan(@Req() req: DeviceRequest, @Query('mode') mode: string) {
    if (!SCAN_MODES.includes(mode as ScanMode)) throw new BadRequestException(`mode must be one of ${SCAN_MODES.join(', ')}`);
    const type = req.headers['content-type'];
    if (type !== 'image/jpeg' && type !== 'image/png') throw new BadRequestException('Send the photo as image/jpeg or image/png');
    if (!Buffer.isBuffer(req.body) || req.body.length < 100) throw new BadRequestException('The photo is empty');
    return this.ai.scan(req.ctx.householdId!, req.ctx.userId, mode as ScanMode, req.body, type);
  }
}
