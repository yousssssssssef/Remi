import { Module } from '@nestjs/common';
import { InventoryModule } from '@/modules/inventory/inventory.module.js';
import { ReceiptsModule } from '@/modules/receipts/receipts.module.js';
import { RecipesModule } from '@/modules/recipes/recipes.module.js';
import { ShoppingModule } from '@/modules/shopping/shopping.module.js';
import { DeviceAiService } from './device-ai.service.js';
import { DeviceController } from './device.controller.js';
import { DeviceHttpAuthGuard } from './device-http-auth.guard.js';
import { DevicePairingService } from './device-pairing.service.js';
import { DeviceResolver } from './device.resolver.js';

/** The Remi kitchen device: AI replies, photo scanning and pairing with the app. */
@Module({
  imports: [InventoryModule, ShoppingModule, RecipesModule, ReceiptsModule],
  controllers: [DeviceController],
  providers: [DeviceAiService, DevicePairingService, DeviceResolver, DeviceHttpAuthGuard],
})
export class DeviceModule {}
