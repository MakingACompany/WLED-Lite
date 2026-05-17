#pragma once

// WLED-Lite — Admin segment editor (public API).
//
// Provides /wled-lite/segments — an admin-only HTML page that lets the
// maintainer define named segments on a single LED strip (e.g. one segment
// per backlit letter in a sign). The user's slim UI reads those segments
// from /json/state and shows them as labeled pills so the end user can
// pick "L" or "A" or "B1" and adjust color/effect/brightness for just
// that segment — without ever seeing pixel-range geometry.
//
// PIN-gated: serves the page only when the admin PIN has been entered
// (correctPIN == true) OR the device is still in factory state
// (settingsPIN empty), matching the rest of /settings/*.
//
// All segment mutations are done client-side via /json/state. We don't
// add a new write endpoint here -- segment changes are part of normal
// state which doesn't require PIN, and the admin page itself is gated
// by the page-serve check.
//
// Gated by -D WLED_LITE_SEGMENTS in wled_server.cpp and this module.

#ifdef WLED_LITE_SEGMENTS

class AsyncWebServer;

namespace WLEDLiteSegments {
  void registerEndpoint(AsyncWebServer &server);
}

#endif // WLED_LITE_SEGMENTS
