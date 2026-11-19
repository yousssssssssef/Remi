import { Inject } from '@nestjs/common';
import { CommandHandler, type ICommandHandler } from '@nestjs/cqrs';
import { AppConfigService } from '@/platform/config/app-config.service.js';
import { TransactionRunner } from '@/platform/db/transaction-runner.js';
import { newId } from '@/shared/ids.js';
import { SESSION_REPOSITORY, type SessionRepository } from '@/modules/identity/domain/ports/session.repository.js';
import { TOKEN_SIGNER, type AuthTokens, type TokenSignerPort } from '@/modules/identity/domain/ports/token-signer.port.js';
import { IssueDeviceSessionCommand } from './issue-device-session.command.js';

/** Same session shape as login (rotating refresh token, revocable from logout-all), labelled as a device. */
@CommandHandler(IssueDeviceSessionCommand)
export class IssueDeviceSessionHandler implements ICommandHandler<IssueDeviceSessionCommand> {
  constructor(
    private readonly tx: TransactionRunner,
    private readonly config: AppConfigService,
    @Inject(SESSION_REPOSITORY) private readonly sessions: SessionRepository,
    @Inject(TOKEN_SIGNER) private readonly signer: TokenSignerPort,
  ) {}

  async execute({ userId, deviceName }: IssueDeviceSessionCommand): Promise<AuthTokens> {
    const refresh = this.signer.createRefreshToken();
    const sessionId = newId();
    const expiresAt = new Date(Date.now() + this.config.env.REFRESH_TOKEN_TTL_DAYS * 86_400_000);
    await this.tx.run(
      (tx) =>
        this.sessions.insert(tx, {
          id: sessionId,
          userId,
          familyId: newId(),
          refreshTokenHash: refresh.hash,
          expiresAt,
          userAgent: `Remi device: ${deviceName}`.slice(0, 512),
        }),
      { userId },
    );
    const access = await this.signer.issueAccess(userId, sessionId);
    return { accessToken: access.accessToken, refreshToken: refresh.raw, expiresInSeconds: access.expiresInSeconds };
  }
}
