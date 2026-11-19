import { UseGuards } from '@nestjs/common';
import { Args, Field, Int, Mutation, ObjectType, Resolver, registerEnumType } from '@nestjs/graphql';
import { Throttle } from '@nestjs/throttler';
import { AUTH_THROTTLE } from '@/platform/graphql/throttle.js';
import { Ctx } from '@/platform/tenancy/ctx.decorator.js';
import { GqlAuthGuard } from '@/platform/tenancy/gql-auth.guard.js';
import { HouseholdGuard } from '@/platform/tenancy/household.guard.js';
import type { RequestContext } from '@/platform/tenancy/request-context.js';
import { DevicePairingService } from './device-pairing.service.js';

export enum DevicePairingStatus { WAITING = 'WAITING', PAIRED = 'PAIRED', EXPIRED = 'EXPIRED' }
registerEnumType(DevicePairingStatus, { name: 'DevicePairingStatus' });

@ObjectType()
export class DevicePairingType {
  @Field() pairingId!: string;
  @Field({ description: 'Six digits the device shows; the person types them in the Remi app.' }) code!: string;
  @Field({ description: 'Kept on the device; proves it when claiming the session.' }) secret!: string;
  @Field(() => Int) expiresInSeconds!: number;
}

@ObjectType()
export class DevicePairingClaimType {
  @Field(() => DevicePairingStatus) status!: DevicePairingStatus;
  @Field(() => String, { nullable: true, description: 'Set once, when status is PAIRED.' }) refreshToken!: string | null;
  @Field(() => String, { nullable: true }) householdId!: string | null;
}

@ObjectType()
export class PairedDeviceType {
  @Field() deviceName!: string;
}

/** Resolvers only translate GraphQL <-> the pairing service. */
@Resolver()
export class DeviceResolver {
  constructor(private readonly pairing: DevicePairingService) {}

  @Throttle(AUTH_THROTTLE)
  @Mutation(() => DevicePairingType, { description: 'Called by a Remi device to get a pairing code. Public.' })
  startDevicePairing(@Args('deviceName', { type: () => String, defaultValue: 'Remi' }) deviceName: string) {
    return this.pairing.start(deviceName);
  }

  @Throttle(AUTH_THROTTLE)
  @Mutation(() => DevicePairingClaimType, { description: 'Polled by the device until the code is entered in the app. Public.' })
  claimDevicePairing(@Args('pairingId') pairingId: string, @Args('secret') secret: string) {
    return this.pairing.claim(pairingId, secret);
  }

  @Throttle(AUTH_THROTTLE)
  @UseGuards(GqlAuthGuard, HouseholdGuard)
  @Mutation(() => PairedDeviceType, { description: 'Called from the Remi app: links the device showing this code to the household.' })
  pairDevice(@Ctx() ctx: RequestContext, @Args('code') code: string) {
    return this.pairing.pair(code, ctx.userId, ctx.householdId!);
  }
}

