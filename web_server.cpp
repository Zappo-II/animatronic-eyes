/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "web_server.h"
#include "config.h"
#include "storage.h"
#include "wifi_manager.h"
#include "servo_controller.h"
#include "led_status.h"
#include "eye_controller.h"
#include "auto_blink.h"
#include "mode_manager.h"
#include "mode_player.h"
#include "impulse_player.h"
#include "auto_impulse.h"
#include "update_checker.h"

#include <ESPAsyncWebServer.h>
#include <stdarg.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_partition.h>

WebServer webServer;

// Filesystem update state (file scope for lambda access)
static const esp_partition_t* fsUpdatePartition = nullptr;
static bool fsUpdateError = false;
static String fsUpdateErrorMsg = "";

// Firmware update auth tracking
static bool fwUpdateAuthFailed = false;

// Restore auth tracking
static bool restoreAuthFailed = false;

// Embedded recovery UI - always available even if LittleFS is corrupted
static const char RECOVERY_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Animatronic Eyes - Recovery</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;padding:20px}
.container{max-width:600px;margin:0 auto}
h1{color:#e94560;margin-bottom:10px;font-size:1.5em}
.subtitle{color:#888;margin-bottom:30px}
.card{background:#16213e;border-radius:12px;padding:20px;margin-bottom:20px}
.card h2{color:#e94560;font-size:1.1em;margin-bottom:15px;display:flex;align-items:center;gap:8px}
.warning{background:#e9456020;border:1px solid #e94560;border-radius:8px;padding:15px;margin-bottom:20px;font-size:0.9em}
.warning strong{color:#e94560}
.info{background:#0f3460;border-radius:8px;padding:15px;margin-bottom:15px;font-size:0.9em}
.info code{background:#1a1a2e;padding:2px 6px;border-radius:4px;font-size:0.85em}
label{display:block;margin-bottom:8px;color:#aaa;font-size:0.9em}
input[type="file"]{width:100%;padding:12px;background:#0f3460;border:2px dashed #e94560;border-radius:8px;color:#eee;margin-bottom:15px;cursor:pointer}
input[type="file"]:hover{background:#1a4080}
.btn{display:inline-block;padding:12px 24px;border:none;border-radius:8px;font-size:1em;cursor:pointer;transition:all 0.2s}
.btn-primary{background:#e94560;color:#fff}
.btn-primary:hover{background:#ff6b6b}
.btn-primary:disabled{background:#666;cursor:not-allowed}
.btn-secondary{background:#0f3460;color:#eee;margin-left:10px}
.btn-secondary:hover{background:#1a4080}
.progress{display:none;margin-top:15px}
.progress-bar{height:20px;background:#0f3460;border-radius:10px;overflow:hidden}
.progress-fill{height:100%;background:#e94560;width:0%;transition:width 0.3s}
.progress-text{text-align:center;margin-top:8px;font-size:0.9em;color:#aaa}
.status{margin-top:15px;padding:10px;border-radius:8px;display:none}
.status.success{display:block;background:#2e7d3220;border:1px solid #2e7d32;color:#4caf50}
.status.error{display:block;background:#e9456020;border:1px solid #e94560;color:#e94560}
.version-info{display:flex;justify-content:space-between;padding:10px;background:#0f3460;border-radius:8px;margin-bottom:15px;font-size:0.9em}
.version-info span:first-child{color:#888}
a{color:#e94560}
</style>
</head>
<body>
<div class="container">
<h1>Recovery Mode</h1>
<p class="subtitle">Upload firmware or UI files</p>

<div class="warning" id="status-banner" style="display:none">
<strong id="status-title">Status:</strong> <span id="status-message">Loading...</span>
</div>

<div class="info" style="margin-bottom:20px">
<strong>Note:</strong> This recovery page is shown when the main UI cannot be loaded safely.
</div>

<div class="card">
<h2>System Info</h2>
<div class="version-info"><span>Firmware</span><span id="fw-version">-</span></div>
<div class="version-info"><span>Min UI Required</span><span id="min-ui-version">-</span></div>
<div class="version-info"><span>UI Version</span><span id="ui-version">-</span></div>
<div class="version-info"><span>UI Requires Firmware</span><span id="ui-min-fw">-</span></div>
<div class="version-info"><span>Free Heap</span><span id="free-heap">-</span></div>
<div class="version-info" id="update-row" style="display:none;color:#f39c12"><span>Update Available</span><span id="update-version">-</span></div>
</div>

<div class="warning" id="lock-banner" style="display:none">
<strong>Admin Lock Active:</strong> Some actions are disabled.
<div style="margin-top:10px;display:flex;gap:8px;align-items:center;flex-wrap:wrap">
<input type="password" id="pin-input" placeholder="Enter PIN" maxlength="6" inputmode="numeric" style="width:100px;padding:8px;border-radius:4px;border:1px solid #e94560;background:#0f3460;color:#eee">
<button class="btn btn-primary" onclick="unlock()" style="padding:8px 16px">Unlock</button>
<span style="color:#888;font-size:0.85em">or connect via AP (192.168.4.1)</span>
</div>
<div id="pin-error" style="color:#ff6b6b;margin-top:8px;font-size:0.9em"></div>
</div>

<div class="card">
<h2>Actions</h2>
<div style="display:flex;flex-wrap:wrap;gap:10px">
<button class="btn btn-secondary" onclick="location.href='/'">Go to Main UI</button>
<button class="btn btn-secondary" onclick="downloadBackup()" id="backup-btn">Download Backup</button>
<button class="btn btn-secondary" onclick="reboot()" id="reboot-btn">Reboot</button>
</div>
</div>

<div class="card">
<h2>Upload Firmware (.bin)</h2>
<div class="info">Upload a new firmware binary to update the device.<br>
Create with: <code>Sketch → Export Compiled Binary</code> in Arduino IDE.<br>
Use the main .bin file (not bootloader/partitions), e.g. <code>animatronic-eyes.ino.bin</code></div>
<form id="fw-form">
<input type="file" id="fw-file" accept=".bin">
<button type="submit" class="btn btn-primary" id="fw-btn">Upload Firmware</button>
</form>
<div class="progress" id="fw-progress">
<div class="progress-bar"><div class="progress-fill" id="fw-fill"></div></div>
<div class="progress-text" id="fw-text">0%</div>
</div>
<div class="status" id="fw-status"></div>
</div>

<div class="card">
<h2>Upload UI Files (.bin)</h2>
<div class="info">
Upload a LittleFS image containing UI files.<br>
Create with: <code id="mklittlefs-cmd">mklittlefs -c data/ -p 256 -b 4096 -s ... ui.bin</code>
</div>
<form id="ui-form">
<input type="file" id="ui-file" accept=".bin">
<button type="submit" class="btn btn-primary" id="ui-btn">Upload UI</button>
</form>
<div class="progress" id="ui-progress">
<div class="progress-bar"><div class="progress-fill" id="ui-fill"></div></div>
<div class="progress-text" id="ui-text">0%</div>
</div>
<div class="status" id="ui-status"></div>
</div>

<div class="card danger-zone">
<h2>Danger Zone</h2>
<div class="info">These actions cannot be undone.</div>
<div style="display:flex;flex-wrap:wrap;gap:10px">
<button class="btn btn-secondary" onclick="wipeUI()" id="wipe-btn" style="background:#c0392b">Wipe UI Files</button>
<button class="btn btn-secondary" onclick="factoryReset()" style="background:#c0392b">Factory Reset</button>
</div>
</div>
</div>

<script>
let isLocked = false;
let lockoutSeconds = 0;

async function checkAdminStatus() {
  try {
    const r = await fetch('/api/admin-status');
    const d = await r.json();
    isLocked = d.locked;
    lockoutSeconds = d.lockoutSeconds || 0;
    if (isLocked) {
      document.getElementById('lock-banner').style.display = 'block';
      // Disable protected controls
      ['fw-btn', 'ui-btn', 'backup-btn', 'wipe-btn'].forEach(id => {
        const el = document.getElementById(id);
        if (el) { el.disabled = true; el.style.opacity = '0.5'; el.style.cursor = 'not-allowed'; }
      });
      // Also disable file inputs
      ['fw-file', 'ui-file'].forEach(id => {
        const el = document.getElementById(id);
        if (el) { el.disabled = true; el.style.opacity = '0.5'; }
      });
    }
    // Reboot: allowed when locked, blocked only when rate limited
    const rebootBtn = document.getElementById('reboot-btn');
    if (rebootBtn && lockoutSeconds > 0) {
      rebootBtn.disabled = true;
      rebootBtn.style.opacity = '0.5';
      rebootBtn.style.cursor = 'not-allowed';
    }
  } catch(e) { console.error('Admin status check failed:', e); }
}

async function unlock() {
  const pin = document.getElementById('pin-input').value;
  const errEl = document.getElementById('pin-error');
  errEl.textContent = '';
  if (!pin) { errEl.textContent = 'Enter PIN'; return; }
  try {
    const r = await fetch('/api/unlock', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({pin})
    });
    if (r.ok) {
      location.reload();
    } else {
      errEl.textContent = await r.text();
    }
  } catch(e) { errEl.textContent = 'Request failed'; }
}

async function loadInfo() {
  try {
    const r = await fetch('/api/version');
    const d = await r.json();
    document.getElementById('fw-version').textContent = d.version || '-';
    document.getElementById('min-ui-version').textContent = d.minUiVersion || '-';
    document.getElementById('ui-version').textContent = d.uiVersion || 'Not installed';
    document.getElementById('ui-min-fw').textContent = d.uiMinFirmware || '-';
    document.getElementById('free-heap').textContent = d.freeHeap ? (d.freeHeap/1024).toFixed(1)+' KB' : '-';
    if (d.partitionSize) {
      const hex = '0x' + d.partitionSize.toString(16).toUpperCase();
      document.getElementById('mklittlefs-cmd').textContent = 'mklittlefs -c data/ -p 256 -b 4096 -s ' + hex + ' ui.bin';
    }
    // Show status banner with appropriate message
    const banner = document.getElementById('status-banner');
    const title = document.getElementById('status-title');
    const msg = document.getElementById('status-message');
    const status = d.uiStatus;
    if (status === 'missing') {
      banner.style.display = 'block';
      title.textContent = 'UI Missing:';
      msg.textContent = 'No UI files found. Upload a UI image below.';
    } else if (status === 'fw_too_old') {
      banner.style.display = 'block';
      title.textContent = 'Firmware Too Old:';
      msg.textContent = 'UI requires firmware ' + d.uiMinFirmware + ' but device has ' + d.version + '. Upload newer firmware below.';
    } else if (status === 'ui_too_old') {
      banner.style.display = 'block';
      title.textContent = 'UI Too Old:';
      msg.textContent = 'Firmware requires UI ' + d.minUiVersion + ' but device has ' + d.uiVersion + '. Upload newer UI below.';
    }
    // Show update available if cached
    if (d.updateAvailable && d.updateVersion) {
      document.getElementById('update-row').style.display = 'flex';
      const link = document.createElement('a');
      link.href = 'https://github.com/Zappo-II/animatronic-eyes/releases';
      link.target = '_blank';
      link.textContent = 'v' + d.updateVersion;
      link.style.color = '#f39c12';
      const span = document.getElementById('update-version');
      span.innerHTML = '';
      span.appendChild(link);
    }
  } catch(e) { console.error(e); }
}

function upload(formId, fileId, endpoint, progressId, fillId, textId, statusId, btnId) {
  const form = document.getElementById(formId);
  const fileInput = document.getElementById(fileId);
  const progress = document.getElementById(progressId);
  const fill = document.getElementById(fillId);
  const text = document.getElementById(textId);
  const status = document.getElementById(statusId);
  const btn = document.getElementById(btnId);

  form.onsubmit = async (e) => {
    e.preventDefault();
    const file = fileInput.files[0];
    if (!file) { alert('Select a file first'); return; }

    btn.disabled = true;
    progress.style.display = 'block';
    status.className = 'status';
    status.style.display = 'none';

    let uploadComplete = false;
    let handled = false;

    const xhr = new XMLHttpRequest();
    xhr.open('POST', endpoint, true);

    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable) {
        const pct = Math.round((e.loaded / e.total) * 100);
        fill.style.width = pct + '%';
        if (pct === 100) {
          uploadComplete = true;
          text.textContent = 'Processing...';
        } else {
          text.textContent = pct + '%';
        }
      }
    };

    const showSuccess = () => {
      if (handled) return;
      handled = true;
      btn.disabled = false;
      status.className = 'status success';
      status.textContent = 'Upload successful! Device will restart...';
      setTimeout(() => location.href = '/', 4000);
    };

    const showError = (msg) => {
      if (handled) return;
      handled = true;
      btn.disabled = false;
      status.className = 'status error';
      status.textContent = 'Upload failed: ' + msg;
    };

    xhr.onload = () => {
      if (xhr.status === 200 && xhr.responseText === 'OK') {
        showSuccess();
      } else {
        showError(xhr.responseText || 'Unknown error');
      }
    };

    xhr.onerror = () => {
      if (uploadComplete) {
        showSuccess();
      } else {
        showError('Network error');
      }
    };

    // Fallback: if upload reached 100% and no response after 2s, assume success
    setTimeout(() => {
      if (uploadComplete && !handled) showSuccess();
    }, 2000);

    const formData = new FormData();
    formData.append('file', file, file.name);
    xhr.send(formData);
  };
}

async function reboot() {
  if (!confirm('Reboot device?')) return;
  fetch('/api/reboot', {method:'POST'}).catch(() => {});
  alert('Device rebooting...');
  setTimeout(() => location.href = '/', 3000);
}

async function downloadBackup() {
  if (isLocked) { alert('Admin lock active. Connect via AP to unlock.'); return; }
  try {
    const r = await fetch('/api/backup');
    if (!r.ok) throw new Error(await r.text());
    const backup = await r.json();
    const blob = new Blob([JSON.stringify(backup, null, 2)], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    const fn = 'animatronic-eyes-' + (backup.device || 'unknown') + '-' + ts + '.json';
    const a = document.createElement('a');
    a.href = url; a.download = fn;
    document.body.appendChild(a); a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    alert('Backup downloaded');
  } catch(e) { alert('Backup failed: ' + e.message); }
}

async function wipeUI() {
  if (isLocked) { alert('Admin lock active. Connect via AP to unlock.'); return; }
  if (!confirm('Wipe all UI files? You will need to upload a new UI image.')) return;
  try {
    const r = await fetch('/api/wipe-ui', {method:'POST'});
    if (r.ok) {
      alert('UI files wiped.');
      location.href = '/';
    } else {
      alert('Wipe failed: ' + await r.text());
    }
  } catch(e) { alert('Wipe request failed'); }
}

async function factoryReset() {
  if (!confirm('Factory reset will erase ALL settings including WiFi credentials. Continue?')) return;
  if (!confirm('Are you really sure? This cannot be undone!')) return;
  fetch('/api/factory-reset', {method:'POST'}).catch(() => {});
  alert('Factory reset complete. Device rebooting...');
  setTimeout(() => location.href = '/', 3000);
}

upload('fw-form', 'fw-file', '/update', 'fw-progress', 'fw-fill', 'fw-text', 'fw-status', 'fw-btn');
upload('ui-form', 'ui-file', '/api/upload-ui', 'ui-progress', 'ui-fill', 'ui-text', 'ui-status', 'ui-btn');
loadInfo();
checkAdminStatus();
</script>
</body>
</html>
)rawliteral";

static AsyncWebServer server(HTTP_PORT);
static AsyncWebSocket ws(WEBSOCKET_PATH);

static const char* SERVO_NAMES[NUM_SERVOS] = {
    "Left Eye X",
    "Left Eye Y",
    "Left Eyelid",
    "Right Eye X",
    "Right Eye Y",
    "Right Eyelid"
};

void WebServer::begin() {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        WEB_LOG("WebServer", "ERROR: LittleFS mount failed");
        _uiFilesValid = false;
    } else {
        WEB_LOG("WebServer", "LittleFS mounted");
        _uiFilesValid = checkUIFiles();
        loadUIVersionInfo();
        WEB_LOG("WebServer", "UI files valid: %s, version: %s, minFirmware: %s, status: %s",
                     _uiFilesValid ? "yes" : "no", _uiVersion.c_str(),
                     _uiMinFirmware.c_str(), getUIStatus().c_str());
    }

    setupWebSocket();
    setupRoutes();

    server.begin();
    WEB_LOG("WebServer", "Started on port %d", HTTP_PORT);
}

bool WebServer::checkUIFiles() {
    // Check for required UI files
    if (!LittleFS.exists("/index.html")) {
        WEB_LOG("WebServer", "Missing: index.html");
        return false;
    }
    if (!LittleFS.exists("/style.css")) {
        WEB_LOG("WebServer", "Missing: style.css");
        return false;
    }
    if (!LittleFS.exists("/app.js")) {
        WEB_LOG("WebServer", "Missing: app.js");
        return false;
    }
    if (!LittleFS.exists("/version.json")) {
        WEB_LOG("WebServer", "Missing: version.json");
        return false;
    }
    return true;
}

void WebServer::loadUIVersionInfo() {
    _uiVersion = "unknown";
    _uiMinFirmware = "";

    if (!LittleFS.exists("/version.json")) {
        return;
    }

    File f = LittleFS.open("/version.json", "r");
    if (!f) {
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();

    if (error) {
        WEB_LOG("WebServer", "version.json parse error: %s", error.c_str());
        _uiVersion = "invalid";
        return;
    }

    const char* version = doc["version"];
    const char* minFirmware = doc["minFirmware"];
    _uiVersion = version ? String(version) : "unknown";
    _uiMinFirmware = minFirmware ? String(minFirmware) : "";
}

// Helper to parse version into components
static void parseVersion(const String& ver, int& major, int& minor, int& patch) {
    major = minor = patch = 0;
    int dot1 = ver.indexOf('.');
    if (dot1 < 0) { major = ver.toInt(); return; }
    major = ver.substring(0, dot1).toInt();
    int dot2 = ver.indexOf('.', dot1 + 1);
    if (dot2 < 0) { minor = ver.substring(dot1 + 1).toInt(); return; }
    minor = ver.substring(dot1 + 1, dot2).toInt();
    patch = ver.substring(dot2 + 1).toInt();
}

String WebServer::getUIStatus() {
    // Returns (worst to best):
    // - "missing": UI files not found → Recovery
    // - "fw_too_old": Firmware < UI's minFirmware → Recovery
    // - "ui_too_old": UI < Firmware's MIN_UI_VERSION → Recovery
    // - "major_mismatch": Major version differs (soft warning, red)
    // - "minor_mismatch": Minor version differs (soft warning, yellow)
    // - "ok": Versions match (green)

    if (!_uiFilesValid || _uiVersion == "unknown" || _uiVersion == "invalid") {
        return "missing";
    }

    String fwVersion = FIRMWARE_VERSION;
    String minUiVersion = MIN_UI_VERSION;
    int fwMajor, fwMinor, fwPatch;
    int uiMajor, uiMinor, uiPatch;
    parseVersion(fwVersion, fwMajor, fwMinor, fwPatch);
    parseVersion(_uiVersion, uiMajor, uiMinor, uiPatch);

    // Check if firmware is too old for this UI (UI's minFirmware requirement)
    if (_uiMinFirmware.length() > 0) {
        int minFwMajor, minFwMinor, minFwPatch;
        parseVersion(_uiMinFirmware, minFwMajor, minFwMinor, minFwPatch);

        // Firmware must be >= minFirmware
        if (fwMajor < minFwMajor ||
            (fwMajor == minFwMajor && fwMinor < minFwMinor) ||
            (fwMajor == minFwMajor && fwMinor == minFwMinor && fwPatch < minFwPatch)) {
            return "fw_too_old";  // Firmware too old for this UI
        }
    }

    // Check if UI is too old for this firmware (Firmware's MIN_UI_VERSION requirement)
    int minUiMajor, minUiMinor, minUiPatch;
    parseVersion(minUiVersion, minUiMajor, minUiMinor, minUiPatch);

    if (uiMajor < minUiMajor ||
        (uiMajor == minUiMajor && uiMinor < minUiMinor) ||
        (uiMajor == minUiMajor && uiMinor == minUiMinor && uiPatch < minUiPatch)) {
        return "ui_too_old";  // UI too old for this firmware
    }

    // Soft warnings for version mismatch (still usable)
    if (fwMajor != uiMajor) {
        return "major_mismatch";
    }
    if (fwMinor != uiMinor) {
        return "minor_mismatch";
    }

    return "ok";
}

void WebServer::serveRecoveryPage(AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", RECOVERY_HTML);
}

void WebServer::loop() {
    ws.cleanupClients();

    // Handle deferred broadcast requests from async context
    if (_broadcastRequested) {
        _broadcastRequested = false;
        if (ws.count() > 0) {
            broadcastState();
        }
        _lastBroadcast = millis();
        return;
    }

    if (millis() - _lastBroadcast > WS_BROADCAST_INTERVAL_MS) {
        _lastBroadcast = millis();
        if (ws.count() > 0) {
            broadcastState();
        }
    }
}

void WebServer::requestBroadcast() {
    _broadcastRequested = true;
}

void WebServer::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                WEB_LOG("WS", "Client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
                WEB_LOG("WS", "Total clients: %u", ws.count());
                sendAvailableLists(client);  // Send available modes/impulses to new client
                sendAdminState(client);      // Send per-client admin lock state
                requestBroadcast();          // Defer state broadcast to main loop
                break;
            case WS_EVT_DISCONNECT:
                WEB_LOG("WS", "Client #%u disconnected", client->id());
                WEB_LOG("WS", "Total clients: %u", ws.count());
                // Don't remove auth - keep IP authenticated for page reloads and recovery UI
                // Entry will expire naturally via timeout
                break;
            case WS_EVT_DATA: {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    data[len] = 0;
                    handleWebSocketMessage((char*)data, client);
                }
                break;
            }
            case WS_EVT_ERROR:
                WEB_LOG("WS", "Client #%u error: %u", client->id(), *((uint16_t*)arg));
                break;
            case WS_EVT_PONG:
                break;
        }
    });

    server.addHandler(&ws);
}

void WebServer::setupRoutes() {

    // Recovery page - always available
    server.on("/recovery", HTTP_GET, [this](AsyncWebServerRequest* request) {
        serveRecoveryPage(request);
    });

    // Serve index.html - check UI status, redirect to recovery for hard failures
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        String status = getUIStatus();

        // Hard failures → redirect to recovery
        if (status == "missing" || status == "fw_too_old" || status == "ui_too_old") {
            WEB_LOG("WebServer", "Redirecting to recovery: %s", status.c_str());
            request->redirect("/recovery");
            return;
        }

        // Soft warnings or OK → serve UI
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            serveRecoveryPage(request);
        }
    });

    // OTA firmware upload
    server.on("/update", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // Check for auth failure first
            if (fwUpdateAuthFailed) {
                request->send(403, "text/plain", "FAIL: Admin lock active");
                return;
            }
            bool success = !Update.hasError();
            request->send(200, "text/plain", success ? "OK" : "FAIL");
            if (success) {
                WEB_LOG("OTA", "Firmware update success, signaling reboot...");
                storage.setRebootRequired(true);
                broadcastState();  // Notify UI before reboot
                delay(500);
                ESP.restart();
            }
        },
        [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            // Auth check at start of upload
            if (index == 0) {
                fwUpdateAuthFailed = false;  // Reset flag
                if (!isHttpRequestAuthorized(request)) {
                    WEB_LOG("Admin", "Firmware upload blocked: not authorized");
                    fwUpdateAuthFailed = true;
                    return;
                }
                WEB_LOG("OTA", "Starting firmware update: %s", filename.c_str());
                // Stop all servo activity during upload
                autoBlink.pause();
                autoImpulse.pause();
                impulsePlayer.stop();
                modeManager.setMode(Mode::NONE);
                ledStatus.veryFastBlink();  // OTA indicator
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (fwUpdateAuthFailed || !Update.isRunning()) return;  // Skip if auth failed or not started
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    WEB_LOG("OTA", "Firmware update complete: %u bytes", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    // LittleFS filesystem upload - direct partition write (bypasses Update library issues)
    server.on("/api/upload-ui", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            WEB_LOG("OTA", "Upload done - error: %d, partition: %p", fsUpdateError, fsUpdatePartition);
            if (!fsUpdateError && fsUpdatePartition != nullptr) {
                request->send(200, "text/plain", "OK");
                WEB_LOG("OTA", "Filesystem update success, signaling reboot...");
                storage.setRebootRequired(true);
                broadcastState();  // Notify UI before reboot
                delay(500);
                ESP.restart();
            } else {
                String msg = "FAIL: " + (fsUpdateErrorMsg.length() > 0 ? fsUpdateErrorMsg : "Unknown error");
                WEB_LOG("OTA", "%s", msg.c_str());
                request->send(500, "text/plain", msg);
            }
        },
        [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                // Auth check at start of upload
                if (!isHttpRequestAuthorized(request)) {
                    WEB_LOG("Admin", "UI upload blocked: not authorized");
                    fsUpdateError = true;
                    fsUpdateErrorMsg = "Admin lock active";
                    // Don't send response here - let completion handler do it
                    return;
                }
                WEB_LOG("OTA", "Starting filesystem update: %s (%u bytes)", filename.c_str(), request->contentLength());
                // Stop all servo activity during upload
                autoBlink.pause();
                autoImpulse.pause();
                impulsePlayer.stop();
                modeManager.setMode(Mode::NONE);
                ledStatus.veryFastBlink();
                fsUpdateError = false;
                fsUpdateErrorMsg = "";
                fsUpdatePartition = nullptr;

                // Find the spiffs partition
                fsUpdatePartition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
                if (!fsUpdatePartition) {
                    WEB_LOG("OTA", "ERROR: SPIFFS partition not found");
                    fsUpdateError = true;
                    fsUpdateErrorMsg = "SPIFFS partition not found";
                    return;
                }

                WEB_LOG("OTA", "Found partition: %s at 0x%x, size: 0x%x",
                    fsUpdatePartition->label, fsUpdatePartition->address, fsUpdatePartition->size);

                // Unmount LittleFS before writing
                LittleFS.end();
                WEB_LOG("OTA", "LittleFS unmounted");

                // Erase the partition
                esp_err_t err = esp_partition_erase_range(fsUpdatePartition, 0, fsUpdatePartition->size);
                if (err != ESP_OK) {
                    WEB_LOG("OTA", "ERROR: Failed to erase partition: %s", esp_err_to_name(err));
                    fsUpdateError = true;
                    fsUpdateErrorMsg = "Failed to erase: " + String(esp_err_to_name(err));
                    return;
                }
                WEB_LOG("OTA", "Partition erased");
            }

            if (fsUpdateError || !fsUpdatePartition) return;

            // Write data to partition
            esp_err_t err = esp_partition_write(fsUpdatePartition, index, data, len);
            if (err != ESP_OK) {
                WEB_LOG("OTA", "ERROR: Write failed at 0x%x: %s", index, esp_err_to_name(err));
                fsUpdateError = true;
                fsUpdateErrorMsg = "Write failed: " + String(esp_err_to_name(err));
                return;
            }

            if (final) {
                WEB_LOG("OTA", "Filesystem write complete: %u bytes", index + len);
            }
        }
    );

    // Simple test endpoint
    server.on("/api/test", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "OK");
    });

    // API endpoint for version info
    server.on("/api/version", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["version"] = FIRMWARE_VERSION;
        doc["minUiVersion"] = MIN_UI_VERSION;
        doc["uiVersion"] = _uiVersion;
        doc["uiMinFirmware"] = _uiMinFirmware;
        doc["uiStatus"] = getUIStatus();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["rebootRequired"] = storage.isRebootRequired();
        doc["partitionSize"] = LITTLEFS_PARTITION_SIZE;
        doc["chipModel"] = ESP.getChipModel();
        doc["chipRevision"] = ESP.getChipRevision();

        // Device ID (derived from chip ID, same as AP suffix)
        uint64_t chipId = ESP.getEfuseMac();
        char deviceId[8];
        snprintf(deviceId, sizeof(deviceId), "%06X", (uint32_t)(chipId & 0xFFFFFF));
        doc["deviceId"] = deviceId;

        // Update check status (for recovery UI)
        doc["updateAvailable"] = updateChecker.isUpdateAvailable();
        doc["updateVersion"] = updateChecker.getAvailableVersion();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API endpoint for reboot (blocked only when rate limited)
    server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request->client()->remoteIP())) {
            WEB_LOG("WebServer", "Reboot blocked: rate limited");
            request->send(429, "text/plain", "Rate limited");
            return;
        }
        WEB_LOG("WebServer", "Reboot requested via API");
        request->send(200, "text/plain", "OK");
        delay(500);
        ESP.restart();
    });

    // Admin status endpoint - for recovery UI to check lock state
    server.on("/api/admin-status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        IPAddress clientIP = request->client()->remoteIP();
        doc["pinConfigured"] = storage.hasAdminPin();
        doc["isAPClient"] = isAPClientIP(clientIP);
        doc["locked"] = !isHttpRequestAuthorized(request);
        doc["lockoutSeconds"] = getRateLimitSeconds(clientIP);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Unlock endpoint - for recovery UI PIN entry
    server.on("/api/unlock", HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            IPAddress clientIP = request->client()->remoteIP();

            // Check rate limit
            if (!checkRateLimit(clientIP)) {
                WEB_LOG("Admin", "Unlock blocked: rate limited");
                request->send(429, "text/plain", "Too many attempts. Try again later.");
                return;
            }

            // Parse JSON body
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);
            if (error) {
                request->send(400, "text/plain", "Invalid request");
                return;
            }

            const char* pin = doc["pin"];
            if (!pin) {
                request->send(400, "text/plain", "PIN required");
                return;
            }

            // Verify PIN
            String storedPin = storage.getAdminPin();
            if (storedPin.length() > 0 && storedPin == pin) {
                authenticateIP(clientIP);
                clearFailedAttempts(clientIP);
                WEB_LOG("Admin", "IP %s authenticated via HTTP", clientIP.toString().c_str());
                request->send(200, "text/plain", "OK");
            } else {
                WEB_LOG("Admin", "Unlock failed: wrong PIN from %s", clientIP.toString().c_str());
                recordFailedAttempt(clientIP);
                request->send(401, "text/plain", "Invalid PIN");
            }
        }
    );

    // Wipe UI files endpoint
    server.on("/api/wipe-ui", HTTP_POST, [this](AsyncWebServerRequest* request) {
        // Auth check
        if (!isHttpRequestAuthorized(request)) {
            WEB_LOG("Admin", "Wipe UI blocked: not authorized");
            request->send(403, "text/plain", "FAIL: Admin lock active");
            return;
        }

        WEB_LOG("WebServer", "Wiping UI files...");

        bool success = true;
        if (LittleFS.exists("/index.html")) success &= LittleFS.remove("/index.html");
        if (LittleFS.exists("/style.css")) success &= LittleFS.remove("/style.css");
        if (LittleFS.exists("/app.js")) success &= LittleFS.remove("/app.js");
        if (LittleFS.exists("/version.json")) success &= LittleFS.remove("/version.json");

        if (success) {
            _uiFilesValid = false;
            _uiVersion = "unknown";
            _uiMinFirmware = "";
            WEB_LOG("WebServer", "UI files wiped successfully");
            request->send(200, "text/plain", "OK");
        } else {
            WEB_LOG("WebServer", "ERROR: Failed to wipe some UI files");
            request->send(500, "text/plain", "Failed to wipe some files");
        }
    });

    // Factory reset endpoint
    server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        WEB_LOG("WebServer", "Factory reset requested");
        request->send(200, "text/plain", "OK");
        ledStatus.strobe();
        storage.factoryReset();
        delay(500);
        ESP.restart();
    });

    // Backup endpoint - download complete device configuration
    server.on("/api/backup", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // Auth check - uses WebSocket session IP correlation
        if (!isHttpRequestAuthorized(request)) {
            WEB_LOG("Admin", "Backup blocked: not authorized");
            request->send(403, "text/plain", "Admin lock active");
            return;
        }
        WEB_LOG("WebServer", "Backup requested");

        JsonDocument doc;
        doc["version"] = FIRMWARE_VERSION;
        doc["type"] = "animatronic-eyes-backup";

        // Device ID
        uint64_t chipId = ESP.getEfuseMac();
        char deviceId[8];
        snprintf(deviceId, sizeof(deviceId), "%06X", (uint32_t)(chipId & 0xFFFFFF));
        doc["device"] = deviceId;

        // Config section
        JsonObject config = doc["config"].to<JsonObject>();

        // Servo config
        JsonArray servos = config["servo"].to<JsonArray>();
        for (int i = 0; i < NUM_SERVOS; i++) {
            ServoConfig sc = storage.getServoConfig(i);
            JsonObject s = servos.add<JsonObject>();
            s["pin"] = sc.pin;
            s["min"] = sc.min;
            s["center"] = sc.center;
            s["max"] = sc.max;
            s["invert"] = sc.invert;
        }

        // WiFi config
        JsonObject wifi = config["wifi"].to<JsonObject>();
        for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
            WifiNetwork net = storage.getWifiNetwork(i);
            if (net.configured) {
                JsonObject netObj = wifi[String("network") + String(i)].to<JsonObject>();
                netObj["ssid"] = net.ssid;
                netObj["password"] = net.password;
            }
        }
        WifiTiming timing = storage.getWifiTiming();
        JsonObject wifiTiming = wifi["timing"].to<JsonObject>();
        wifiTiming["graceMs"] = timing.graceMs;
        wifiTiming["retries"] = timing.retries;
        wifiTiming["retryDelayMs"] = timing.retryDelayMs;
        wifiTiming["apScanMs"] = timing.apScanMs;
        wifiTiming["keepAP"] = timing.keepAP;

        // AP config
        ApConfig apConfig = storage.getApConfig();
        JsonObject ap = config["ap"].to<JsonObject>();
        ap["ssidPrefix"] = apConfig.ssidPrefix;
        ap["password"] = apConfig.password;

        // LED config
        LedConfig ledConfig = storage.getLedConfig();
        JsonObject led = config["led"].to<JsonObject>();
        led["enabled"] = ledConfig.enabled;
        led["pin"] = ledConfig.pin;
        led["brightness"] = ledConfig.brightness;

        // mDNS config
        MdnsConfig mdnsConfig = storage.getMdnsConfig();
        JsonObject mdns = config["mdns"].to<JsonObject>();
        mdns["enabled"] = mdnsConfig.enabled;
        mdns["hostname"] = mdnsConfig.hostname;

        // Mode config
        ModeConfig modeConfig = storage.getModeConfig();
        JsonObject mode = config["mode"].to<JsonObject>();
        mode["default"] = modeConfig.defaultMode;
        mode["autoBlink"] = modeConfig.autoBlink;
        mode["blinkIntervalMin"] = modeConfig.blinkIntervalMin;
        mode["blinkIntervalMax"] = modeConfig.blinkIntervalMax;
        mode["rememberLastMode"] = modeConfig.rememberLastMode;

        // Impulse config
        ImpulseConfig impulseConfig = storage.getImpulseConfig();
        JsonObject impulse = config["impulse"].to<JsonObject>();
        impulse["autoImpulse"] = impulseConfig.autoImpulse;
        impulse["impulseIntervalMin"] = impulseConfig.impulseIntervalMin;
        impulse["impulseIntervalMax"] = impulseConfig.impulseIntervalMax;
        impulse["impulseSelection"] = impulseConfig.impulseSelection;

        // Modes section - read all mode files from /modes/
        JsonObject modes = doc["modes"].to<JsonObject>();
        File root = LittleFS.open("/modes");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    String name = file.name();
                    if (name.endsWith(".json")) {
                        JsonDocument modeDoc;
                        DeserializationError err = deserializeJson(modeDoc, file);
                        if (!err) {
                            // Remove .json extension for key
                            String key = name.substring(0, name.length() - 5);
                            modes[key] = modeDoc.as<JsonObject>();
                        }
                    }
                }
                file = root.openNextFile();
            }
            root.close();
        }

        // Impulses section - read all impulse files from /impulses/
        JsonObject impulses = doc["impulses"].to<JsonObject>();
        root = LittleFS.open("/impulses");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    String name = file.name();
                    if (name.endsWith(".json")) {
                        JsonDocument impulseDoc;
                        DeserializationError err = deserializeJson(impulseDoc, file);
                        if (!err) {
                            // Remove .json extension for key
                            String key = name.substring(0, name.length() - 5);
                            impulses[key] = impulseDoc.as<JsonObject>();
                        }
                    }
                }
                file = root.openNextFile();
            }
            root.close();
        }

        String output;
        serializeJsonPretty(doc, output);

        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", output);
        response->addHeader("Content-Disposition", "attachment; filename=\"animatronic-eyes-backup.json\"");
        request->send(response);

        WEB_LOG("WebServer", "Backup sent (%d bytes)", output.length());
    });

    // Restore endpoint - upload and apply backup
    server.on("/api/restore", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (restoreAuthFailed) {
                request->send(403, "application/json", "{\"success\":false,\"error\":\"Admin lock active\"}");
                return;
            }
            request->send(200, "application/json", "{\"success\":true}");
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            static String body;
            if (index == 0) {
                restoreAuthFailed = false;
                // Auth check - uses WebSocket session IP correlation
                if (!isHttpRequestAuthorized(request)) {
                    WEB_LOG("Admin", "Restore blocked: not authorized");
                    restoreAuthFailed = true;
                    return;
                }
                body = "";
                WEB_LOG("WebServer", "Restore started (%u bytes)", total);
                modeManager.setMode(Mode::NONE);  // Stop servos during restore
            }

            if (restoreAuthFailed) return;

            // Accumulate body
            for (size_t i = 0; i < len; i++) {
                body += (char)data[i];
            }

            if (index + len == total) {
                // Parse JSON
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, body);

                if (error) {
                    WEB_LOG("WebServer", "Restore failed: JSON parse error");
                    return;
                }

                // Validate backup type
                if (doc["type"] != "animatronic-eyes-backup") {
                    WEB_LOG("WebServer", "Restore failed: Invalid backup type");
                    return;
                }

                WEB_LOG("WebServer", "Restoring backup from version %s", doc["version"].as<const char*>());

                // Restore servo config
                JsonArray servos = doc["config"]["servo"].as<JsonArray>();
                for (size_t i = 0; i < servos.size() && i < NUM_SERVOS; i++) {
                    JsonObject s = servos[i];
                    storage.setServoPin(i, s["pin"] | DEFAULT_PIN_LEFT_EYE_X);
                    storage.setServoCalibration(i,
                        s["min"] | DEFAULT_SERVO_MIN,
                        s["center"] | DEFAULT_SERVO_CENTER,
                        s["max"] | DEFAULT_SERVO_MAX);
                    storage.setServoInvert(i, s["invert"] | false);
                }

                // Restore WiFi config
                JsonObject wifi = doc["config"]["wifi"].as<JsonObject>();
                for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
                    String key = "network" + String(i);
                    if (wifi.containsKey(key)) {
                        JsonObject net = wifi[key].as<JsonObject>();
                        storage.setWifiNetwork(i,
                            net["ssid"] | "",
                            net["password"] | "");
                    }
                }
                if (wifi.containsKey("timing")) {
                    JsonObject t = wifi["timing"].as<JsonObject>();
                    WifiTiming timing;
                    timing.graceMs = t["graceMs"] | DEFAULT_WIFI_GRACE_MS;
                    timing.retries = t["retries"] | DEFAULT_WIFI_RETRIES;
                    timing.retryDelayMs = t["retryDelayMs"] | DEFAULT_WIFI_RETRY_DELAY_MS;
                    timing.apScanMs = t["apScanMs"] | DEFAULT_WIFI_AP_SCAN_MS;
                    timing.keepAP = t["keepAP"] | DEFAULT_WIFI_KEEP_AP;
                    storage.setWifiTiming(timing);
                }

                // Restore AP config
                if (doc["config"].containsKey("ap")) {
                    JsonObject ap = doc["config"]["ap"].as<JsonObject>();
                    ApConfig apConfig;
                    strncpy(apConfig.ssidPrefix, ap["ssidPrefix"] | DEFAULT_AP_SSID_PREFIX, sizeof(apConfig.ssidPrefix) - 1);
                    strncpy(apConfig.password, ap["password"] | DEFAULT_AP_PASSWORD, sizeof(apConfig.password) - 1);
                    storage.setApConfig(apConfig);
                }

                // Restore LED config
                if (doc["config"].containsKey("led")) {
                    JsonObject led = doc["config"]["led"].as<JsonObject>();
                    LedConfig ledConfig;
                    ledConfig.enabled = led["enabled"] | DEFAULT_LED_ENABLED;
                    ledConfig.pin = led["pin"] | DEFAULT_LED_PIN;
                    ledConfig.brightness = led["brightness"] | DEFAULT_LED_BRIGHTNESS;
                    storage.setLedConfig(ledConfig);
                }

                // Restore mDNS config
                if (doc["config"].containsKey("mdns")) {
                    JsonObject mdns = doc["config"]["mdns"].as<JsonObject>();
                    MdnsConfig mdnsConfig;
                    mdnsConfig.enabled = mdns["enabled"] | DEFAULT_MDNS_ENABLED;
                    strncpy(mdnsConfig.hostname, mdns["hostname"] | DEFAULT_MDNS_HOSTNAME, sizeof(mdnsConfig.hostname) - 1);
                    storage.setMdnsConfig(mdnsConfig);
                }

                // Restore Mode config
                if (doc["config"].containsKey("mode")) {
                    JsonObject mode = doc["config"]["mode"].as<JsonObject>();
                    ModeConfig modeConfig;
                    strncpy(modeConfig.defaultMode, mode["default"] | DEFAULT_MODE, sizeof(modeConfig.defaultMode) - 1);
                    modeConfig.autoBlink = mode["autoBlink"] | DEFAULT_AUTO_BLINK;
                    modeConfig.blinkIntervalMin = mode["blinkIntervalMin"] | DEFAULT_BLINK_INTERVAL_MIN;
                    modeConfig.blinkIntervalMax = mode["blinkIntervalMax"] | DEFAULT_BLINK_INTERVAL_MAX;
                    modeConfig.rememberLastMode = mode["rememberLastMode"] | false;
                    storage.setModeConfig(modeConfig);
                }

                // Restore mode files
                if (doc.containsKey("modes")) {
                    // Ensure /modes directory exists
                    if (!LittleFS.exists("/modes")) {
                        LittleFS.mkdir("/modes");
                    }

                    JsonObject modes = doc["modes"].as<JsonObject>();
                    for (JsonPair kv : modes) {
                        char path[64];
                        snprintf(path, sizeof(path), "/modes/%s.json", kv.key().c_str());
                        File file = LittleFS.open(path, "w");
                        if (file) {
                            serializeJson(kv.value(), file);
                            file.close();
                            WEB_LOG("WebServer", "Restored mode: %s", kv.key().c_str());
                        }
                    }
                }

                // Restore Impulse config
                if (doc["config"].containsKey("impulse")) {
                    JsonObject impulse = doc["config"]["impulse"].as<JsonObject>();
                    ImpulseConfig impulseConfig;
                    impulseConfig.autoImpulse = impulse["autoImpulse"] | DEFAULT_AUTO_IMPULSE;
                    impulseConfig.impulseIntervalMin = impulse["impulseIntervalMin"] | DEFAULT_IMPULSE_INTERVAL_MIN;
                    impulseConfig.impulseIntervalMax = impulse["impulseIntervalMax"] | DEFAULT_IMPULSE_INTERVAL_MAX;
                    strncpy(impulseConfig.impulseSelection, impulse["impulseSelection"] | DEFAULT_IMPULSE_SELECTION, sizeof(impulseConfig.impulseSelection) - 1);
                    storage.setImpulseConfig(impulseConfig);
                }

                // Restore impulse files
                if (doc.containsKey("impulses")) {
                    // Ensure /impulses directory exists
                    if (!LittleFS.exists("/impulses")) {
                        LittleFS.mkdir("/impulses");
                    }

                    JsonObject impulses = doc["impulses"].as<JsonObject>();
                    for (JsonPair kv : impulses) {
                        char path[64];
                        snprintf(path, sizeof(path), "/impulses/%s.json", kv.key().c_str());
                        File file = LittleFS.open(path, "w");
                        if (file) {
                            serializeJson(kv.value(), file);
                            file.close();
                            WEB_LOG("WebServer", "Restored impulse: %s", kv.key().c_str());
                        }
                    }
                }

                WEB_LOG("WebServer", "Restore complete, signaling reboot...");
                storage.setRebootRequired(true);
                webServer.broadcastState();  // Notify UI before reboot
                body = "";  // Free memory
                delay(500);
                ESP.restart();
            }
        }
    );

    // Serve static files from LittleFS (CSS, JS, etc.)
    // MUST be last - catches all unmatched routes
    server.serveStatic("/", LittleFS, "/");
}

void WebServer::broadcastState() {
    // Use static buffer to avoid stack allocation in async context
    static char jsonBuffer[2048];  // Includes eye controller, mode system, and impulse state
    static JsonDocument doc;

    doc.clear();
    doc["type"] = "state";

    // Device ID (derived from chip ID, same as AP suffix)
    uint64_t chipId = ESP.getEfuseMac();
    char deviceId[8];
    snprintf(deviceId, sizeof(deviceId), "%06X", (uint32_t)(chipId & 0xFFFFFF));

    // WiFi runtime status (not config)
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    AppWifiMode mode = wifiManager.getMode();
    if (mode == APP_WIFI_AP_STA) {
        wifi["mode"] = "AP+STA";
    } else if (mode == APP_WIFI_STA) {
        wifi["mode"] = "STA";
    } else {
        wifi["mode"] = "AP";
    }
    wifi["ssid"] = wifiManager.getSSID();
    wifi["ip"] = wifiManager.getIP();
    wifi["apIp"] = wifiManager.getAPIP();
    wifi["apName"] = wifiManager.getAPName();
    wifi["apActive"] = wifiManager.isAPActive();
    wifi["connected"] = wifiManager.isConnected();
    wifi["reconnecting"] = wifiManager.isReconnecting();
    wifi["reconnectAttempt"] = wifiManager.getReconnectAttempt();
    wifi["mdnsActive"] = wifiManager.isMdnsActive();
    if (wifiManager.isMdnsActive()) {
        MdnsConfig mdnsConfig = storage.getMdnsConfig();
        wifi["mdnsHostname"] = String(mdnsConfig.hostname) + "-" + String(deviceId);
    }

    // System status (runtime, not config)
    JsonObject system = doc["system"].to<JsonObject>();
    system["rebootRequired"] = storage.isRebootRequired();
    system["uiVersion"] = _uiVersion;
    system["uiStatus"] = getUIStatus();
    system["deviceId"] = deviceId;

    // Servo states
    JsonArray servos = doc["servos"].to<JsonArray>();
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        JsonObject servo = servos.add<JsonObject>();
        const ServoConfig& config = servoController.getConfig(i);
        servo["name"] = SERVO_NAMES[i];
        servo["pos"] = servoController.getPosition(i);
        servo["min"] = config.min;
        servo["center"] = config.center;
        servo["max"] = config.max;
        servo["invert"] = config.invert;
        servo["pin"] = config.pin;
    }

    // Eye Controller state
    JsonObject eye = doc["eye"].to<JsonObject>();
    eye["gazeX"] = eyeController.getGazeX();
    eye["gazeY"] = eyeController.getGazeY();
    eye["gazeZ"] = eyeController.getGazeZ();
    eye["lidLeft"] = eyeController.getLidLeft();
    eye["lidRight"] = eyeController.getLidRight();
    eye["coupling"] = eyeController.getCoupling();
    eye["maxVergence"] = eyeController.getMaxVergence();

    // Mode System state
    JsonObject modeState = doc["mode"].to<JsonObject>();
    modeState["current"] = modeManager.getCurrentModeName();
    modeState["isAuto"] = (modeManager.getCurrentMode() == Mode::AUTO);
    modeState["autoBlink"] = autoBlink.isEnabled();         // Config setting
    modeState["autoBlinkActive"] = autoBlink.isActive();    // Effective state (considers pause/override)
    modeState["autoBlinkPaused"] = autoBlink.isPaused();
    modeState["blinkIntervalMin"] = autoBlink.getIntervalMin();
    modeState["blinkIntervalMax"] = autoBlink.getIntervalMax();
    // NOTE: Available modes sent once on connect via sendAvailableLists()

    // Impulse System state
    JsonObject impulseState = doc["impulse"].to<JsonObject>();
    impulseState["playing"] = impulsePlayer.isPlaying();
    impulseState["pending"] = impulsePlayer.isPending();
    impulseState["current"] = impulsePlayer.getCurrentImpulseName();
    impulseState["preloaded"] = impulsePlayer.getPreloadedName();
    impulseState["autoImpulse"] = autoImpulse.isEnabled();
    impulseState["autoImpulseActive"] = autoImpulse.isActive();
    impulseState["impulseIntervalMin"] = autoImpulse.getIntervalMin();
    impulseState["impulseIntervalMax"] = autoImpulse.getIntervalMax();
    impulseState["impulseSelection"] = autoImpulse.getSelection();
    // NOTE: Available impulses sent once on connect via sendAvailableLists()

    // Update Check state
    JsonObject updateState = doc["update"].to<JsonObject>();
    updateState["available"] = updateChecker.isUpdateAvailable();
    updateState["version"] = updateChecker.getAvailableVersion();
    updateState["lastCheck"] = updateChecker.getLastCheckTime();
    updateState["checking"] = updateChecker.isCheckInProgress();
    updateState["enabled"] = updateChecker.isEnabled();
    updateState["interval"] = updateChecker.getInterval();

    size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    if (len > 0 && len < sizeof(jsonBuffer)) {
        ws.textAll(jsonBuffer, len);
    }
}

void WebServer::sendConfigToClient(AsyncWebSocketClient* client) {
    // Send configuration data on-demand to requesting client only
    static char jsonBuffer[1024];
    JsonDocument doc;

    doc["type"] = "config";

    // WiFi networks
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (uint8_t i = 0; i < WIFI_MAX_NETWORKS; i++) {
        WifiNetwork net = storage.getWifiNetwork(i);
        JsonObject netObj = networks.add<JsonObject>();
        netObj["index"] = i;
        netObj["ssid"] = net.ssid;
        netObj["configured"] = net.configured;
        // Password not sent for security
    }

    // WiFi timing
    WifiTiming timing = storage.getWifiTiming();
    JsonObject wifiTiming = doc["wifiTiming"].to<JsonObject>();
    wifiTiming["grace"] = timing.graceMs / 1000;  // Convert to seconds for UI
    wifiTiming["retries"] = timing.retries;
    wifiTiming["retryDelay"] = timing.retryDelayMs / 1000;  // Convert to seconds
    wifiTiming["apScan"] = timing.apScanMs / 60000;  // Convert to minutes
    wifiTiming["keepAP"] = timing.keepAP;

    // AP config
    ApConfig apConfig = storage.getApConfig();
    JsonObject ap = doc["ap"].to<JsonObject>();
    ap["ssidPrefix"] = apConfig.ssidPrefix;
    ap["hasPassword"] = strlen(apConfig.password) > 0;

    // LED config
    LedConfig ledConfig = storage.getLedConfig();
    JsonObject led = doc["led"].to<JsonObject>();
    led["enabled"] = ledConfig.enabled;
    led["pin"] = ledConfig.pin;
    led["brightness"] = ledConfig.brightness;

    // mDNS config
    MdnsConfig mdnsConfig = storage.getMdnsConfig();
    JsonObject mdns = doc["mdns"].to<JsonObject>();
    mdns["enabled"] = mdnsConfig.enabled;
    mdns["hostname"] = mdnsConfig.hostname;

    // Mode config
    ModeConfig modeConfig = storage.getModeConfig();
    JsonObject mode = doc["mode"].to<JsonObject>();
    mode["defaultMode"] = modeConfig.defaultMode;
    mode["autoBlink"] = modeConfig.autoBlink;
    mode["blinkIntervalMin"] = modeConfig.blinkIntervalMin;
    mode["blinkIntervalMax"] = modeConfig.blinkIntervalMax;
    mode["rememberLastMode"] = modeConfig.rememberLastMode;

    // Impulse config
    ImpulseConfig impulseConfig = storage.getImpulseConfig();
    JsonObject impulse = doc["impulse"].to<JsonObject>();
    impulse["autoImpulse"] = impulseConfig.autoImpulse;
    impulse["impulseIntervalMin"] = impulseConfig.impulseIntervalMin;
    impulse["impulseIntervalMax"] = impulseConfig.impulseIntervalMax;
    impulse["impulseSelection"] = impulseConfig.impulseSelection;

    size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    if (len > 0 && len < sizeof(jsonBuffer)) {
        client->text(jsonBuffer, len);
    }
}

void WebServer::sendAvailableLists(AsyncWebSocketClient* client) {
    // Send available modes and impulses to a specific client (on connect)
    // This replaces sending them in every state broadcast
    char jsonBuffer[512];
    JsonDocument doc;

    // Send available modes
    doc.clear();
    doc["type"] = "availableModes";
    JsonArray modes = doc["modes"].to<JsonArray>();
    modes.add("follow");  // Always available
    int modeCount = modeManager.getAvailableModeCount();
    for (int i = 0; i < modeCount; i++) {
        char name[32];
        if (modeManager.getAvailableModeName(i, name, sizeof(name))) {
            modes.add(name);
        }
    }
    size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    if (len > 0 && len < sizeof(jsonBuffer)) {
        client->text(jsonBuffer, len);
    }

    // Send available impulses
    doc.clear();
    doc["type"] = "availableImpulses";
    JsonArray impulses = doc["impulses"].to<JsonArray>();
    int impulseCount = impulsePlayer.getAvailableImpulseCount();
    for (int i = 0; i < impulseCount; i++) {
        char name[32];
        if (impulsePlayer.getAvailableImpulseName(i, name, sizeof(name))) {
            impulses.add(name);
        }
    }
    len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    if (len > 0 && len < sizeof(jsonBuffer)) {
        client->text(jsonBuffer, len);
    }
}

void WebServer::log(const char* tag, const char* format, ...) {
    char message[LOG_LINE_MAX_LEN];
    char fullLine[LOG_LINE_MAX_LEN];

    // Format the message
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Create full log line with timestamp and tag
    unsigned long ms = millis();
    snprintf(fullLine, sizeof(fullLine), "[%lu.%03lu] [%s] %s",
             ms / 1000, ms % 1000, tag, message);

    // Add to ring buffer
    _logBuffer[_logHead] = String(fullLine);
    _logHead = (_logHead + 1) % LOG_BUFFER_SIZE;
    if (_logCount < LOG_BUFFER_SIZE) {
        _logCount++;
    }

    // Also print to Serial
    Serial.println(fullLine);

    // Broadcast to WebSocket clients
    broadcastLog(fullLine);
}

void WebServer::broadcastLog(const String& logLine) {
    if (ws.count() == 0) return;

    JsonDocument doc;
    doc["type"] = "log";
    doc["line"] = logLine;

    char buffer[LOG_LINE_MAX_LEN + 32];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len > 0 && len < sizeof(buffer)) {
        ws.textAll(buffer, len);
    }
}

void WebServer::sendLogHistory(AsyncWebSocketClient* client) {
    // Send buffered logs to a newly connected client
    JsonDocument doc;
    doc["type"] = "logHistory";
    JsonArray lines = doc["lines"].to<JsonArray>();

    // Calculate start index for ring buffer
    int start = (_logCount < LOG_BUFFER_SIZE) ? 0 : _logHead;
    for (int i = 0; i < _logCount; i++) {
        int idx = (start + i) % LOG_BUFFER_SIZE;
        lines.add(_logBuffer[idx]);
    }

    char buffer[4096];  // Larger buffer for history
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len > 0 && len < sizeof(buffer)) {
        client->text(buffer, len);
    }
}

// ============================================================================
// Admin Auth Functions
// ============================================================================

bool WebServer::isAPClient(AsyncWebSocketClient* client) {
    // AP clients have IP addresses in the 192.168.4.x subnet
    IPAddress ip = client->remoteIP();
    return isAPClientIP(ip);
}

bool WebServer::isAPClientIP(IPAddress ip) {
    // AP clients have IP addresses in the 192.168.4.x subnet
    return (ip[0] == 192 && ip[1] == 168 && ip[2] == 4);
}

bool WebServer::isHttpRequestAuthorized(AsyncWebServerRequest* request) {
    // No PIN configured = everyone is authorized
    if (!storage.hasAdminPin()) {
        return true;
    }

    IPAddress requestIP = request->client()->remoteIP();

    // AP clients are always authorized
    if (isAPClientIP(requestIP)) {
        return true;
    }

    // Check if this IP has an authenticated WebSocket session
    for (int i = 0; i < _authClientCount; i++) {
        if (_authClients[i].ip == requestIP && _authClients[i].authenticated) {
            // Check timeout
            if (_authClients[i].unlockTime > 0 && millis() > _authClients[i].unlockTime) {
                _authClients[i].authenticated = false;
                continue;
            }
            return true;
        }
    }

    return false;
}

bool WebServer::isClientAuthenticated(AsyncWebSocketClient* client) {
    // No PIN configured = everyone is authenticated
    if (!storage.hasAdminPin()) {
        return true;
    }

    // AP clients are always authenticated
    if (isAPClient(client)) {
        return true;
    }

    IPAddress clientIP = client->remoteIP();

    // Check auth state by IP
    for (int i = 0; i < _authClientCount; i++) {
        if (_authClients[i].ip == clientIP) {
            if (!_authClients[i].authenticated) {
                return false;
            }
            // Check timeout (0 = no timeout for AP clients)
            if (_authClients[i].unlockTime > 0 && millis() > _authClients[i].unlockTime) {
                _authClients[i].authenticated = false;
                return false;
            }
            return true;
        }
    }

    return false;  // Not in list = not authenticated
}

bool WebServer::isClientLocked(AsyncWebSocketClient* client) {
    return !isClientAuthenticated(client);
}

int WebServer::getRemainingSeconds(AsyncWebSocketClient* client) {
    // No PIN = no countdown needed
    if (!storage.hasAdminPin()) {
        return 0;
    }

    // AP clients = no timeout
    if (isAPClient(client)) {
        return 0;
    }

    IPAddress clientIP = client->remoteIP();

    // Check by IP
    for (int i = 0; i < _authClientCount; i++) {
        if (_authClients[i].ip == clientIP && _authClients[i].authenticated) {
            if (_authClients[i].unlockTime == 0) {
                return 0;  // No timeout
            }
            int remaining = (_authClients[i].unlockTime - millis()) / 1000;
            return remaining > 0 ? remaining : 0;
        }
    }

    return 0;
}

void WebServer::authenticateClient(AsyncWebSocketClient* client) {
    IPAddress clientIP = client->remoteIP();
    bool isAP = isAPClient(client);

    // Check if IP already in list
    for (int i = 0; i < _authClientCount; i++) {
        if (_authClients[i].ip == clientIP) {
            _authClients[i].authenticated = true;
            _authClients[i].isAPClient = isAP;
            _authClients[i].unlockTime = isAP ? 0 : (millis() + ADMIN_TIMEOUT_MS);
            return;
        }
    }

    // Add new entry (evict oldest if full)
    if (_authClientCount >= MAX_AUTH_CLIENTS) {
        for (int i = 0; i < MAX_AUTH_CLIENTS - 1; i++) {
            _authClients[i] = _authClients[i + 1];
        }
        _authClientCount = MAX_AUTH_CLIENTS - 1;
    }

    _authClients[_authClientCount].ip = clientIP;
    _authClients[_authClientCount].authenticated = true;
    _authClients[_authClientCount].isAPClient = isAP;
    _authClients[_authClientCount].unlockTime = isAP ? 0 : (millis() + ADMIN_TIMEOUT_MS);
    _authClientCount++;
}

void WebServer::authenticateIP(IPAddress ip) {
    bool isAP = isAPClientIP(ip);

    // Check if IP already in list
    for (int i = 0; i < _authClientCount; i++) {
        if (_authClients[i].ip == ip) {
            _authClients[i].authenticated = true;
            _authClients[i].isAPClient = isAP;
            _authClients[i].unlockTime = isAP ? 0 : (millis() + ADMIN_TIMEOUT_MS);
            return;
        }
    }

    // Add new entry (evict oldest if full)
    if (_authClientCount >= MAX_AUTH_CLIENTS) {
        for (int i = 0; i < MAX_AUTH_CLIENTS - 1; i++) {
            _authClients[i] = _authClients[i + 1];
        }
        _authClientCount = MAX_AUTH_CLIENTS - 1;
    }

    _authClients[_authClientCount].ip = ip;
    _authClients[_authClientCount].authenticated = true;
    _authClients[_authClientCount].isAPClient = isAP;
    _authClients[_authClientCount].unlockTime = isAP ? 0 : (millis() + ADMIN_TIMEOUT_MS);
    _authClientCount++;
}

void WebServer::lockClient(AsyncWebSocketClient* client) {
    // Lock by IP so all tabs/connections from same IP get locked
    IPAddress clientIP = client->remoteIP();
    for (int i = 0; i < _authClientCount; i++) {
        if (_authClients[i].ip == clientIP) {
            _authClients[i].authenticated = false;
            _authClients[i].unlockTime = 0;
            return;
        }
    }
}

bool WebServer::checkRateLimit(IPAddress ip) {
    // Returns true if allowed, false if rate limited
    for (int i = 0; i < _rateLimitCount; i++) {
        if (_rateLimits[i].ip == ip) {
            if (_rateLimits[i].lockoutUntil > 0 && millis() < _rateLimits[i].lockoutUntil) {
                return false;  // Still locked out
            }
            // Lockout expired, reset
            if (_rateLimits[i].lockoutUntil > 0 && millis() >= _rateLimits[i].lockoutUntil) {
                _rateLimits[i].failedAttempts = 0;
                _rateLimits[i].lockoutUntil = 0;
            }
            return true;
        }
    }
    return true;  // Not in list = allowed
}

void WebServer::recordFailedAttempt(IPAddress ip) {
    // Find existing entry
    for (int i = 0; i < _rateLimitCount; i++) {
        if (_rateLimits[i].ip == ip) {
            _rateLimits[i].failedAttempts++;
            if (_rateLimits[i].failedAttempts >= ADMIN_MAX_FAILED_ATTEMPTS) {
                _rateLimits[i].lockoutUntil = millis() + ADMIN_LOCKOUT_MS;
                WEB_LOG("Admin", "Rate limit: %s locked out for %d seconds",
                        ip.toString().c_str(), ADMIN_LOCKOUT_MS / 1000);
            }
            return;
        }
    }

    // Add new entry (evict oldest if full)
    if (_rateLimitCount >= MAX_RATE_LIMIT_ENTRIES) {
        for (int i = 0; i < MAX_RATE_LIMIT_ENTRIES - 1; i++) {
            _rateLimits[i] = _rateLimits[i + 1];
        }
        _rateLimitCount = MAX_RATE_LIMIT_ENTRIES - 1;
    }

    _rateLimits[_rateLimitCount].ip = ip;
    _rateLimits[_rateLimitCount].failedAttempts = 1;
    _rateLimits[_rateLimitCount].lockoutUntil = 0;
    _rateLimitCount++;
}

int WebServer::getRateLimitSeconds(IPAddress ip) {
    // Returns seconds remaining in lockout, or 0 if not locked out
    for (int i = 0; i < _rateLimitCount; i++) {
        if (_rateLimits[i].ip == ip) {
            if (_rateLimits[i].lockoutUntil > 0 && millis() < _rateLimits[i].lockoutUntil) {
                return (_rateLimits[i].lockoutUntil - millis()) / 1000;
            }
        }
    }
    return 0;
}

void WebServer::clearFailedAttempts(IPAddress ip) {
    for (int i = 0; i < _rateLimitCount; i++) {
        if (_rateLimits[i].ip == ip) {
            _rateLimits[i].failedAttempts = 0;
            _rateLimits[i].lockoutUntil = 0;
            return;
        }
    }
}

void WebServer::sendAdminState(AsyncWebSocketClient* client) {
    // Send per-client admin lock state
    char buffer[256];
    JsonDocument doc;

    doc["type"] = "adminState";
    doc["locked"] = isClientLocked(client);
    doc["isAPClient"] = isAPClient(client);
    doc["pinConfigured"] = storage.hasAdminPin();
    doc["remainingSeconds"] = getRemainingSeconds(client);
    doc["lockoutSeconds"] = getRateLimitSeconds(client->remoteIP());  // Rate limit lockout

    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len > 0 && len < sizeof(buffer)) {
        client->text(buffer, len);
    }
}

void WebServer::broadcastAdminStateToIP(IPAddress ip) {
    // Send admin state to ALL connected clients from the same IP
    for (auto& c : ws.getClients()) {
        if (c.status() == WS_CONNECTED && c.remoteIP() == ip) {
            sendAdminState(&c);
        }
    }
}

void WebServer::sendAdminBlocked(AsyncWebSocketClient* client, const char* command) {
    // Notify client that a command was blocked due to admin lock
    char buffer[128];
    JsonDocument doc;

    doc["type"] = "adminBlocked";
    doc["command"] = command;

    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len > 0 && len < sizeof(buffer)) {
        client->text(buffer, len);
    }
}

// ============================================================================
// WebSocket Message Handler
// ============================================================================

void WebServer::handleWebSocketMessage(const char* data, AsyncWebSocketClient* client) {
    // Static to avoid stack allocation on every message (reduces stack pressure in async context)
    static JsonDocument doc;
    doc.clear();
    DeserializationError error = deserializeJson(doc, data);

    if (error) {
        WEB_LOG("WS", "JSON parse error: %s", error.c_str());
        return;
    }

    const char* type = doc["type"];
    if (!type) return;

    if (strcmp(type, "setServo") == 0) {
        uint8_t index = doc["index"];
        uint8_t position = doc["position"];
        servoController.setPosition(index, position);
    }
    else if (strcmp(type, "setCalibration") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setCalibration blocked: client locked");
            sendAdminBlocked(client, "setCalibration");
            return;
        }
        uint8_t index = doc["index"];
        uint8_t min = doc["min"];
        uint8_t center = doc["center"];
        uint8_t max = doc["max"];

        // Clamp to 0-180
        min = constrain(min, 0, 180);
        center = constrain(center, 0, 180);
        max = constrain(max, 0, 180);

        // Enforce min < center < max
        if (min >= center) min = (center > 0) ? center - 1 : 0;
        if (max <= center) max = (center < 180) ? center + 1 : 180;
        if (min >= center) center = min + 1;
        if (max <= center) center = max - 1;

        servoController.setCalibration(index, min, center, max);
    }
    else if (strcmp(type, "setPin") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setPin blocked: client locked");
            sendAdminBlocked(client, "setPin");
            return;
        }
        uint8_t index = doc["index"];
        uint8_t pin = doc["pin"];
        servoController.setPin(index, pin);
    }
    else if (strcmp(type, "setInvert") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setInvert blocked: client locked");
            sendAdminBlocked(client, "setInvert");
            return;
        }
        uint8_t index = doc["index"];
        bool invert = doc["invert"];
        servoController.setInvert(index, invert);
    }
    else if (strcmp(type, "centerAll") == 0) {
        servoController.requestCenterAll();
    }
    // Eye Controller commands
    else if (strcmp(type, "setGaze") == 0) {
        float x = doc["x"] | 0.0f;
        float y = doc["y"] | 0.0f;
        float z = doc["z"] | 100.0f;
        eyeController.setGaze(x, y, z);
    }
    else if (strcmp(type, "setLids") == 0) {
        float left = doc["left"] | 100.0f;
        float right = doc["right"] | 100.0f;
        eyeController.setLids(left, right);
        autoBlink.resetTimer();  // Prevent auto-blink from fighting with manual lid control
    }
    else if (strcmp(type, "blink") == 0) {
        unsigned int duration = doc["duration"] | 0;  // 0 = scaled based on lid position
        eyeController.startBlink(duration);
        autoBlink.resetTimer();
        WEB_LOG("Control", "Blink");
    }
    else if (strcmp(type, "blinkLeft") == 0) {
        unsigned int duration = doc["duration"] | 0;  // 0 = scaled based on lid position
        eyeController.startBlinkLeft(duration);
        autoBlink.resetTimer();
        WEB_LOG("Control", "Wink left");
    }
    else if (strcmp(type, "blinkRight") == 0) {
        unsigned int duration = doc["duration"] | 0;  // 0 = scaled based on lid position
        eyeController.startBlinkRight(duration);
        autoBlink.resetTimer();
        WEB_LOG("Control", "Wink right");
    }
    else if (strcmp(type, "setCoupling") == 0) {
        float value = doc["value"] | 1.0f;
        eyeController.setCoupling(value);
    }
    else if (strcmp(type, "setVergence") == 0) {
        float max = doc["max"] | 30.0f;
        eyeController.setMaxVergence(max);
    }
    else if (strcmp(type, "centerEyes") == 0) {
        eyeController.center();
    }
    else if (strcmp(type, "reapplyEyeState") == 0) {
        // Re-apply current Eye Controller state to servos
        // Used when returning to Control tab from Calibration
        eyeController.reapply();
    }
    // Mode System commands
    else if (strcmp(type, "setMode") == 0) {
        const char* mode = doc["mode"];
        if (mode) {
            if (strcmp(mode, "follow") == 0) {
                modeManager.setMode(Mode::FOLLOW);
                WEB_LOG("Control", "Mode: Follow");
            } else {
                if (modeManager.setAutoMode(mode)) {
                    WEB_LOG("Control", "Mode: Auto (%s)", mode);
                } else {
                    WEB_LOG("Control", "Failed to load mode: %s", mode);
                }
            }
        }
    }
    else if (strcmp(type, "setAutoBlink") == 0) {
        bool enabled = doc["enabled"] | true;
        autoBlink.setEnabled(enabled);
        // Save to config
        ModeConfig config = storage.getModeConfig();
        config.autoBlink = enabled;
        storage.setModeConfig(config);
        WEB_LOG("Mode", "Auto-blink %s", enabled ? "enabled" : "disabled");
    }
    else if (strcmp(type, "setRememberLastMode") == 0) {
        bool enabled = doc["enabled"] | false;
        ModeConfig config = storage.getModeConfig();
        config.rememberLastMode = enabled;
        // When enabling remember, also save current mode as default
        if (enabled) {
            const char* currentMode = modeManager.getCurrentModeName();
            strncpy(config.defaultMode, currentMode, sizeof(config.defaultMode) - 1);
            config.defaultMode[sizeof(config.defaultMode) - 1] = '\0';
            WEB_LOG("Mode", "Remember last mode enabled, saving current mode: %s", currentMode);
        }
        storage.setModeConfig(config);
        WEB_LOG("Mode", "Remember last mode %s", enabled ? "enabled" : "disabled");
    }
    else if (strcmp(type, "pauseAutoBlink") == 0) {
        // Temporary pause for calibration - pauses ALL automated control
        bool paused = doc["paused"] | false;
        if (paused) {
            autoBlink.pause();
            autoImpulse.pause();
            modePlayer.pause();
            WEB_LOG("Calibration", "Entering calibration mode");
        } else {
            autoBlink.resume();
            autoImpulse.resume();
            modePlayer.resume();
            WEB_LOG("Calibration", "Exiting calibration mode");
        }
    }
    else if (strcmp(type, "pauseModePlayer") == 0) {
        // Pause mode player during manual control interaction (auto modes only)
        bool paused = doc["paused"] | false;
        if (paused) {
            modePlayer.pause();
        } else {
            modePlayer.resume();
        }
    }
    else if (strcmp(type, "setAutoBlinkOverride") == 0) {
        // Runtime override (for Follow mode toggle) - doesn't affect config
        if (doc.containsKey("enabled")) {
            bool enabled = doc["enabled"];
            autoBlink.setRuntimeOverride(enabled);
            WEB_LOG("Control", "Auto-blink: %s", enabled ? "on" : "off");
        } else {
            autoBlink.clearRuntimeOverride();
            WEB_LOG("Control", "Auto-blink: default");
        }
    }
    else if (strcmp(type, "setAutoImpulseOverride") == 0) {
        // Runtime override (for Follow mode toggle) - doesn't affect config
        if (doc.containsKey("enabled")) {
            bool enabled = doc["enabled"];
            autoImpulse.setRuntimeOverride(enabled);
            WEB_LOG("Control", "Auto-impulse: %s", enabled ? "on" : "off");
        } else {
            autoImpulse.clearRuntimeOverride();
            WEB_LOG("Control", "Auto-impulse: default");
        }
    }
    else if (strcmp(type, "setBlinkInterval") == 0) {
        uint16_t minMs = doc["min"] | DEFAULT_BLINK_INTERVAL_MIN;
        uint16_t maxMs = doc["max"] | DEFAULT_BLINK_INTERVAL_MAX;
        autoBlink.setInterval(minMs, maxMs);
        // Save to config
        ModeConfig config = storage.getModeConfig();
        config.blinkIntervalMin = minMs;
        config.blinkIntervalMax = maxMs;
        storage.setModeConfig(config);
        WEB_LOG("Mode", "Blink interval set to %d-%d ms", minMs, maxMs);
    }
    else if (strcmp(type, "setDefaultMode") == 0) {
        const char* mode = doc["mode"];
        if (mode) {
            ModeConfig config = storage.getModeConfig();
            strncpy(config.defaultMode, mode, sizeof(config.defaultMode) - 1);
            config.defaultMode[sizeof(config.defaultMode) - 1] = '\0';
            storage.setModeConfig(config);
            WEB_LOG("Mode", "Default startup mode set to: %s", mode);
        }
    }
    else if (strcmp(type, "getAvailableModes") == 0) {
        // Send list of available modes to client
        JsonDocument response;
        response["type"] = "availableModes";
        JsonArray modes = response["modes"].to<JsonArray>();
        modes.add("follow");  // Always available
        int count = modeManager.getAvailableModeCount();
        for (int i = 0; i < count; i++) {
            char name[32];
            if (modeManager.getAvailableModeName(i, name, sizeof(name))) {
                modes.add(name);
            }
        }
        String output;
        serializeJson(response, output);
        client->text(output);
        return;  // Don't request broadcast, we sent our own response
    }
    // Impulse System commands
    else if (strcmp(type, "triggerImpulse") == 0) {
        // Trigger impulse (uses preloaded or loads specified name)
        const char* name = doc["name"];
        if (name && strlen(name) > 0) {
            WEB_LOG("Control", "Impulse triggered: %s", name);
            if (!impulsePlayer.triggerByName(name)) {
                WEB_LOG("Control", "Impulse trigger failed: %s", name);
            }
        } else {
            // Trigger preloaded impulse
            WEB_LOG("Control", "Impulse triggered");
            if (!impulsePlayer.trigger()) {
                WEB_LOG("Control", "Impulse trigger failed");
            }
        }
        autoImpulse.resetTimer();  // Reset auto-impulse timer
    }
    else if (strcmp(type, "setAutoImpulse") == 0) {
        bool enabled = doc["enabled"] | true;
        autoImpulse.setEnabled(enabled);
        // Save to config
        ImpulseConfig config = storage.getImpulseConfig();
        config.autoImpulse = enabled;
        storage.setImpulseConfig(config);
        WEB_LOG("Impulse", "Auto-impulse %s", enabled ? "enabled" : "disabled");
    }
    else if (strcmp(type, "setImpulseInterval") == 0) {
        uint32_t minMs = doc["min"] | DEFAULT_IMPULSE_INTERVAL_MIN;
        uint32_t maxMs = doc["max"] | DEFAULT_IMPULSE_INTERVAL_MAX;
        autoImpulse.setInterval(minMs, maxMs);
        // Save to config
        ImpulseConfig config = storage.getImpulseConfig();
        config.impulseIntervalMin = minMs;
        config.impulseIntervalMax = maxMs;
        storage.setImpulseConfig(config);
        WEB_LOG("Impulse", "Impulse interval set to %lu-%lu ms", minMs, maxMs);
    }
    else if (strcmp(type, "setImpulseSelection") == 0) {
        const char* selection = doc["selection"];
        if (selection) {
            autoImpulse.setSelection(selection);
            // Save to config
            ImpulseConfig config = storage.getImpulseConfig();
            strncpy(config.impulseSelection, selection, sizeof(config.impulseSelection) - 1);
            config.impulseSelection[sizeof(config.impulseSelection) - 1] = '\0';
            storage.setImpulseConfig(config);
            WEB_LOG("Impulse", "Impulse selection updated: %s", selection);
        }
    }
    else if (strcmp(type, "getAvailableImpulses") == 0) {
        // Send list of available impulses to client
        JsonDocument response;
        response["type"] = "availableImpulses";
        JsonArray impulses = response["impulses"].to<JsonArray>();
        int count = impulsePlayer.getAvailableImpulseCount();
        for (int i = 0; i < count; i++) {
            char name[32];
            if (impulsePlayer.getAvailableImpulseName(i, name, sizeof(name))) {
                impulses.add(name);
            }
        }
        String output;
        serializeJson(response, output);
        client->text(output);
        return;  // Don't request broadcast, we sent our own response
    }
    // Legacy setWifi (uses network 0)
    else if (strcmp(type, "setWifi") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setWifi blocked: client locked");
            sendAdminBlocked(client, "setWifi");
            return;
        }
        const char* ssid = doc["ssid"];
        const char* password = doc["password"];
        if (ssid && password) {
            if (storage.setWifiNetwork(0, ssid, password)) {
                wifiManager.connectToNetwork(0);
            } else {
                WEB_LOG("WS", "Failed to save WiFi credentials");
            }
        }
    }
    // New multi-network commands
    else if (strcmp(type, "setWifiNetwork") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setWifiNetwork blocked: client locked");
            sendAdminBlocked(client, "setWifiNetwork");
            return;
        }
        uint8_t index = doc["index"];
        const char* ssid = doc["ssid"];
        const char* password = doc["password"];
        if (ssid && password && index < WIFI_MAX_NETWORKS) {
            if (storage.setWifiNetwork(index, ssid, password)) {
                WEB_LOG("Config", "WiFi network %d saved: %s", index, ssid);
                // If setting primary and not connected, try to connect
                if (index == 0 && !wifiManager.isConnected()) {
                    wifiManager.connectToNetwork(0);
                }
            } else {
                WEB_LOG("Config", "Failed to save WiFi network %d", index);
            }
        }
    }
    else if (strcmp(type, "clearWifiNetwork") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "clearWifiNetwork blocked: client locked");
            sendAdminBlocked(client, "clearWifiNetwork");
            return;
        }
        uint8_t index = doc["index"];
        if (index < WIFI_MAX_NETWORKS) {
            storage.clearWifiNetwork(index);
            WEB_LOG("Config", "WiFi network %d cleared", index);
        }
    }
    else if (strcmp(type, "setWifiTiming") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setWifiTiming blocked: client locked");
            sendAdminBlocked(client, "setWifiTiming");
            return;
        }
        WifiTiming timing;
        timing.graceMs = doc["grace"].as<uint16_t>() * 1000;  // UI sends seconds
        timing.retries = doc["retries"];
        timing.retryDelayMs = doc["retryDelay"].as<uint16_t>() * 1000;
        timing.apScanMs = doc["apScan"].as<uint32_t>() * 60000;  // UI sends minutes
        timing.keepAP = doc["keepAP"] | true;

        // Validate ranges
        timing.graceMs = constrain(timing.graceMs, 1000, 10000);
        timing.retries = constrain(timing.retries, 1, 10);
        timing.retryDelayMs = constrain(timing.retryDelayMs, 5000, 60000);
        timing.apScanMs = constrain(timing.apScanMs, 60000, 1800000);

        storage.setWifiTiming(timing);
    }
    else if (strcmp(type, "setKeepAP") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setKeepAP blocked: client locked");
            sendAdminBlocked(client, "setKeepAP");
            return;
        }
        bool enabled = doc["enabled"];
        WifiTiming timing = storage.getWifiTiming();
        timing.keepAP = enabled;
        storage.setWifiTiming(timing);
    }
    else if (strcmp(type, "setLed") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setLed blocked: client locked");
            sendAdminBlocked(client, "setLed");
            return;
        }
        LedConfig config;
        config.enabled = doc["enabled"] | true;
        config.pin = doc["pin"] | DEFAULT_LED_PIN;
        config.brightness = doc["brightness"] | DEFAULT_LED_BRIGHTNESS;
        storage.setLedConfig(config);
        ledStatus.setEnabled(config.enabled);
        ledStatus.setPin(config.pin);
        ledStatus.setBrightness(config.brightness);
    }
    else if (strcmp(type, "setMdns") == 0) {
        if (isClientLocked(client)) {
            WEB_LOG("Admin", "setMdns blocked: client locked");
            sendAdminBlocked(client, "setMdns");
            return;
        }
        MdnsConfig config;
        config.enabled = doc["enabled"] | true;
        const char* hostname = doc["hostname"];
        if (hostname) {
            strncpy(config.hostname, hostname, sizeof(config.hostname) - 1);
            config.hostname[sizeof(config.hostname) - 1] = '\0';
        } else {
            strncpy(config.hostname, DEFAULT_MDNS_HOSTNAME, sizeof(config.hostname) - 1);
        }
        storage.setMdnsConfig(config);
        WEB_LOG("Config", "mDNS config saved: %s (reboot required)", config.hostname);
    }
    else if (strcmp(type, "leaveNetwork") == 0) {
        // Legacy: clears all credentials and starts AP
        wifiManager.disconnect();
    }
    else if (strcmp(type, "resetConnection") == 0) {
        // New: drop connection, trigger reconnect cycle (keeps credentials)
        WEB_LOG("WiFi", "Connection reset requested");
        wifiManager.resetConnection();
    }
    else if (strcmp(type, "scanNetworks") == 0) {
        WEB_LOG("WiFi", "Network scan started");
        String networks = wifiManager.scanNetworks();
        // Send scan results back to requesting client
        String response = "{\"type\":\"networkList\",\"networks\":" + networks + "}";
        client->text(response);
        return;  // Don't request broadcast, we sent our own response
    }
    else if (strcmp(type, "getConfig") == 0) {
        sendConfigToClient(client);
        return;  // Don't request broadcast, we sent config directly
    }
    else if (strcmp(type, "getLogHistory") == 0) {
        sendLogHistory(client);
        return;  // Don't request broadcast, we sent logs directly
    }
    else if (strcmp(type, "previewCalibration") == 0) {
        // Move servo to position for live preview (doesn't save)
        // Uses setPositionRaw to bypass calibration limits during calibration
        uint8_t index = doc["index"];
        uint8_t position = doc["position"];
        if (index < NUM_SERVOS) {
            servoController.setPositionRaw(index, position);
        }
    }
    else if (strcmp(type, "saveAllCalibration") == 0) {
        // Protected: requires admin unlock
        if (!isClientAuthenticated(client)) {
            WEB_LOG("Admin", "saveAllCalibration blocked: not authenticated");
            sendAdminBlocked(client, "saveAllCalibration");
            return;
        }
        // Save all servo calibration at once
        JsonArray servos = doc["servos"];
        for (JsonObject servo : servos) {
            uint8_t index = servo["index"];
            if (index >= NUM_SERVOS) continue;

            uint8_t pin = servo["pin"];
            uint8_t min = servo["min"];
            uint8_t center = servo["center"];
            uint8_t max = servo["max"];
            bool invert = servo["invert"];

            // Apply clamping rules (no push/pull)
            min = constrain(min, 0, 180);
            center = constrain(center, 0, 180);
            max = constrain(max, 0, 180);

            // Clamp values to valid ranges
            if (min > center) min = center;
            if (max < center) max = center;

            // Get current config to check what actually changed
            const ServoConfig& current = servoController.getConfig(index);

            // Only update pin if changed (avoids unnecessary detach/reattach)
            if (current.pin != pin) {
                servoController.setPin(index, pin);
            }

            // Always update calibration (cheap operation, no servo write)
            servoController.setCalibration(index, min, center, max);

            // Only update invert if changed (avoids unnecessary servo write)
            if (current.invert != invert) {
                servoController.setInvert(index, invert);
            }
        }
        WEB_LOG("Calibration", "Calibration saved for %d servos", servos.size());
    }
    else if (strcmp(type, "resetCalibration") == 0) {
        // Protected: requires admin unlock
        if (!isClientAuthenticated(client)) {
            WEB_LOG("Admin", "resetCalibration blocked: not authenticated");
            sendAdminBlocked(client, "resetCalibration");
            return;
        }
        // Reset all servos to factory default calibration
        for (int i = 0; i < NUM_SERVOS; i++) {
            servoController.setCalibration(i, DEFAULT_SERVO_MIN, DEFAULT_SERVO_CENTER, DEFAULT_SERVO_MAX);
            servoController.setInvert(i, false);
        }
        WEB_LOG("Calibration", "Calibration reset to factory defaults");
        requestBroadcast();  // Send updated state to clients
    }
    else if (strcmp(type, "reboot") == 0) {
        // Blocked only when rate limited (allows reboot when locked)
        if (!checkRateLimit(client->remoteIP())) {
            WEB_LOG("WebServer", "Reboot blocked: rate limited");
            return;
        }
        WEB_LOG("WebServer", "Reboot requested via WebSocket");
        delay(500);
        ESP.restart();
    }
    else if (strcmp(type, "setApConfig") == 0) {
        // Protected: requires admin unlock
        if (!isClientAuthenticated(client)) {
            WEB_LOG("Admin", "setApConfig blocked: not authenticated");
            sendAdminBlocked(client, "setApConfig");
            return;
        }
        ApConfig config;
        const char* prefix = doc["ssidPrefix"];
        const char* password = doc["password"];

        if (prefix) {
            strncpy(config.ssidPrefix, prefix, sizeof(config.ssidPrefix) - 1);
            config.ssidPrefix[sizeof(config.ssidPrefix) - 1] = '\0';
        } else {
            strncpy(config.ssidPrefix, DEFAULT_AP_SSID_PREFIX, sizeof(config.ssidPrefix) - 1);
        }

        if (password) {
            strncpy(config.password, password, sizeof(config.password) - 1);
            config.password[sizeof(config.password) - 1] = '\0';
        } else {
            strncpy(config.password, DEFAULT_AP_PASSWORD, sizeof(config.password) - 1);
        }

        storage.setApConfig(config);  // This sets rebootRequired flag
        WEB_LOG("Config", "AP config saved: %s (reboot required)", config.ssidPrefix);
    }
    else if (strcmp(type, "clearRebootFlag") == 0) {
        storage.clearRebootRequired();
    }
    else if (strcmp(type, "factoryReset") == 0) {
        // Protected: requires admin unlock
        if (!isClientAuthenticated(client)) {
            WEB_LOG("Admin", "factoryReset blocked: not authenticated");
            sendAdminBlocked(client, "factoryReset");
            return;
        }
        WEB_LOG("WebServer", "Factory reset requested via WebSocket");
        ledStatus.strobe();  // Visual indicator
        storage.factoryReset();
        delay(500);
        ESP.restart();
    }
    // ========================================================================
    // Admin Auth Commands
    // ========================================================================
    else if (strcmp(type, "adminAuth") == 0) {
        const char* pin = doc["pin"];
        if (!pin) {
            WEB_LOG("Admin", "Auth failed: no PIN provided");
            sendAdminState(client);
            return;
        }

        // Check rate limit
        if (!checkRateLimit(client->remoteIP())) {
            WEB_LOG("Admin", "Auth failed: rate limited");
            sendAdminState(client);
            return;
        }

        // Verify PIN
        String storedPin = storage.getAdminPin();
        if (storedPin.length() > 0 && storedPin == pin) {
            IPAddress clientIP = client->remoteIP();
            authenticateClient(client);
            clearFailedAttempts(clientIP);
            WEB_LOG("Admin", "IP %s unlocked", clientIP.toString().c_str());
            broadcastAdminStateToIP(clientIP);  // Notify all tabs from same IP
        } else {
            WEB_LOG("Admin", "Auth failed: wrong PIN from %s", client->remoteIP().toString().c_str());
            recordFailedAttempt(client->remoteIP());
            sendAdminState(client);
        }
    }
    else if (strcmp(type, "adminLock") == 0) {
        IPAddress clientIP = client->remoteIP();
        lockClient(client);
        WEB_LOG("Admin", "IP %s locked", clientIP.toString().c_str());
        broadcastAdminStateToIP(clientIP);  // Notify all tabs from same IP
    }
    else if (strcmp(type, "setAdminPin") == 0) {
        const char* pin = doc["pin"];
        if (!pin) {
            WEB_LOG("Admin", "Set PIN failed: no PIN provided");
            return;
        }

        // Must be unlocked OR no PIN configured (first-time setup)
        if (!isClientAuthenticated(client) && storage.hasAdminPin()) {
            WEB_LOG("Admin", "Set PIN failed: not authenticated");
            return;
        }

        if (storage.setAdminPin(pin)) {
            WEB_LOG("Admin", "Admin PIN set");
            // Re-authenticate this client with new PIN
            authenticateClient(client);
            sendAdminState(client);
        } else {
            WEB_LOG("Admin", "Set PIN failed: invalid format (must be 4-6 digits)");
        }
    }
    else if (strcmp(type, "clearAdminPin") == 0) {
        // Must be unlocked to clear PIN
        if (!isClientAuthenticated(client)) {
            WEB_LOG("Admin", "Clear PIN failed: not authenticated");
            return;
        }

        storage.clearAdminPin();
        WEB_LOG("Admin", "Admin PIN cleared");
        sendAdminState(client);
    }
    else if (strcmp(type, "getAdminState") == 0) {
        sendAdminState(client);
    }
    // ========================================================================
    // Update Check Commands
    // ========================================================================
    else if (strcmp(type, "checkForUpdate") == 0) {
        // Always allowed (read-only action)
        WEB_LOG("Update", "Manual update check requested");
        updateChecker.checkNow();
    }
    else if (strcmp(type, "setUpdateCheckEnabled") == 0) {
        // Protected: requires admin unlock
        if (!isClientAuthenticated(client)) {
            WEB_LOG("Admin", "setUpdateCheckEnabled blocked: not authenticated");
            sendAdminBlocked(client, "setUpdateCheckEnabled");
            return;
        }
        bool enabled = doc["enabled"] | true;
        updateChecker.setEnabled(enabled);
        WEB_LOG("Update", "Update check %s", enabled ? "enabled" : "disabled");
    }
    else if (strcmp(type, "setUpdateCheckInterval") == 0) {
        // Protected: requires admin unlock
        if (!isClientAuthenticated(client)) {
            WEB_LOG("Admin", "setUpdateCheckInterval blocked: not authenticated");
            sendAdminBlocked(client, "setUpdateCheckInterval");
            return;
        }
        uint8_t interval = doc["interval"] | 1;
        if (interval > 2) interval = 1;  // Validate: 0, 1, or 2
        updateChecker.setInterval(interval);
        WEB_LOG("Update", "Update check interval set to %d", interval);
    }
    else {
        WEB_LOG("WS", "Unknown command: %s", type);
    }

    // Request broadcast from main loop
    requestBroadcast();
}
