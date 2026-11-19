import { Command } from '@nestjs/cqrs';
import type { AuthTokens } from '@/modules/identity/domain/ports/token-signer.port.js';

/** A new session for a Remi kitchen device the user just paired from the app (no password involved). */
export class IssueDeviceSessionCommand extends Command<AuthTokens> {
  constructor(
    public readonly userId: string,
    public readonly deviceName: string,
  ) {
    super();
  }
}
