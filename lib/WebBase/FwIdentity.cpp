#include "FwIdentity.h"
#include <cstring>

namespace fwid {

bool find(const uint8_t* data, size_t len, Identity* out) {
  if (len < sizeof(Identity)) return false;
  // Linear scan for the magic at every 4-byte-aligned offset (the struct is
  // packed but the compiler still places it on a natural alignment boundary
  // in .rodata, so we don't need to check every single byte offset).
  for (size_t i = 0; i + sizeof(Identity) <= len; i += 4) {
    uint32_t magic;
    memcpy(&magic, data + i, sizeof(magic));
    if (magic != MAGIC) continue;

    Identity candidate;
    memcpy(&candidate, data + i, sizeof(Identity));
    // project/version must be NUL-terminated within their fixed fields.
    bool projectTerminated = false, versionTerminated = false;
    for (size_t j = 0; j < PROJECT_MAX_LEN; j++) if (candidate.project[j] == 0) { projectTerminated = true; break; }
    for (size_t j = 0; j < VERSION_MAX_LEN; j++) if (candidate.version[j] == 0) { versionTerminated = true; break; }
    if (!projectTerminated || !versionTerminated) continue;

    uint32_t h = 5381;
    for (const char* p = candidate.project; *p; p++) h = ((h << 5) + h) + static_cast<uint32_t>(static_cast<unsigned char>(*p));
    for (const char* p = candidate.version; *p; p++) h = ((h << 5) + h) + static_cast<uint32_t>(static_cast<unsigned char>(*p));
    if (h != candidate.hash) continue;

    *out = candidate;
    return true;
  }
  return false;
}

bool matches(const Identity& found, const char* expectedProject) {
  return strncmp(found.project, expectedProject, PROJECT_MAX_LEN) == 0;
}

}  // namespace fwid
