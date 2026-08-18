#pragma once
#include <cstdint>
#include <cstddef>

// Lightweight firmware-identity tagging for OTA safety, generalized from
// WLED-Lite's wled_metadata.cpp approach: every FlashLight-managed project
// embeds one small Identity struct in its binary (via DECLARE_FW_IDENTITY),
// and an OTA acceptor scans an uploaded image for it before writing.
//
// Two different policies apply depending on who's checking (see WebBase.cpp's
// attachOTA() vs FlashLight Core's src/ota/OTA.cpp) — this header only
// provides the scan/match primitives, not the accept/reject decision.
namespace fwid {
  constexpr uint32_t MAGIC             = 0x464C4944;  // "FLID"
  constexpr size_t   PROJECT_MAX_LEN   = 32;
  constexpr size_t   VERSION_MAX_LEN   = 16;

  struct __attribute__((packed)) Identity {
    uint32_t magic;
    char     project[PROJECT_MAX_LEN];
    char     version[VERSION_MAX_LEN];
    uint32_t hash;
  };

  constexpr uint32_t djb2(const char* s, uint32_t h = 5381) {
    return (*s == 0) ? h : djb2(s + 1, ((h << 5) + h) + static_cast<uint32_t>(static_cast<unsigned char>(*s)));
  }

  // Scans `data` (length `len`) for a valid, hash-verified Identity struct.
  // NOTE: only scans within this single buffer — a struct split across two
  // upload chunks won't be found. Callers should scan every chunk as it
  // streams in and treat "never found in any chunk" as "unknown project",
  // not as "corrupt" — see the permissive-fallback policy in attachOTA().
  bool find(const uint8_t* data, size_t len, Identity* out);

  // Case-sensitive exact match against `expectedProject`.
  bool matches(const Identity& found, const char* expectedProject);
}

// Emits a const Identity that fwid::find() can locate inside an uploaded
// firmware.bin via a raw byte scan (see find()'s docs above) — no fixed
// offset or symbol lookup needed, so it doesn't need a dedicated linker
// section. `used` just keeps the linker from discarding it as dead data,
// since nothing else in the program ever reads g_fwIdentity directly.
//
// IMPORTANT: do NOT give this a custom section() attribute — a section name
// outside the toolchain's normal .rodata grouping can land in a separate
// ELF LOAD segment that collides with another segment's 64KB flash-mapping
// page, which esptool's elf2image step rejects outright ("lands in same
// 64KB flash mapping ... Can't generate binary"). Confirmed by direct build
// failure when this was tried.
//
// Call exactly once, anywhere, in any FlashLight-managed project's source
// (NAME and VERSION must be string literals, e.g. from a -D build flag).
#define DECLARE_FW_IDENTITY(NAME, VERSION)                     \
  extern const fwid::Identity g_fwIdentity;                    \
  const fwid::Identity __attribute__((used))                   \
      g_fwIdentity = { fwid::MAGIC, NAME, VERSION, fwid::djb2(NAME VERSION) }
