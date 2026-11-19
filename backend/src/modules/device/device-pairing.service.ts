import { createHash, randomBytes, randomInt, timingSafeEqual } from 'node:crypto';
import { Injectable, NotFoundException } from '@nestjs/common';
import { CommandBus } from '@nestjs/cqrs';
import { IssueDeviceSessionCommand } from '@/modules/identity/application/commands/issue-device-session/issue-device-session.command.js';
import type { AuthTokens } from '@/modules/identity/domain/ports/token-signer.port.js';

const TTL_MS = 10 * 60_000;

interface Pairing {
  id: string;
  code: string;
  secretHash: Buffer;
  deviceName: string;
  expiresAt: number;
  result?: { tokens: AuthTokens; householdId: string };
}

const hash = (s: string) => createHash('sha256').update(s).digest();

/**
 * Pairing a Remi device with the app, like a TV app: the device shows a 6-digit code, the person types it in the
 * Remi app (signed in, in their household), and the device collects its own session by polling with its secret.
 * shortcut: pairings live in memory, so they don't survive a restart and need one API instance; move them to the
 * database (or Redis) before running several instances.
 */
@Injectable()
export class DevicePairingService {
  private readonly pairings = new Map<string, Pairing>();

  constructor(private readonly commands: CommandBus) {}

  start(deviceName: string) {
    this.sweep();
    let code: string;
    do code = String(randomInt(0, 1_000_000)).padStart(6, '0');
    while ([...this.pairings.values()].some((p) => p.code === code));
    const secret = randomBytes(24).toString('base64url');
    const pairing: Pairing = { id: randomBytes(12).toString('base64url'), code, secretHash: hash(secret), deviceName: deviceName.slice(0, 40) || 'Remi', expiresAt: Date.now() + TTL_MS };
    this.pairings.set(pairing.id, pairing);
    return { pairingId: pairing.id, code, secret, expiresInSeconds: TTL_MS / 1000 };
  }

  /** Called from the app by a signed-in household member. */
  async pair(code: string, userId: string, householdId: string) {
    this.sweep();
    const pairing = [...this.pairings.values()].find((p) => p.code === code.trim() && !p.result);
    if (!pairing) throw new NotFoundException('That code is not valid or has expired. Check the code on Remi and try again.');
    const tokens = await this.commands.execute(new IssueDeviceSessionCommand(userId, pairing.deviceName));
    pairing.result = { tokens, householdId };
    return { deviceName: pairing.deviceName };
  }

  /** Polled by the device. Hands over the session once, then forgets the pairing. */
  claim(pairingId: string, secret: string) {
    this.sweep();
    const pairing = this.pairings.get(pairingId);
    const given = hash(secret);
    if (!pairing || !timingSafeEqual(pairing.secretHash, given)) return { status: 'EXPIRED' as const, refreshToken: null, householdId: null };
    if (!pairing.result) return { status: 'WAITING' as const, refreshToken: null, householdId: null };
    this.pairings.delete(pairingId);
    return { status: 'PAIRED' as const, refreshToken: pairing.result.tokens.refreshToken, householdId: pairing.result.householdId };
  }

  private sweep() {
    const now = Date.now();
    for (const [id, p] of this.pairings) if (p.expiresAt < now) this.pairings.delete(id);
  }
}
