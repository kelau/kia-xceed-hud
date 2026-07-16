#pragma once
#if __has_include("TestModeSecrets.h")
#include "TestModeSecrets.h"
#else
#define HUD_TEST_MODE 0
#define HUD_TEST_WIFI_SSID ""
#define HUD_TEST_WIFI_PASSWORD ""
#define HUD_TEST_MDNS_NAME "kia-hud"
#endif

namespace hud::test_mode {
inline constexpr bool enabled = HUD_TEST_MODE != 0;
inline constexpr const char* ssid = HUD_TEST_WIFI_SSID;
inline constexpr const char* password = HUD_TEST_WIFI_PASSWORD;
inline constexpr const char* mdnsName = HUD_TEST_MDNS_NAME;
}
