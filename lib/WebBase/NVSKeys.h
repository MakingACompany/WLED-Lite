#pragma once
// Shared NVS namespace and key constants used by both FlashLight factory
// firmware (src/) and any user project that includes the WebBase library.
// Import this file anywhere you need to read FlashLight-managed settings.
//
// Keep byte-for-byte in sync with FlashLight's own src/cfg.h — there is no
// shared header between src/ and lib/WebBase/ today.
namespace webbase_nvs {
  constexpr char NS[]            = "webbase";
  // Saved-network slots. Slot 0 keeps the original key names so existing
  // devices migrate with zero data loss; slots 1-2 are new.
  constexpr uint8_t MAX_WIFI_NETWORKS = 3;
  constexpr char WIFI_SSID[]     = "wifi_ssid";
  constexpr char WIFI_PASS[]     = "wifi_pass";
  constexpr char WIFI_SSID2[]    = "wifi_ssid2";
  constexpr char WIFI_PASS2[]    = "wifi_pass2";
  constexpr char WIFI_SSID3[]    = "wifi_ssid3";
  constexpr char WIFI_PASS3[]    = "wifi_pass3";
  constexpr char DEVICE_NAME[]   = "device_name";
  constexpr char SERIAL_EN[]     = "serial_en";
  constexpr char AP_ONLY[]       = "ap_only";      // legacy bool, read-only migration source for AP_MODE
  constexpr char AP_MODE[]       = "ap_mode";       // 0 = auto-fallback, 1 = always-on
  constexpr char FORCE_FACTORY[] = "force_factory";
  constexpr char OTA_PASS[]      = "ota_pass";
  constexpr char PARTITION[]     = "webbase_nvs";  // encrypted NVS partition label
}
