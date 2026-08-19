#include "WebBase.h"
#include "NVSKeys.h"
#include "FwIdentity.h"
#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <esp_partition.h>
#include <esp_mac.h>
#include <cstdarg>

WebBase webbase;

// Declared by this project's own DECLARE_FW_IDENTITY() call (any .cpp file) —
// tells attachOTA() what project it's allowed to accept uploads for.
extern const fwid::Identity g_fwIdentity;

// Per-upload-session state for the identity check (reset at index==0).
static bool sOtaIdentityMismatch = false;
static bool sOtaIdentityFoundAny = false;

struct WifiCred { String ssid; String pass; };
enum class ApMode : uint8_t { AutoFallback = 0, AlwaysOn = 1 };

// Module-level NVS state (loaded once in begin())
static WifiCred gNets[webbase_nvs::MAX_WIFI_NETWORKS];
static String gDeviceName, gOtaPass;
static bool   gSerialEn     = false;
static ApMode gApMode       = ApMode::AutoFallback;
static bool   gNvsEncrypted = false;

static bool netsHaveWifi() {
  for (const auto& n : gNets) if (n.ssid.length() > 0) return true;
  return false;
}

// Try to open the encrypted webbase_nvs partition.
// If the nvs_keys partition has keys (set by factory firmware) we init it;
// otherwise fall back to the default NVS namespace unencrypted.
static void initEncryptedNVS() {
#ifdef CONFIG_NVS_ENCRYPTION
  const esp_partition_t* keysPart = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, nullptr);
  if (!keysPart) return;

  nvs_sec_cfg_t cfg = {};
  if (nvs_flash_read_security_cfg(keysPart, &cfg) != ESP_OK) return;  // keys not generated yet

  esp_err_t err = nvs_flash_secure_init_partition(webbase_nvs::PARTITION, &cfg);
  gNvsEncrypted = (err == ESP_OK);
#else
  // CONFIG_NVS_ENCRYPTION is not enabled in this SDK build (Tasmota Arduino
  // Core does not compile the nvs_sec subsystem) — same situation as
  // src/core/NVSEncrypt.cpp on the FlashLight Core side. Falls back to
  // plaintext NVS; gNvsEncrypted stays false.
#endif
}

static const char* nvsPart() {
  return gNvsEncrypted ? webbase_nvs::PARTITION : nullptr;
}

static void loadNVS() {
  initEncryptedNVS();
  Preferences prefs;
  prefs.begin(webbase_nvs::NS, true, nvsPart());
  gNets[0].ssid = prefs.getString(webbase_nvs::WIFI_SSID,  "");
  gNets[0].pass = prefs.getString(webbase_nvs::WIFI_PASS,  "");
  gNets[1].ssid = prefs.getString(webbase_nvs::WIFI_SSID2, "");
  gNets[1].pass = prefs.getString(webbase_nvs::WIFI_PASS2, "");
  gNets[2].ssid = prefs.getString(webbase_nvs::WIFI_SSID3, "");
  gNets[2].pass = prefs.getString(webbase_nvs::WIFI_PASS3, "");
  gDeviceName = prefs.getString(webbase_nvs::DEVICE_NAME, "ESP32Device");
  gOtaPass    = prefs.getString(webbase_nvs::OTA_PASS,    "");
  gSerialEn   = prefs.getBool  (webbase_nvs::SERIAL_EN,   false);

  bool hasApMode = prefs.isKey(webbase_nvs::AP_MODE);
  if (hasApMode) {
    gApMode = static_cast<ApMode>(prefs.getUChar(webbase_nvs::AP_MODE, 0));
  } else {
    // First boot after upgrade: migrate the legacy boolean into the new key.
    gApMode = prefs.getBool(webbase_nvs::AP_ONLY, false) ? ApMode::AlwaysOn : ApMode::AutoFallback;
  }
  prefs.end();

  if (!hasApMode) {
    Preferences mig;
    mig.begin(webbase_nvs::NS, false, nvsPart());
    mig.putUChar(webbase_nvs::AP_MODE, static_cast<uint8_t>(gApMode));
    mig.end();
  }
}

static void startAP(const String& deviceName) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  // WiFi SSIDs are capped at 32 bytes — truncate the device-name portion so
  // "<name>-XXXXXX" (a '-' plus 6 hex digits = 7 chars) always fits. Falls
  // back to the default device name ("ESP32Device") when it hasn't been
  // customized, so this reduces to a fixed-looking name in that case too.
  char namePart[26];
  strlcpy(namePart, deviceName.c_str(), sizeof(namePart));
  char ssid[33];
  snprintf(ssid, sizeof(ssid), "%s-%02X%02X%02X", namePart, mac[3], mac[4], mac[5]);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  MDNS.begin(deviceName.c_str());
  MDNS.addService("http", "tcp", 80);
  Serial.printf("[webbase] AP '%s' up  ip=%s\n", ssid, WiFi.softAPIP().toString().c_str());
}

// ---------------------------------------------------------------------------
//  begin()
// ---------------------------------------------------------------------------

void WebBase::checkFactoryButton() {
  constexpr uint8_t  PIN     = 0;
  constexpr uint32_t HOLD_MS = 3000;
  pinMode(PIN, INPUT_PULLUP);
  if (digitalRead(PIN) == HIGH) return;  // not pressed

  const uint32_t start = millis();
  while (digitalRead(PIN) == LOW) {
    if (millis() - start >= HOLD_MS) {
      Serial.println("[webbase] BOOT held — returning to factory");
      triggerFactoryMode();  // does not return
    }
    delay(50);
  }
}

void WebBase::begin() {
  checkFactoryButton();
  loadNVS();

  WiFi.persistent(false);
  WiFi.setHostname(gDeviceName.c_str());

  int connectedSlot = -1;
  bool hasWifi = netsHaveWifi();
  if (gApMode != ApMode::AlwaysOn && hasWifi) {
    WiFi.mode(WIFI_STA);
    for (uint8_t i = 0; i < webbase_nvs::MAX_WIFI_NETWORKS; i++) {
      if (gNets[i].ssid.length() == 0) continue;
      WiFi.begin(gNets[i].ssid.c_str(), gNets[i].pass.c_str());
      const uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - start) < 8000) {
        delay(100);
      }
      if (WiFi.status() == WL_CONNECTED) { connectedSlot = i; break; }
      WiFi.disconnect();
    }
  }

  if (connectedSlot >= 0) {
    connected_ = true;
    apActive_  = false;
    MDNS.begin(gDeviceName.c_str());
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[webbase] WiFi up  ssid=%s  host=%s  ip=%s\n",
                  gNets[connectedSlot].ssid.c_str(), gDeviceName.c_str(), WiFi.localIP().toString().c_str());
  } else {
    if (gApMode == ApMode::AlwaysOn) {
      Serial.println("[webbase] AP-Only / Outdoor mode enabled, remaining in Access Point mode");
    } else if (hasWifi) {
      Serial.println("[webbase] WiFi connect failed, falling back to AP");
    } else {
      Serial.println("[webbase] no WiFi credentials, starting AP");
    }
    connected_ = false;
    apActive_  = true;
    startAP(gDeviceName);
  }
}

// ---------------------------------------------------------------------------
//  attachOTA() — adds POST /ota to user firmware's web server.
//  Browser uploads firmware.bin → writes to inactive OTA slot → reboots.
// ---------------------------------------------------------------------------
void WebBase::attachOTA(AsyncWebServer& server) {
  server.on("/ota", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (gOtaPass.length() > 0 &&
          !req->authenticate("admin", gOtaPass.c_str())) {
        return req->requestAuthentication("WebBase", /*digest=*/true);
      }
      if (sOtaIdentityMismatch) {
        Update.abort();
        return req->send(409, "application/json",
            "{\"ok\":false,\"error\":\"firmware mismatch: this device runs '" + String(g_fwIdentity.project) + "'\"}");
      }
      bool ok = !Update.hasError();
      AsyncWebServerResponse* res = req->beginResponse(
          200, "application/json",
          ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"update failed\"}");
      res->addHeader("Connection", "close");
      req->send(res);
      if (ok) { delay(200); ESP.restart(); }
    },
    [](AsyncWebServerRequest* req, String filename, size_t index,
       uint8_t* data, size_t len, bool final) {
      if (gOtaPass.length() > 0 &&
          !req->authenticate("admin", gOtaPass.c_str())) return;
      if (!index) {
        Serial.printf("[webbase/ota] begin  file=%s\n", filename.c_str());
        sOtaIdentityMismatch = false;
        sOtaIdentityFoundAny = false;
        Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
      }
      if (!sOtaIdentityMismatch) {
        fwid::Identity found;
        // Only scan chunks until we've made a decision — an Identity struct
        // split across a chunk boundary just falls back to "unknown" (see
        // FwIdentity.h), which is already the permissive, allowed outcome.
        if (!sOtaIdentityFoundAny && fwid::find(data, len, &found)) {
          sOtaIdentityFoundAny = true;
          if (!fwid::matches(found, g_fwIdentity.project)) {
            sOtaIdentityMismatch = true;
            Update.abort();
            Serial.printf("[webbase/ota] REJECTED: uploaded firmware is '%s', this device runs '%s'\n",
                          found.project, g_fwIdentity.project);
          }
        }
      }
      if (Update.isRunning() && !sOtaIdentityMismatch) Update.write(data, len);
      if (final && !sOtaIdentityMismatch) {
        if (Update.end(true)) {
          Serial.printf("[webbase/ota] done  %u bytes\n", (unsigned)(index + len));
        } else {
          Serial.printf("[webbase/ota] error: %s\n", Update.errorString());
        }
      }
    }
  );
}

// ---------------------------------------------------------------------------
//  attachSerial() — WebSocket serial bridge on /ws/webbase/serial.
//  Use webbase.log() / webbase.logf() to emit messages visible in browser.
// ---------------------------------------------------------------------------
void WebBase::attachSerial(AsyncWebServer& server) {
  if (ws_) return;
  ws_ = new AsyncWebSocket("/ws/webbase/serial");
  ws_->onEvent([](AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType t,
                  void*, uint8_t*, size_t) {
    if (t == WS_EVT_CONNECT)
      Serial.printf("[webbase/serial] client %u connected\n", c->id());
  });
  server.addHandler(ws_);
}

// ---------------------------------------------------------------------------
//  attachSettings() — NVS settings API and factory-mode trigger.
//    GET  /api/webbase/settings  — read current values
//    POST /api/webbase/settings  — update WiFi, device name, serial toggle
//    POST /api/webbase/factory   — return to FlashLight factory firmware
// ---------------------------------------------------------------------------
void WebBase::attachSettings(AsyncWebServer& server) {
  server.on("/api/webbase/settings", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["device_name"] = gDeviceName;
    JsonArray nets = doc["wifi_nets"].to<JsonArray>();
    for (uint8_t i = 0; i < webbase_nvs::MAX_WIFI_NETWORKS; i++) {
      JsonObject n = nets.add<JsonObject>();
      n["ssid"] = gNets[i].ssid;
    }
    doc["ap_mode"]   = (gApMode == ApMode::AlwaysOn) ? "always" : "auto";
    doc["serial_en"] = gSerialEn;
    String body; serializeJson(doc, body);
    req->send(200, "application/json", body);
  });

  server.on("/api/webbase/settings", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (gOtaPass.length() > 0 && !req->authenticate("admin", gOtaPass.c_str())) {
        return req->requestAuthentication("WebBase", true);
      }
      req->send(200, "application/json", "{\"ok\":true}");
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      if (gOtaPass.length() > 0 && !req->authenticate("admin", gOtaPass.c_str())) return;
      JsonDocument doc;
      if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        req->send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
      }
      Preferences prefs;
      prefs.begin(webbase_nvs::NS, false);
      if (doc["wifi_nets"].is<JsonArray>()) {
        static const char* ssidKeys[] = {webbase_nvs::WIFI_SSID, webbase_nvs::WIFI_SSID2, webbase_nvs::WIFI_SSID3};
        static const char* passKeys[] = {webbase_nvs::WIFI_PASS, webbase_nvs::WIFI_PASS2, webbase_nvs::WIFI_PASS3};
        JsonArray nets = doc["wifi_nets"].as<JsonArray>();
        uint8_t i = 0;
        for (JsonObject n : nets) {
          if (i >= webbase_nvs::MAX_WIFI_NETWORKS) break;
          if (n["ssid"].is<const char*>()) {
            gNets[i].ssid = n["ssid"].as<String>();
            prefs.putString(ssidKeys[i], gNets[i].ssid);
          }
          if (n["pass"].is<const char*>()) {
            String p = n["pass"].as<String>();
            if (p.length() > 0 && p != "••••••••") {
              gNets[i].pass = p;
              prefs.putString(passKeys[i], gNets[i].pass);
            }
          }
          i++;
        }
      }
      if (doc["ap_mode"].is<const char*>()) {
        gApMode = (doc["ap_mode"].as<String>() == "always") ? ApMode::AlwaysOn : ApMode::AutoFallback;
        prefs.putUChar(webbase_nvs::AP_MODE, static_cast<uint8_t>(gApMode));
      }
      if (doc["device_name"].is<const char*>()) {
        gDeviceName = doc["device_name"].as<String>();
        prefs.putString(webbase_nvs::DEVICE_NAME, gDeviceName);
      }
      if (doc["serial_en"].is<bool>()) {
        gSerialEn = doc["serial_en"].as<bool>();
        prefs.putBool(webbase_nvs::SERIAL_EN, gSerialEn);
      }
      prefs.end();
    }
  );

  server.on("/api/webbase/factory", HTTP_POST,
    [this](AsyncWebServerRequest* req) {
      if (gOtaPass.length() > 0 && !req->authenticate("admin", gOtaPass.c_str())) {
        return req->requestAuthentication("WebBase", true);
      }
      req->send(200, "application/json", "{\"ok\":true}");
      delay(200);
      triggerFactoryMode();
    }
  );
}

// ---------------------------------------------------------------------------
//  log / logf — emit to Serial + WS serial bridge (when enabled)
// ---------------------------------------------------------------------------
void WebBase::log(const String& msg) {
  Serial.println(msg);
  if (ws_ && gSerialEn && ws_->count() > 0) {
    String line = msg + "\n";
    ws_->textAll(line);
  }
}

void WebBase::logf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  log(String(buf));
}

void WebBase::broadcastSerial(const char* data, size_t len) {
  if (ws_ && gSerialEn && ws_->count() > 0) {
    ws_->textAll(data, len);
  }
}

// ---------------------------------------------------------------------------
//  triggerFactoryMode() — sets boot partition to factory, then restarts.
// ---------------------------------------------------------------------------
void WebBase::triggerFactoryMode() {
  Preferences prefs;
  prefs.begin(webbase_nvs::NS, false);
  prefs.putBool(webbase_nvs::FORCE_FACTORY, true);
  prefs.end();

  const esp_partition_t* factory = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
  if (factory) esp_ota_set_boot_partition(factory);

  ESP.restart();
  while (true) {}
}

String WebBase::deviceName()    const { return gDeviceName; }
String WebBase::ip()            const { return apActive_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
bool   WebBase::serialEnabled() const { return gSerialEn; }

// ---------------------------------------------------------------------------
//  attachUI() — self-contained management page embedded as a string literal.
//  No LittleFS upload required. Registers at `path` (default /webbase).
// ---------------------------------------------------------------------------
static const char WEBBASE_UI[] = R"html(<!doctype html>
<html lang="en"><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Device Management</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0f1117;color:#e2e8f0;min-height:100vh}
.tb{background:#1a1f2e;padding:.9rem 1.5rem;display:flex;align-items:center;gap:.75rem;border-bottom:1px solid #2d3748}
.tb .logo{font-size:1.1rem;font-weight:700;color:#63b3ed}.tb .sub{font-size:.85rem;color:#718096}
main{max-width:540px;margin:1.5rem auto;padding:0 1rem 3rem}
.card{background:#1a1f2e;border:1px solid #2d3748;border-radius:10px;padding:1.2rem;margin-bottom:1rem}
h2{font-size:.8rem;font-weight:600;color:#a0aec0;text-transform:uppercase;letter-spacing:.06em;margin-bottom:.9rem}
.row{display:flex;justify-content:space-between;padding:.3rem 0;border-bottom:1px solid #2d374830;font-size:.8rem}
.row:last-child{border:none}.lbl{color:#718096}.val{font-family:monospace;color:#e2e8f0}
.field{margin-bottom:.8rem}label{display:block;font-size:.75rem;color:#718096;margin-bottom:.3rem}
input[type=text]{width:100%;background:#0f1117;border:1px solid #2d3748;color:#e2e8f0;padding:.45rem .7rem;border-radius:6px;font-size:.875rem}
input:focus{outline:none;border-color:#3182ce}
.drop{border:2px dashed #2d3748;border-radius:8px;padding:1.5rem;text-align:center;cursor:pointer;transition:border-color .2s;font-size:.875rem;color:#718096}
.drop:hover,.drop.drag{border-color:#3182ce;background:#3182ce11}
.bar-wrap{height:6px;background:#2d3748;border-radius:3px;overflow:hidden;margin-top:.75rem;display:none}
.bar-fill{height:100%;background:#3182ce;width:0;transition:width .3s}
.toggle-row{display:flex;align-items:center;justify-content:space-between}
.toggle{width:42px;height:23px;background:#2d3748;border-radius:12px;position:relative;cursor:pointer;transition:background .2s}
.toggle.on{background:#3182ce}.toggle::after{content:'';position:absolute;width:17px;height:17px;background:#fff;border-radius:9px;top:3px;left:3px;transition:left .2s}
.toggle.on::after{left:22px}
.btn-row{display:flex;gap:.6rem;flex-wrap:wrap}
button{border:none;padding:.5rem 1rem;border-radius:6px;cursor:pointer;font-size:.8rem;font-weight:500}
.bp{background:#3182ce;color:#fff}.bp:hover{background:#2b6cb0}
.bg{background:#2d3748;color:#a0aec0}.bg:hover{background:#4a5568}
.bd{background:#c53030;color:#fff}.bd:hover{background:#9b2c2c}
#ota-status{font-size:.8rem;margin-top:.5rem;min-height:1em}
.ok{color:#48bb78}.err{color:#fc8181}
input[type=file]{display:none}
</style></head><body>
<div class="tb"><span class="logo">FlashLight</span><span class="sub" id="devname"></span></div>
<main>
<div class="card"><h2>Device</h2>
  <div class="row"><span class="lbl">Name</span><span class="val" id="i-name">—</span></div>
  <div class="row"><span class="lbl">IP</span><span class="val" id="i-ip">—</span></div>
  <div class="row"><span class="lbl">Heap</span><span class="val" id="i-heap">—</span></div>
  <div class="row"><span class="lbl">Firmware</span><span class="val" id="i-fw">—</span></div>
</div>
<div class="card"><h2>Firmware Update</h2>
  <div class="drop" id="drop" onclick="document.getElementById('fi').click()">
    Drop firmware.bin here or click to browse
  </div>
  <input type="file" id="fi" accept=".bin" onchange="upload(this.files[0])"/>
  <div class="bar-wrap" id="bw"><div class="bar-fill" id="bf"></div></div>
  <div id="ota-status"></div>
</div>
<div class="card"><h2>Settings</h2>
  <div class="field"><label>Device Name</label>
    <input type="text" id="dname" autocapitalize="none"/>
  </div>
  <div class="field"><div class="toggle-row">
    <label style="margin:0">Serial Bridge</label>
    <div class="toggle" id="stog" onclick="togSer()"></div>
  </div></div>
  <div class="btn-row" style="margin-top:.5rem">
    <button class="bp" onclick="saveSettings()">Save Settings</button>
  </div>
  <div id="set-msg" style="font-size:.8rem;margin-top:.5rem"></div>
</div>
<div class="card"><h2>FlashLight</h2>
  <p style="font-size:.8rem;color:#718096;margin-bottom:.85rem">Return to FlashLight factory firmware to reconfigure WiFi, upload new firmware, or access the serial monitor.</p>
  <div class="btn-row">
    <button class="bd" onclick="goFactory()">Return to FlashLight</button>
    <button class="bg" onclick="window.location='/'">Back to App</button>
  </div>
</div>
</main>
<script>
let serOn=false;
const drop=document.getElementById('drop');
drop.addEventListener('dragover',e=>{e.preventDefault();drop.classList.add('drag')});
drop.addEventListener('dragleave',()=>drop.classList.remove('drag'));
drop.addEventListener('drop',e=>{e.preventDefault();drop.classList.remove('drag');upload(e.dataTransfer.files[0])});
function upload(f){
  if(!f)return;
  const st=document.getElementById('ota-status'),bw=document.getElementById('bw'),bf=document.getElementById('bf');
  st.textContent='Uploading '+f.name+'…';st.className='';bw.style.display='block';bf.style.width='0%';
  const fd=new FormData();fd.append('firmware',f,f.name);
  const x=new XMLHttpRequest();
  x.upload.onprogress=e=>{if(e.lengthComputable)bf.style.width=(e.loaded/e.total*95)+'%'};
  x.onload=()=>{try{const r=JSON.parse(x.responseText);if(r.ok){bf.style.width='100%';st.textContent='Upload complete — rebooting…';st.className='ok';}else{st.textContent='Error: '+(r.error||'?');st.className='err';}}catch{st.textContent='Unexpected response';st.className='err';}};
  x.onerror=()=>{st.textContent='Network error';st.className='err'};
  x.open('POST','/ota');x.send(fd);
}
function togSer(){serOn=!serOn;document.getElementById('stog').className='toggle'+(serOn?' on':'')}
async function load(){
  try{
    const d=await fetch('/api/info').then(r=>r.json());
    document.getElementById('i-name').textContent=d.device_name||'—';
    document.getElementById('i-ip').textContent=d.ip||'—';
    document.getElementById('i-heap').textContent=d.free_heap?(d.free_heap/1024).toFixed(1)+' KB':'—';
    document.getElementById('i-fw').textContent=d.firmware||'—';
    document.getElementById('devname').textContent=d.device_name||'';
  }catch{}
  try{
    const s=await fetch('/api/webbase/settings').then(r=>r.json());
    document.getElementById('dname').value=s.device_name||'';
    serOn=!!s.serial_en;
    document.getElementById('stog').className='toggle'+(serOn?' on':'');
  }catch{}
}
async function saveSettings(){
  const msg=document.getElementById('set-msg');
  try{
    const r=await fetch('/api/webbase/settings',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({device_name:document.getElementById('dname').value.trim(),serial_en:serOn})});
    const d=await r.json();
    msg.textContent=d.ok?'Saved.':'Error: '+(d.error||'?');
    msg.className=d.ok?'ok':'err';
  }catch{msg.textContent='Network error';msg.className='err';}
}
async function goFactory(){
  if(!confirm('Return to FlashLight? The device will reboot.'))return;
  await fetch('/api/webbase/factory',{method:'POST'}).catch(()=>{});
}
load();
</script></body></html>)html";

void WebBase::attachUI(AsyncWebServer& server, const char* path) {
  server.on(path, HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", WEBBASE_UI);
  });
}
