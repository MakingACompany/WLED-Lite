#pragma once

// WLED-Lite — Daily-schedule endpoint (public API).
//
// HTTP-callable mechanism for the slim user UI to configure a daily on/off
// schedule WITHOUT exposing the full PIN-gated /json?cfg surface. The end
// user (Dad) needs to set their own schedule; PIN is the maintainer's. So
// we expose a narrow, scope-limited endpoint that can ONLY:
//   1. Save the current device state as preset 250 ("WLED-Lite Daily ON")
//   2. Save an off-state as preset 249 ("WLED-Lite Daily OFF")
//   3. Add/replace two entries in the global `timers` vector that fire
//      preset 250 at the requested "on" time and preset 249 at the "off"
//      time, every day of the week.
// Nothing else is reachable through this endpoint.
//
// POST /wled-lite/schedule
//   request body:  {"on":"HH:MM","off":"HH:MM"}   (24-hour format)
//             or:  {"clear":true}                  (removes WLED-Lite-managed timers)
//   response:      {"ok":true} on success; 400 + {"error":"..."} on bad input
//
// Gated by -D WLED_LITE_SCHEDULE in both wled_server.cpp and this module
// so a build without the flag keeps upstream behavior unchanged.

#ifdef WLED_LITE_SCHEDULE

class AsyncWebServer;

namespace WLEDLiteSchedule {
  // Register the endpoint with the global WLED server. Call from
  // wled_server.cpp's registerServerEndpoints() (or similar), once.
  void registerEndpoint(AsyncWebServer &server);
}

#endif // WLED_LITE_SCHEDULE
