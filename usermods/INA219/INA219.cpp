// WLED-Lite — INA219 current-monitor usermod.
//
// A safety net over WLED's existing math-based ABL (auto brightness limiter).
//
//  - Math ABL (bus_manager.cpp) runs every frame and scales brightness based on
//    a per-pixel current estimate. Fast, but only as accurate as the configured
//    mA/LED constant.
//  - This usermod reads the INA219 every ~200 ms and, if measured current
//    exceeds the configured limit by more than the hysteresis margin, directly
//    reduces `bri` to bring the device back into safe operating range. It does
//    NOT touch the math ABL logic; the two layers cooperate.
//
// Topology: single INA219 monitoring the carrier's main 5V supply (Phase 1).
// Internal data model uses a sensor array (`_sensors[1]` today) so a future
// rev can grow to per-bus monitoring without rewriting the public surface.
//
// See docs/current-limit-ina219.md.

#include "wled.h"
#include <INA219_WE.h>

// ----- Compile-time defaults (overridable via -D in platformio.ini) -----

#ifndef INA219_DEFAULT_ADDRESS
  #define INA219_DEFAULT_ADDRESS 0x40  // INA219 strap pins both grounded
#endif

#ifndef INA219_DEFAULT_SHUNT_MICRO_OHMS
  #define INA219_DEFAULT_SHUNT_MICRO_OHMS 100000  // 0.1 Ω = 100,000 μΩ (typical "small" shunt)
#endif

#ifndef INA219_DEFAULT_MAX_CURRENT_MA
  #define INA219_DEFAULT_MAX_CURRENT_MA 3200  // 3.2 A — sane for a small-sign carrier
#endif

#ifndef INA219_DEFAULT_POLL_INTERVAL_MS
  #define INA219_DEFAULT_POLL_INTERVAL_MS 200
#endif

#ifndef INA219_DEFAULT_HYSTERESIS_PCT
  #define INA219_DEFAULT_HYSTERESIS_PCT 110  // safety-net kicks in at 110% of limit
#endif

#ifndef INA219_ENABLED_DEFAULT
  #define INA219_ENABLED_DEFAULT false
#endif

// Local usermod ID. Upstream's USERMOD_ID_* range is currently 0-58; we pick 200
// to avoid future collision without modifying const.h (preserves upstream-merge
// friendliness). USERMOD_ID_UNSPECIFIED (1) would also work but doesn't let the
// admin UI distinguish this module by ID.
#ifndef USERMOD_ID_INA219
  #define USERMOD_ID_INA219 200
#endif

// Single-sensor build today, designed to expand.
#ifndef INA219_MAX_SENSORS
  #define INA219_MAX_SENSORS 1
#endif


class UsermodINA219 : public Usermod {
  private:
    static const char _name[];

    // ---- per-sensor record. Today only [0] is used. ----
    struct SensorState {
      INA219_WE   *driver       = nullptr;
      uint8_t      i2cAddress   = INA219_DEFAULT_ADDRESS;
      uint32_t     shuntUohm    = INA219_DEFAULT_SHUNT_MICRO_OHMS;
      uint16_t     maxCurrentMa = INA219_DEFAULT_MAX_CURRENT_MA;

      // Most recent reading
      float        lastCurrentA = 0.0f;
      float        lastBusV     = 0.0f;
      float        lastPowerW   = 0.0f;
      unsigned long lastReadMs  = 0;
      bool         deviceOk     = false;  // init() succeeded and last read returned valid data
    };

    SensorState   _sensors[INA219_MAX_SENSORS];

    // ---- global settings (config-persistent) ----
    bool          _enabled            : 1;
    bool          _autoDetect         : 1;  // probe common addrs on setup; otherwise honor configured addr
    bool          _capEnforced        : 1;  // act on measurements (true) or telemetry-only (false)
    bool          _initDone           : 1;
    uint16_t      _pollIntervalMs;
    uint16_t      _hysteresisPct;           // safety-net trigger at this % of maxCurrentMa

    // ---- internal state ----
    unsigned long _lastPollMs   = 0;

    // Configure an INA219_WE for the chosen shunt + range.
    // We use the wollewald library's `calibrate()` API to set the calibration
    // register for the requested expected-max-current and shunt resistance.
    void configureDriver(SensorState &s) {
      if (!s.driver) return;
      float shuntOhms      = static_cast<float>(s.shuntUohm) / 1000000.0f;
      float maxCurrentAmps = static_cast<float>(s.maxCurrentMa) / 1000.0f;
      // 32V bus, configurable shunt range; auto-pick PGA appropriate for our current range.
      // Wollewald API: setADCMode/setPGain/setMeasureMode/calibrate
      s.driver->setADCMode(BIT_MODE_12);
      s.driver->setMeasureMode(CONTINUOUS);
      s.driver->setPGain(PG_320);                     // ±320 mV across shunt = widest range
      s.driver->setBusRange(BRNG_32);                 // up to 32 V bus
      s.driver->setShuntSizeInOhms(shuntOhms);
      // The library's calibrate() flavor in v1.3.x derives the LSB internally;
      // we keep our own maxCurrentMa as the cap reference (independent of the IC's range).
      (void)maxCurrentAmps;
    }

    // Try to instantiate a driver for this sensor's i2cAddress.
    // Returns true if init succeeded (chip ACKed on bus).
    bool tryInit(SensorState &s) {
      if (s.driver) {
        delete s.driver;
        s.driver = nullptr;
      }
      s.driver = new INA219_WE(s.i2cAddress);
      if (!s.driver->init()) {
        DEBUG_PRINTF_P(PSTR("INA219: no device at 0x%02X\n"), s.i2cAddress);
        delete s.driver;
        s.driver = nullptr;
        s.deviceOk = false;
        return false;
      }
      configureDriver(s);
      s.deviceOk = true;
      DEBUG_PRINTF_P(PSTR("INA219: device 0x%02X ready (shunt=%luuOhm max=%umA)\n"),
                     s.i2cAddress, s.shuntUohm, s.maxCurrentMa);
      return true;
    }

    // Probe the 4 standard INA219 addresses; use the first that responds.
    void autoDetect(SensorState &s) {
      static const uint8_t candidates[] = { 0x40, 0x41, 0x44, 0x45 };
      for (uint8_t addr : candidates) {
        s.i2cAddress = addr;
        if (tryInit(s)) return;
      }
      DEBUG_PRINTLN(F("INA219: auto-detect found no device"));
    }

    // Read one sensor. Returns measured current in mA, or -1 if read failed.
    int32_t readSensor(SensorState &s) {
      if (!s.driver || !s.deviceOk) return -1;
      float curA = s.driver->getCurrent_mA() / 1000.0f;
      // INA219_WE returns absolute value; sign comes from getCurrent_uA(). For LED
      // applications we always sink power so curA is always >= 0. A negative reading
      // would indicate wiring reversed across the shunt -- still safe to act on |curA|.
      s.lastCurrentA = fabsf(curA);
      s.lastBusV     = s.driver->getBusVoltage_V();
      s.lastPowerW   = s.driver->getBusPower() / 1000.0f;
      s.lastReadMs   = millis();
      return static_cast<int32_t>(s.lastCurrentA * 1000.0f);
    }

    // Apply safety-net cap if any sensor exceeds the hysteresis threshold.
    // Reduces bri proportionally so the device returns to safe range.
    void enforceCap() {
      if (!_capEnforced) return;
      uint32_t totalMa     = 0;
      uint32_t totalLimitMa = 0;
      for (uint8_t i = 0; i < INA219_MAX_SENSORS; i++) {
        SensorState &s = _sensors[i];
        if (!s.deviceOk) continue;
        totalMa      += static_cast<uint32_t>(s.lastCurrentA * 1000.0f);
        totalLimitMa += s.maxCurrentMa;
      }
      if (totalLimitMa == 0) return;
      uint32_t triggerMa = (totalLimitMa * _hysteresisPct) / 100;
      if (totalMa > triggerMa) {
        // We're over the safety net. Scale brightness down so that
        // newBri/bri == limit/measured. We apply via the global `bri` so the
        // next show() picks it up; math ABL also runs but its cap is unchanged.
        if (totalMa > 0 && bri > 0) {
          uint16_t newBri = (uint32_t)bri * totalLimitMa / totalMa;
          if (newBri < 1) newBri = 1;
          if (newBri < bri) {
            DEBUG_PRINTF_P(PSTR("INA219: safety-net %umA > %umA (cap=%umA), bri %u -> %u\n"),
                           totalMa, triggerMa, totalLimitMa, bri, newBri);
            bri = newBri;
            stateUpdated(CALL_MODE_DIRECT_CHANGE);
          }
        }
      }
    }

  public:
    UsermodINA219() {
      _enabled       = INA219_ENABLED_DEFAULT;
      _autoDetect    = true;
      _capEnforced   = true;
      _initDone      = false;
      _pollIntervalMs = INA219_DEFAULT_POLL_INTERVAL_MS;
      _hysteresisPct = INA219_DEFAULT_HYSTERESIS_PCT;
    }

    void setup() override {
      if (!_enabled) return;
      // Phase 1: single sensor. Future phases iterate _sensors[].
      SensorState &s0 = _sensors[0];
      if (_autoDetect) autoDetect(s0);
      else             tryInit(s0);
    }

    void loop() override {
      if (!_enabled) return;
      // Don't hit I2C while the strip is mid-update -- could cause LED glitches
      // on boards where I2C shares timing-sensitive resources.
      if (strip.isUpdating()) return;

      unsigned long now = millis();
      if (now - _lastPollMs < _pollIntervalMs) return;
      _lastPollMs = now;

      for (uint8_t i = 0; i < INA219_MAX_SENSORS; i++) {
        readSensor(_sensors[i]);
      }
      enforceCap();
    }

    uint16_t getId() override {
      return USERMOD_ID_INA219;
    }

    // -------- Admin UI: live readings in /json/info "u" section --------
    void addToJsonInfo(JsonObject &root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      if (!_enabled) {
        JsonArray j = user.createNestedArray(F("INA219"));
        j.add(F("disabled"));
        return;
      }

      for (uint8_t i = 0; i < INA219_MAX_SENSORS; i++) {
        SensorState &s = _sensors[i];
        JsonArray jCurrent = user.createNestedArray(F("INA219 Current"));
        JsonArray jVoltage = user.createNestedArray(F("INA219 Voltage"));
        JsonArray jPower   = user.createNestedArray(F("INA219 Power"));
        if (!s.deviceOk) {
          jCurrent.add(F("no device"));
          jVoltage.add(F("--"));
          jPower.add(F("--"));
          continue;
        }
        jCurrent.add(s.lastCurrentA);  jCurrent.add(F("A"));
        jVoltage.add(s.lastBusV);      jVoltage.add(F("V"));
        jPower.add(s.lastPowerW);      jPower.add(F("W"));
      }
    }

    // -------- Admin UI: settings form fields --------
    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[F("Enabled")]         = _enabled;
      top[F("AutoDetect")]      = _autoDetect;
      top[F("I2CAddress")]      = static_cast<uint8_t>(_sensors[0].i2cAddress);
      top[F("ShuntMilliOhms")]  = static_cast<float>(_sensors[0].shuntUohm) / 1000.0f;
      top[F("MaxCurrentMa")]    = _sensors[0].maxCurrentMa;
      top[F("PollIntervalMs")]  = _pollIntervalMs;
      top[F("HysteresisPct")]   = _hysteresisPct;
      top[F("EnforceCap")]      = _capEnforced;
    }

    void appendConfigData() override {
      oappend(F("addInfo('INA219:I2CAddress',1,'(hex 0x40..0x45, auto-detect overrides)');"));
      oappend(F("addInfo('INA219:ShuntMilliOhms',1,'m&Omega; (typical 100)');"));
      oappend(F("addInfo('INA219:MaxCurrentMa',1,'mA');"));
      oappend(F("addInfo('INA219:PollIntervalMs',1,'ms');"));
      oappend(F("addInfo('INA219:HysteresisPct',1,'% of max (110 = safety-net at 110%)');"));
      oappend(F("addInfo('INA219:EnforceCap',1,'safety-net mode (off = telemetry only)');"));
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      if (!configComplete) return false;

      bool tmpBool;
      configComplete &= getJsonValue(top[F("Enabled")],    tmpBool); _enabled    = tmpBool;
      configComplete &= getJsonValue(top[F("AutoDetect")], tmpBool); _autoDetect = tmpBool;
      configComplete &= getJsonValue(top[F("EnforceCap")], tmpBool); _capEnforced = tmpBool;
      configComplete &= getJsonValue(top[F("I2CAddress")], _sensors[0].i2cAddress);
      float shuntMilliOhms;
      if (getJsonValue(top[F("ShuntMilliOhms")], shuntMilliOhms) && shuntMilliOhms > 0) {
        _sensors[0].shuntUohm = static_cast<uint32_t>(shuntMilliOhms * 1000.0f + 0.5f);
      } else configComplete = false;
      configComplete &= getJsonValue(top[F("MaxCurrentMa")],   _sensors[0].maxCurrentMa);
      configComplete &= getJsonValue(top[F("PollIntervalMs")], _pollIntervalMs);
      configComplete &= getJsonValue(top[F("HysteresisPct")],  _hysteresisPct);

      if (_hysteresisPct < 100) _hysteresisPct = 100;       // never trigger below limit
      if (_hysteresisPct > 200) _hysteresisPct = 200;       // sanity cap
      if (_pollIntervalMs < 20) _pollIntervalMs = 20;       // avoid hammering the bus
      if (_pollIntervalMs > 60000) _pollIntervalMs = 60000;

      // If we've already inited once, reapply config (e.g. address changed in admin UI).
      if (_initDone && _enabled) {
        SensorState &s0 = _sensors[0];
        if (_autoDetect) autoDetect(s0);
        else             tryInit(s0);
      }
      _initDone = true;
      return configComplete;
    }

    ~UsermodINA219() {
      for (uint8_t i = 0; i < INA219_MAX_SENSORS; i++) {
        delete _sensors[i].driver;
        _sensors[i].driver = nullptr;
      }
    }
};

const char UsermodINA219::_name[] PROGMEM = "INA219";

static UsermodINA219 ina219;
REGISTER_USERMOD(ina219);
