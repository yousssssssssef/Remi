import { type CanActivate, type ExecutionContext, ForbiddenException, Injectable, UnauthorizedException } from '@nestjs/common';
import { QueryBus } from '@nestjs/cqrs';
import { GetActiveMembershipQuery } from '@/platform/tenancy/get-active-membership.query.js';
import { IsSessionActiveQuery } from '@/platform/tenancy/is-session-active.query.js';
import { JwtTokensService } from '@/platform/tenancy/jwt-tokens.service.js';
import type { RequestContext } from '@/platform/tenancy/request-context.js';

const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

/** GqlAuthGuard + HouseholdGuard for the device's plain HTTP endpoints (photo uploads aren't GraphQL). */
@Injectable()
export class DeviceHttpAuthGuard implements CanActivate {
  constructor(
    private readonly tokens: JwtTokensService,
    private readonly queryBus: QueryBus,
  ) {}

  async canActivate(context: ExecutionContext): Promise<boolean> {
    const req = context.switchToHttp().getRequest<{ headers: Record<string, string | undefined>; ctx?: RequestContext }>();
    const header = req.headers.authorization;
    const raw = header?.startsWith('Bearer ') ? header.slice(7) : undefined;
    if (!raw) throw new UnauthorizedException('Missing bearer token');
    let userId: string, sessionId: string;
    try {
      const payload = await this.tokens.verifyAccess(raw);
      userId = payload.sub;
      sessionId = payload.sid;
    } catch {
      throw new UnauthorizedException('Invalid or expired token');
    }
    if (!(await this.queryBus.execute(new IsSessionActiveQuery(sessionId, userId)))) throw new UnauthorizedException('Session revoked or expired');
    const householdId = req.headers['x-household-id'];
    if (!householdId || !UUID.test(householdId)) throw new ForbiddenException('x-household-id header is required');
    const membership = await this.queryBus.execute(new GetActiveMembershipQuery(userId, householdId));
    if (!membership) throw new ForbiddenException('Not an active member of this household');
    req.ctx = { userId, sessionId, householdId, householdRole: membership.role } satisfies RequestContext;
    return true;
  }
}
