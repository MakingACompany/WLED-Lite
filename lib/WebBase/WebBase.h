#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// WebBase — embed in any user firmware to get FlashLight integration.
//
// Minimal usage:
//   #include <WebBase.h>
//
//   void setup() {
//     Serial.begin(115200);
//     webbase.begin();          // WiFi from NVS, factory-mode button check
//     webbase.attachOTA(server);     // POST /ota  → live firmware upload
//     webbase.attachSerial(server);  // WS /ws/webbase/serial → serial bridge
//     webbase.attachSettings(server);// GET/POST /api/webbase/* → NVS settings UI
//     server.begin();
//   }
//
//   void loop() {
//     webbase.log("hello");  // goes to Serial + browser serial monitor
//   }
class WebBase {
public:
  // Must be called first in setup(). Checks factory-mode button, reads NVS,
  // connects to WiFi (credentials stored by FlashLight factory firmware).
  void begin();

  // Attach hooks to the user firmware's existing AsyncWebServer instance.
  // Call these before server.begin().
  void attachOTA     (AsyncWebServer& server);  // POST /ota
  void attachSerial  (AsyncWebServer& server);  // WS /ws/webbase/serial
  void attachSettings(AsyncWebServer& server);  // GET/POST /api/webbase/settings
                                                // POST /api/webbase/factory

  // Self-contained device management page (no LittleFS upload needed).
  // Default path: /webbase  —  shows device info, OTA upload, settings,
  // serial toggle, and a "Return to FlashLight" button.
  void attachUI(AsyncWebServer& server, const char* path = "/webbase");

  // Log a message to Serial and, if the serial bridge is enabled and a
  // browser is connected, to the WebSocket serial monitor as well.
  void log (const String& msg);
  void logf(const char* fmt, ...);

  // Forward already-formatted text to the WS serial bridge only — does NOT
  // re-print to Serial. Use as a Log sink in user firmware that already
  // prints to Serial itself, to avoid double output on the serial port.
  void broadcastSerial(const char* data, size_t len);

  // Set boot partition to factory and restart. Call from a settings page
  // button or any trigger that should return the device to FlashLight mode.
  [[noreturn]] void triggerFactoryMode();

  // State accessors
  bool   online()       const { return connected_; }
  bool   apActive()     const { return apActive_; }
  String deviceName()   const;
  String ip()           const;
  bool   serialEnabled()const;

private:
  void checkFactoryButton();

  bool           connected_ = false;
  bool           apActive_  = false;
  AsyncWebSocket* ws_       = nullptr;
};

extern WebBase webbase;
