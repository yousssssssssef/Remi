#pragma once
#include <Arduino.h>

// Device settings, kept in flash (NVS). secrets.h, if present, only provides first-boot defaults.
// They're edited from the phone/Mac setup page (remote.cpp).
struct DeviceConfig {
  String ssid, wifiPass, apiUrl, email, password, tz;
};
DeviceConfig loadConfig();
void saveConfig(const DeviceConfig &c);  // empty password/wifiPass keep the stored ones

void backendBegin();  // starts Wi-Fi (or the setup network), clock sync and the Remi API task (core 0)
bool backendInSetupMode();
