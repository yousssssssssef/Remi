import { BadRequestException, NotFoundException } from '@nestjs/common';
import { describe, expect, it, vi } from 'vitest';
import { DeviceController } from '@/modules/device/device.controller.js';
import { DevicePairingService } from '@/modules/device/device-pairing.service.js';

const tokens = { accessToken: 'a', refreshToken: 'r', expiresInSeconds: 900 };

describe('device pairing', () => {
  it('hands the session to the device once, after the app enters the code', async () => {
    const commands = { execute: vi.fn().mockResolvedValue(tokens) };
    const service = new DevicePairingService(commands as never);
    const started = service.start('Kitchen Remi');
    expect(started.code).toMatch(/^\d{6}$/);

    expect(service.claim(started.pairingId, started.secret).status).toBe('WAITING');
    await expect(service.pair(started.code, 'user-1', 'household-1')).resolves.toEqual({ deviceName: 'Kitchen Remi' });
    expect(commands.execute).toHaveBeenCalledTimes(1);

    expect(service.claim(started.pairingId, 'wrong-secret').status).toBe('EXPIRED');
    expect(service.claim(started.pairingId, started.secret)).toEqual({ status: 'PAIRED', refreshToken: 'r', householdId: 'household-1' });
    expect(service.claim(started.pairingId, started.secret).status).toBe('EXPIRED');  // only once
  });

  it('rejects unknown codes and codes already used', async () => {
    const service = new DevicePairingService({ execute: vi.fn().mockResolvedValue(tokens) } as never);
    await expect(service.pair('000000', 'u', 'h')).rejects.toThrow(NotFoundException);
    const { code } = service.start('Remi');
    await service.pair(code, 'u', 'h');
    await expect(service.pair(code, 'u', 'h')).rejects.toThrow(NotFoundException);
  });

  it('expires pairings after ten minutes', () => {
    vi.useFakeTimers();
    const service = new DevicePairingService({ execute: vi.fn() } as never);
    const { pairingId, secret } = service.start('Remi');
    vi.advanceTimersByTime(10 * 60_000 + 1);
    expect(service.claim(pairingId, secret).status).toBe('EXPIRED');
    vi.useRealTimers();
  });
});

describe('device scan endpoint', () => {
  const req = (headers: Record<string, string>, body: unknown) => ({ ctx: { userId: 'u', householdId: 'h' }, headers, body });

  it('validates the mode and the photo before calling the model', () => {
    const ai = { scan: vi.fn().mockResolvedValue({ title: 'ok' }) };
    const controller = new DeviceController(ai as never);
    expect(() => controller.scan(req({ 'content-type': 'image/jpeg' }, Buffer.alloc(500)) as never, 'selfie')).toThrow(BadRequestException);
    expect(() => controller.scan(req({ 'content-type': 'text/plain' }, Buffer.alloc(500)) as never, 'barcode')).toThrow(BadRequestException);
    expect(() => controller.scan(req({ 'content-type': 'image/jpeg' }, Buffer.alloc(10)) as never, 'barcode')).toThrow(BadRequestException);
    expect(ai.scan).not.toHaveBeenCalled();
    controller.scan(req({ 'content-type': 'image/jpeg' }, Buffer.alloc(500)) as never, 'receipt');
    expect(ai.scan).toHaveBeenCalledWith('h', 'u', 'receipt', expect.any(Buffer), 'image/jpeg');
  });
});
