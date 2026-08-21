/*
  WLED-Lite build identity.

  Replaces the old wled_metadata.h/.cpp (fixed linker-section struct + magic +
  hash, scanned by ota_update.cpp's own findWledMetadata()/shouldAllowOTA()).
  That whole OTA-identity-validation mechanism is gone along with
  ota_update.cpp -- WebBase's FwIdentity (g_fwIdentity below) now does the
  equivalent job for the WebBase-owned /ota route (see lib/WebBase/FwIdentity.h
  and WebBase.cpp's attachOTA()).

  What's left here are the small, genuinely still-needed display/info strings
  (versionString/releaseString/repoString) that json.cpp, e131.cpp, and
  wled.cpp's own debug banner print -- these have nothing to do with OTA
  validation, they're just "what build is this" strings for humans/APIs.
  productString/brandString from the old wled_metadata.cpp were unused
  everywhere except improv.cpp (now removed) and were already marked
  deprecated, so they're dropped rather than carried forward.
*/
#include <FwIdentity.h>
#include <WString.h>

#ifndef WLED_RELEASE_NAME
  #warning WLED_RELEASE_NAME was not set - using default value of 'Custom'
  #define WLED_RELEASE_NAME "Custom"
#endif

// package.json's "version" field, as of this writing -- see platformio.ini /
// pio-scripts/set_metadata.py, which is what fed the old wled_metadata.cpp's
// TOSTRING(WLED_VERSION). DECLARE_FW_IDENTITY() requires true string-literal
// tokens (see FwIdentity.h), so this can't be wired to that build-time value
// automatically; bump it by hand alongside package.json's "version".
#define WLED_LITE_VERSION_STR "0.2.0-lite"

static_assert(sizeof(WLED_RELEASE_NAME) <= fwid::PROJECT_MAX_LEN,
              "WLED_RELEASE_NAME exceeds fwid::PROJECT_MAX_LEN");
static_assert(sizeof(WLED_LITE_VERSION_STR) <= fwid::VERSION_MAX_LEN,
              "WLED_LITE_VERSION_STR exceeds fwid::VERSION_MAX_LEN");

DECLARE_FW_IDENTITY(WLED_RELEASE_NAME, WLED_LITE_VERSION_STR);

#ifndef WLED_REPO
  // No warning for this one: integrators are not always on GitHub
  #define WLED_REPO "unknown"
#endif

// `extern` is required here: a const namespace-scope variable has internal
// linkage by default in C++ (unlike C), and this file never sees the
// `extern const char versionString[];` declaration from fcn_declare.h (it
// only includes FwIdentity.h/WString.h), so without `extern` these would be
// silently local to this translation unit and every other .cpp referencing
// them would fail to link.
extern const char versionString[] = WLED_LITE_VERSION_STR;
extern const char releaseString[] = WLED_RELEASE_NAME;

static const char repoString_s[] PROGMEM = WLED_REPO;
const __FlashStringHelper* repoString = FPSTR(repoString_s);
