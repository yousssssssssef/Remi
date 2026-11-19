#pragma once
// Copy this file to secrets.h and fill it in. secrets.h stays on your computer; don't share it.
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-wifi-password"

// The Remi API. On the same Wi-Fi as your Mac: run `ipconfig getifaddr en0` and use that address.
#define REMI_API_URL "http://192.168.1.7:3000/graphql"

// A Remi account in your household. The device signs in once, then keeps only a refresh token
// (in its own flash). shortcut: the password is compiled in; replace with on-device pairing (see README).
#define REMI_EMAIL "you@example.com"
#define REMI_PASSWORD "your-remi-password"

// POSIX timezone. Egypt (Cairo, with summer time): EET-2EEST,M4.5.5/0,M10.5.4/24
#define REMI_TZ "EET-2EEST,M4.5.5/0,M10.5.4/24"
