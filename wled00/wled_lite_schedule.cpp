// WLED-Lite — Daily-schedule endpoint implementation.
// See wled_lite_schedule.h for the API contract.

#include "wled.h"
#include "wled_lite_schedule.h"

#ifdef WLED_LITE_SCHEDULE

namespace {

  // Reserved preset slots managed by the daily-schedule endpoint.
  // Out of WLED's 1-250 preset ID space; using the top to minimize collision
  // risk with maintainer-defined presets.
  constexpr uint8_t PRESET_ID_DAILY_ON  = 250;
  constexpr uint8_t PRESET_ID_DAILY_OFF = 249;

  // weekdays bitmask: bit 0 = enabled, bits 1..7 = Mon..Sun.
  // 0xFF = enabled + every day of the week.
  constexpr uint8_t WEEKDAYS_DAILY_ENABLED = 0xFF;

  // Parse "HH:MM" -> {hour, minute}. Returns false on bad format.
  bool parseHHMM(const char* s, uint8_t &hour, uint8_t &minute) {
    if (!s) return false;
    int h = -1, m = -1;
    if (sscanf(s, "%d:%d", &h, &m) != 2) return false;
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
    hour = static_cast<uint8_t>(h);
    minute = static_cast<uint8_t>(m);
    return true;
  }

  // Drop any existing timer entries that point at our reserved preset slots.
  // Iterates backwards so removeTimer() doesn't shift unvisited indices.
  void clearOurTimers() {
    for (int i = static_cast<int>(getTimerCount()) - 1; i >= 0; --i) {
      if (timers[i].preset == PRESET_ID_DAILY_ON || timers[i].preset == PRESET_ID_DAILY_OFF) {
        removeTimer(i);
      }
    }
    compactTimers();
  }

  // Apply a new schedule (replaces any prior WLED-Lite schedule entries).
  // Requires the caller to have acquired the global JSON buffer lock and
  // already set up pDoc to contain the off-state for the OFF preset write.
  // After this call, pDoc has been consumed by savePreset(249).
  void applySchedule_locked(uint8_t onH, uint8_t onM, uint8_t offH, uint8_t offM) {
    clearOurTimers();

    // Save OFF preset first (immediate; consumes pDoc which the caller
    // populated with the off-state JSON before locking).
    savePreset(PRESET_ID_DAILY_OFF, "WLED-Lite Daily OFF", pDoc->as<JsonObject>());

    // Save ON preset (async; doSaveState will capture current device state
    // and write later via handlePresets()).
    savePreset(PRESET_ID_DAILY_ON, "WLED-Lite Daily ON", JsonObject());

    addTimer(PRESET_ID_DAILY_ON,  onH,  static_cast<int8_t>(onM),  WEEKDAYS_DAILY_ENABLED);
    addTimer(PRESET_ID_DAILY_OFF, offH, static_cast<int8_t>(offM), WEEKDAYS_DAILY_ENABLED);

    configNeedsWrite = true; // persists the new timer entries to cfg.json
  }

  void respond(AsyncWebServerRequest *request, uint16_t code, const char *bodyJson) {
    request->send(code, F("application/json"), bodyJson);
  }

  // Body of the POST handler. Already runs with JSON_LOCK_SERVER held.
  void handleScheduleRequest_locked(AsyncWebServerRequest *request, JsonObject root) {
    if (root["clear"] == true) {
      clearOurTimers();
      configNeedsWrite = true;
      respond(request, 200, R"({"ok":true,"cleared":true})");
      return;
    }

    const char *onStr  = root["on"]  | (const char*)nullptr;
    const char *offStr = root["off"] | (const char*)nullptr;
    if (!onStr || !offStr) {
      respond(request, 400, R"({"error":"need 'on' and 'off' fields as 'HH:MM'"})");
      return;
    }

    uint8_t onH = 0, onM = 0, offH = 0, offM = 0;
    if (!parseHHMM(onStr, onH, onM) || !parseHHMM(offStr, offH, offM)) {
      respond(request, 400, R"({"error":"bad time format; use 'HH:MM' in 24-hour time"})");
      return;
    }

    // Set up pDoc with the off-state JSON. savePreset() inside
    // applySchedule_locked() will consume this.
    pDoc->clear();
    JsonObject offObj = pDoc->to<JsonObject>();
    offObj["on"] = false;
    offObj["o"]  = true;
    offObj["n"]  = "WLED-Lite Daily OFF";

    applySchedule_locked(onH, onM, offH, offM);
    respond(request, 200, R"({"ok":true})");
  }

} // namespace

namespace WLEDLiteSchedule {

void registerEndpoint(AsyncWebServer &server) {
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
    "/wled-lite/schedule",
    [](AsyncWebServerRequest *request) {
      if (!requestJSONBufferLock(JSON_LOCK_SERVER)) {
        request->deferResponse();
        return;
      }
      DeserializationError err = deserializeJson(*pDoc, (uint8_t*)(request->_tempObject));
      if (err) {
        releaseJSONBufferLock();
        respond(request, 400, R"({"error":"invalid JSON body"})");
        return;
      }
      JsonObject root = pDoc->as<JsonObject>();
      if (root.isNull()) {
        releaseJSONBufferLock();
        respond(request, 400, R"({"error":"expected JSON object"})");
        return;
      }
      handleScheduleRequest_locked(request, root);
      releaseJSONBufferLock();
    });
  handler->setMaxContentLength(256);
  server.addHandler(handler);
}

} // namespace WLEDLiteSchedule

#endif // WLED_LITE_SCHEDULE
