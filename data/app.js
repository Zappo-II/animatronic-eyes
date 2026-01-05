/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

// WebSocket connection
let ws = null;
let reconnectInterval = null;
let lastMessageTime = 0;
let heartbeatInterval = null;

// Runtime state (from continuous broadcast)
let state = {
    servos: [],
    wifi: {},
    system: {},
    eye: {},
    mode: {},
    impulse: {}
};

// Configuration (fetched on-demand)
let config = {
    networks: [{index: 0, ssid: '', configured: false}, {index: 1, ssid: '', configured: false}],
    wifiTiming: {grace: 3, retries: 3, retryDelay: 10, apScan: 5, keepAP: true},
    ap: {ssidPrefix: 'LookIntoMyEyes', hasPassword: true},
    led: {enabled: true, pin: 2},
    mdns: {enabled: true, hostname: 'animatronic-eyes'},
    mode: {defaultMode: 'follow', autoBlink: true, blinkIntervalMin: 2000, blinkIntervalMax: 6000, rememberLastMode: false, mirrorPreview: false},
    impulse: {autoImpulse: true, impulseIntervalMin: 30000, impulseIntervalMax: 120000, impulseSelection: 'startle,distraction'}
};

// Deep copy of config as last-saved reference
let savedConfig = JSON.parse(JSON.stringify(config));

// Dirty tracking per section
let dirtyState = {
    wifiPrimary: false,
    wifiSecondary: false,
    ap: false,
    wifiTiming: false,
    mdns: false,
    led: false,
    modeSettings: false,
    impulseSettings: false
};

// Local calibration state (not saved until explicit save)
let localCalibration = [];

// Active network slot for scan dropdown
let activeScanSlot = null;

// Gaze pad state
let gazePadActive = false;

// Lid slider interaction tracking
let lidSlidersActive = false;

// Control lockout - prevents WS state from overriding UI during/after user interaction
let controlsLockedUntil = 0;
const CONTROL_LOCK_MS = 300;  // Time to wait after user interaction (must be > WS_BROADCAST_INTERVAL_MS)

// Blink animation tracking - prevents WS state from fighting with local blink preview
let blinkAnimationActive = false;

// Mode selector lock - prevents WS state from reverting dropdown during mode switch
let modeSelectorLockedUntil = 0;

// Available impulses list (received once on connect, stored for config form population)
let availableImpulsesList = [];

// Track previous tab for Control tab re-entry logic
let previousTab = 'control';

// Admin lock state
let adminState = {
    locked: true,
    isAPClient: false,
    pinConfigured: false,
    remainingSeconds: 0
};

// Countdown timer for lock timeout display
let lockCountdownInterval = null;

// Throttle helper (leading + trailing edge)
// Fires immediately, then at most once per 'limit' ms, and always fires the last call
const throttle = (func, limit) => {
    let lastArgs = null;
    let timeoutId = null;
    let lastRan = 0;

    return function(...args) {
        const now = Date.now();

        if (now - lastRan >= limit) {
            // Enough time passed, fire immediately
            func.apply(this, args);
            lastRan = now;
            lastArgs = null;
        } else {
            // Save args for trailing call
            lastArgs = args;

            // Schedule trailing call if not already scheduled
            if (!timeoutId) {
                timeoutId = setTimeout(() => {
                    if (lastArgs) {
                        func.apply(this, lastArgs);
                        lastRan = Date.now();
                        lastArgs = null;
                    }
                    timeoutId = null;
                }, limit - (now - lastRan));
            }
        }
    };
};

// DOM elements
const connectionStatusEl = document.getElementById('connectionStatus');
const apIndicatorEl = document.getElementById('apIndicator');
const wifiIndicatorEl = document.getElementById('wifiIndicator');
const uiIndicatorEl = document.getElementById('uiIndicator');
const rebootIndicatorEl = document.getElementById('rebootIndicator');
const updateIndicatorEl = document.getElementById('updateIndicator');
const deviceIdEl = document.getElementById('deviceId');
// const servoControlsEl = document.getElementById('servoControls');  // LEGACY: removed in v0.5
const calibrationCardsEl = document.getElementById('calibrationCards');
const versionInfoEl = document.getElementById('versionInfo');

// Calibration dirty tracking
let savedCalibration = [];

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    createToastContainer();
    connectWebSocket();
    setupTabs();
    setupEventListeners();
    setupConfigSections();
    setupTooltips();
    setupConsole();
    setupEyeController();
    fetchVersion();
});

// Handle tab visibility changes (mobile/background tabs)
document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') {
        // Force reconnect check when tab becomes visible
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            if (reconnectInterval) {
                clearInterval(reconnectInterval);
                reconnectInterval = null;
            }
            connectWebSocket();
        }
    }
});

// Toast notifications
function createToastContainer() {
    if (!document.querySelector('.toast-container')) {
        const container = document.createElement('div');
        container.className = 'toast-container';
        document.body.appendChild(container);
    }
}

function showToast(message, type = 'info') {
    const container = document.querySelector('.toast-container');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    container.appendChild(toast);

    setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

// Tap-to-show tooltips for mobile
function setupTooltips() {
    let activeTooltip = null;

    function showTooltip(trigger) {
        hideTooltip();
        const text = trigger.getAttribute('title') || trigger.dataset.tooltip;
        if (!text) return;

        const popup = document.createElement('div');
        popup.className = 'tooltip-popup';
        popup.textContent = text;
        document.body.appendChild(popup);

        // Position below the trigger
        const rect = trigger.getBoundingClientRect();
        popup.style.left = Math.max(8, Math.min(rect.left, window.innerWidth - popup.offsetWidth - 8)) + 'px';
        popup.style.top = (rect.bottom + 8) + 'px';

        activeTooltip = popup;
    }

    function hideTooltip() {
        if (activeTooltip) {
            activeTooltip.remove();
            activeTooltip = null;
        }
    }

    // Click/tap handler for tooltip triggers
    document.addEventListener('click', (e) => {
        const trigger = e.target.closest('.tooltip-trigger');
        if (trigger) {
            e.preventDefault();
            if (activeTooltip) {
                hideTooltip();
            } else {
                showTooltip(trigger);
            }
        } else {
            hideTooltip();
        }
    });
}

// Tab Navigation
function setupTabs() {
    document.querySelectorAll('.tab').forEach(tab => {
        tab.addEventListener('click', () => {
            const tabId = tab.dataset.tab;

            // Update tab buttons
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            tab.classList.add('active');

            // Update tab content
            document.querySelectorAll('.tab-content').forEach(content => {
                content.classList.remove('active');
            });
            document.getElementById(`${tabId}Tab`).classList.add('active');

            // Save active tab
            localStorage.setItem('activeTab', tabId);

            // Fetch config when switching to Configuration tab
            if (tabId === 'configuration') {
                requestConfig();
            }

            // Request log history when switching to Console tab
            if (tabId === 'console') {
                requestLogHistory();
            }

            // Sync test sliders to actual servo positions when switching to Calibration tab
            if (tabId === 'calibration') {
                syncTestSlidersToServoPositions();
                // Pause auto-blink during calibration
                send({ type: 'pauseAutoBlink', paused: true });
            }

            // Re-apply Eye Controller state when returning to Control tab from Calibration
            if (tabId === 'control' && previousTab === 'calibration') {
                send({ type: 'reapplyEyeState' });
                showToast('Resuming previous pose', 'info');
            }

            // Resume auto-blink when leaving calibration tab
            if (previousTab === 'calibration' && tabId !== 'calibration') {
                send({ type: 'pauseAutoBlink', paused: false });
            }

            // Track previous tab
            previousTab = tabId;
        });
    });

    // Restore active tab
    const savedTab = localStorage.getItem('activeTab');
    if (savedTab) {
        const tab = document.querySelector(`.tab[data-tab="${savedTab}"]`);
        if (tab) tab.click();
    }
}

// WebSocket
function connectWebSocket() {
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(`${protocol}//${location.host}/ws`);

    ws.onopen = () => {
        console.log('WebSocket connected');
        if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
        }
        // Update status (will show "Connected" until adminState arrives)
        updateLockStatusIndicator();
        // Fetch config immediately so forms are populated correctly
        requestConfig();
    };

    ws.onclose = () => {
        console.log('WebSocket disconnected');
        connectionStatusEl.textContent = 'Disconnected';
        connectionStatusEl.className = 'connection-status disconnected';
        if (!reconnectInterval) {
            reconnectInterval = setInterval(connectWebSocket, 3000);
        }
    };

    ws.onerror = (err) => {
        console.error('WebSocket error:', err);
    };

    ws.onmessage = (event) => {
        lastMessageTime = Date.now();  // Track for heartbeat detection
        try {
            const data = JSON.parse(event.data);
            handleMessage(data);
        } catch (e) {
            console.error('Failed to parse message:', e);
        }
    };

    // Start heartbeat check - detect dead connection if no messages for 3 seconds
    if (heartbeatInterval) clearInterval(heartbeatInterval);
    heartbeatInterval = setInterval(() => {
        if (ws && ws.readyState === WebSocket.OPEN && lastMessageTime > 0) {
            if (Date.now() - lastMessageTime > 3000) {
                console.log('No heartbeat - connection lost');
                ws.close();  // Triggers onclose → shows Disconnected → starts reconnect
            }
        }
    }, 1000);
}

function handleMessage(data) {
    if (data.type === 'state') {
        // Runtime state only (no config data)
        state.wifi = data.wifi;
        state.servos = data.servos;
        state.system = data.system;
        state.eye = data.eye || {};
        state.mode = data.mode || {};
        state.impulse = data.impulse || {};
        state.update = data.update || {};

        // Update the Update Check UI
        updateUpdateCheckUI();

        // Initialize local calibration from server state on first load
        if (localCalibration.length === 0 && state.servos && state.servos.length > 0) {
            localCalibration = state.servos.map(s => ({
                pin: s.pin,
                min: s.min,
                center: s.center,
                max: s.max,
                invert: s.invert
            }));
            // Save initial state for dirty tracking
            savedCalibration = JSON.parse(JSON.stringify(localCalibration));
        }

        // NOTE: Mode dropdowns populated from 'availableModes' message on connect

        updateUI();
    } else if (data.type === 'config') {
        handleConfigResponse(data);
    } else if (data.type === 'networkList') {
        showNetworkDropdown(data.networks);
    } else if (data.type === 'log') {
        addConsoleLine(data.line);
    } else if (data.type === 'logHistory') {
        // Load history into console
        const output = document.getElementById('consoleOutput');
        if (output) {
            output.innerHTML = '';
            data.lines.forEach(line => addConsoleLine(line, false));
            // Scroll to bottom after loading history
            if (document.getElementById('consoleAutoscroll')?.checked) {
                output.scrollTop = output.scrollHeight;
            }
        }
    } else if (data.type === 'availableModes') {
        // Available modes sent once on connect
        if (data.modes && data.modes.length > 0) {
            populateModeDropdowns(data.modes);
        }
    } else if (data.type === 'availableImpulses') {
        // Available impulses sent once on connect - store for later use
        availableImpulsesList = data.impulses || [];
        // Populate if config is already loaded, otherwise will populate when config arrives
        if (savedConfig.impulse?.impulseSelection !== undefined) {
            populateImpulseSelection(availableImpulsesList, savedConfig.impulse.impulseSelection);
            updateProtectedControls();
        }
    } else if (data.type === 'adminState') {
        // Admin lock state - per-client
        adminState.locked = data.locked;
        adminState.isAPClient = data.isAPClient;
        adminState.pinConfigured = data.pinConfigured;
        adminState.remainingSeconds = data.remainingSeconds || 0;
        adminState.lockoutSeconds = data.lockoutSeconds || 0;

        // Handle PIN auth response
        if (awaitingPinAuth) {
            awaitingPinAuth = false;
            const modal = document.getElementById('pinModal');
            if (modal && modal.style.display === 'flex') {
                if (!adminState.locked) {
                    // Success - close modal
                    hidePinModal();
                    showToast('Unlocked successfully', 'success');
                } else if (adminState.lockoutSeconds > 0) {
                    // Rate limited - show lockout
                    showPinModal();  // Refresh to show lockout state
                } else {
                    // Wrong PIN
                    document.getElementById('pinError').textContent = 'Incorrect PIN';
                    document.getElementById('pinInput').value = '';
                }
            }
        }

        updateAdminLockUI();
    } else if (data.type === 'adminBlocked') {
        // Command was blocked due to admin lock
        showToast(`Action blocked: Admin lock is active`, 'error');
        // Refresh admin state in case UI is out of sync
        updateAdminLockUI();
    }
}

// Config Management
function requestConfig() {
    send({ type: 'getConfig' });
}

function handleConfigResponse(data) {
    config = {
        networks: data.networks || [],
        wifiTiming: data.wifiTiming || {},
        ap: data.ap || {},
        led: data.led || {},
        mdns: data.mdns || {},
        mode: data.mode || {defaultMode: 'follow', autoBlink: true, blinkIntervalMin: 2000, blinkIntervalMax: 6000, rememberLastMode: false, mirrorPreview: false},
        impulse: data.impulse || {autoImpulse: true, impulseIntervalMin: 30000, impulseIntervalMax: 120000, impulseSelection: 'startle,distraction'}
    };
    savedConfig = JSON.parse(JSON.stringify(config));

    // Populate all forms
    populateConfigForms();

    // Reset all dirty states
    Object.keys(dirtyState).forEach(key => {
        dirtyState[key] = false;
    });
    updateAllSectionStates();
    updateProtectedControls();
}

function populateConfigForms() {
    // WiFi Primary
    const primary = config.networks.find(n => n.index === 0) || {};
    document.getElementById('wifiPrimarySsid').value = primary.ssid || '';
    document.getElementById('wifiPrimaryPassword').value = '';
    document.getElementById('wifiPrimaryPassword').placeholder =
        primary.configured ? '\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022' : 'Password';

    // WiFi Secondary
    const secondary = config.networks.find(n => n.index === 1) || {};
    document.getElementById('wifiSecondarySsid').value = secondary.ssid || '';
    document.getElementById('wifiSecondaryPassword').value = '';
    document.getElementById('wifiSecondaryPassword').placeholder =
        secondary.configured ? '\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022' : 'Password';

    // AP Settings
    document.getElementById('apPrefix').value = config.ap.ssidPrefix || 'LookIntoMyEyes';
    document.getElementById('apPassword').value = '';
    document.getElementById('apPassword').placeholder =
        config.ap.hasPassword ? '\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022' : 'Min 8 characters';
    document.getElementById('keepAP').checked = config.wifiTiming.keepAP !== false;
    updateApSectionBadge();

    // WiFi Timing
    document.getElementById('wifiGrace').value = config.wifiTiming.grace || 3;
    document.getElementById('wifiRetries').value = config.wifiTiming.retries || 3;
    document.getElementById('wifiRetryDelay').value = config.wifiTiming.retryDelay || 10;
    document.getElementById('wifiAPScan').value = config.wifiTiming.apScan || 5;

    // mDNS
    document.getElementById('mdnsEnabled').checked = config.mdns.enabled !== false;
    document.getElementById('mdnsHostname').value = config.mdns.hostname || 'animatronic-eyes';
    updateMdnsSectionBadge();

    // LED
    document.getElementById('ledEnabled').checked = config.led.enabled !== false;
    document.getElementById('ledPin').value = config.led.pin || 2;
    document.getElementById('ledBrightness').value = config.led.brightness || 255;
    document.getElementById('ledBrightnessValue').textContent = config.led.brightness || 255;
    updateLedSectionBadge();

    // Update hints
    updateApNameHint();
    updateMdnsFullNameHint();

    // Mode Settings
    document.getElementById('autoBlinkEnabled').checked = config.mode.autoBlink !== false;
    document.getElementById('rememberLastMode').checked = config.mode.rememberLastMode === true;
    document.getElementById('mirrorPreview').checked = config.mode.mirrorPreview === true;
    document.getElementById('blinkIntervalMin').value = config.mode.blinkIntervalMin || 2000;
    document.getElementById('blinkIntervalMax').value = config.mode.blinkIntervalMax || 6000;
    // Default mode dropdown is populated by populateModeDropdowns when state is received
    updateDefaultModeDropdownState();

    // Impulse Settings
    document.getElementById('autoImpulseEnabled').checked = config.impulse.autoImpulse !== false;
    document.getElementById('impulseIntervalMin').value = (config.impulse.impulseIntervalMin || 30000) / 1000;  // Convert to seconds
    document.getElementById('impulseIntervalMax').value = (config.impulse.impulseIntervalMax || 120000) / 1000;
    // Impulse selection checkboxes populated from 'availableImpulses' message on connect
    // Also populate here if availableImpulsesList already received
    if (availableImpulsesList.length > 0) {
        populateImpulseSelection(availableImpulsesList, config.impulse.impulseSelection || '');
    }
}

// Disable startup mode dropdown when "remember" is checked (mutually exclusive)
function updateDefaultModeDropdownState() {
    const rememberCheckbox = document.getElementById('rememberLastMode');
    const defaultModeSelect = document.getElementById('defaultModeSelect');
    if (rememberCheckbox && defaultModeSelect) {
        defaultModeSelect.disabled = rememberCheckbox.checked;
    }
}

function updateMdnsSectionBadge() {
    const badge = document.getElementById('mdnsSectionStatus');
    const enabled = document.getElementById('mdnsEnabled')?.checked;
    if (badge) {
        badge.textContent = enabled ? 'on' : 'off';
        badge.classList.toggle('off', !enabled);
    }
}

function updateLedSectionBadge() {
    const badge = document.getElementById('ledSectionStatus');
    const enabled = document.getElementById('ledEnabled')?.checked;
    if (badge) {
        badge.textContent = enabled ? 'on' : 'off';
        badge.classList.toggle('off', !enabled);
    }
}

function updateApSectionBadge() {
    const badge = document.getElementById('apSectionStatus');
    const enabled = document.getElementById('keepAP')?.checked;
    if (badge) {
        badge.textContent = enabled ? 'on' : 'off';
        badge.classList.toggle('off', !enabled);
    }
}

function updateUpdateCheckUI() {
    const updateStatus = document.getElementById('updateStatus');
    const enabledCheckbox = document.getElementById('updateCheckEnabled');
    const intervalSelect = document.getElementById('updateCheckInterval');

    if (!state.update) return;

    // Update checkbox and select to match state
    if (enabledCheckbox) enabledCheckbox.checked = state.update.enabled !== false;
    if (intervalSelect) intervalSelect.value = state.update.interval ?? 1;

    // Update status display
    if (updateStatus) {
        updateStatus.classList.remove('update-available', 'up-to-date', 'checking');

        if (state.update.checking) {
            updateStatus.classList.add('checking');
            updateStatus.innerHTML = 'Checking for updates...';
        } else if (state.update.available && state.update.version) {
            updateStatus.classList.add('update-available');
            updateStatus.innerHTML = `Update available: <a href="https://github.com/Zappo-II/animatronic-eyes/releases" target="_blank">v${state.update.version}</a>`;
        } else if (state.update.lastCheck > 0) {
            updateStatus.classList.add('up-to-date');
            updateStatus.innerHTML = 'Firmware is up to date';
        } else {
            updateStatus.innerHTML = 'Not checked yet';
        }
    }
}

// Dirty State Management
function checkDirty(sectionName) {
    let isDirty = false;

    switch(sectionName) {
        case 'wifiPrimary': {
            const saved = savedConfig.networks.find(n => n.index === 0) || {};
            const ssid = document.getElementById('wifiPrimarySsid').value;
            const password = document.getElementById('wifiPrimaryPassword').value;
            isDirty = ssid !== (saved.ssid || '') || password.length > 0;
            break;
        }
        case 'wifiSecondary': {
            const saved = savedConfig.networks.find(n => n.index === 1) || {};
            const ssid = document.getElementById('wifiSecondarySsid').value;
            const password = document.getElementById('wifiSecondaryPassword').value;
            isDirty = ssid !== (saved.ssid || '') || password.length > 0;
            break;
        }
        case 'ap': {
            const prefix = document.getElementById('apPrefix').value;
            const password = document.getElementById('apPassword').value;
            const keepAP = document.getElementById('keepAP').checked;
            isDirty = prefix !== (savedConfig.ap.ssidPrefix || '') ||
                      password.length > 0 ||
                      keepAP !== (savedConfig.wifiTiming.keepAP !== false);
            break;
        }
        case 'wifiTiming': {
            isDirty =
                parseInt(document.getElementById('wifiGrace').value) !== (savedConfig.wifiTiming.grace || 3) ||
                parseInt(document.getElementById('wifiRetries').value) !== (savedConfig.wifiTiming.retries || 3) ||
                parseInt(document.getElementById('wifiRetryDelay').value) !== (savedConfig.wifiTiming.retryDelay || 10) ||
                parseInt(document.getElementById('wifiAPScan').value) !== (savedConfig.wifiTiming.apScan || 5);
            break;
        }
        case 'mdns': {
            isDirty =
                document.getElementById('mdnsEnabled').checked !== (savedConfig.mdns.enabled !== false) ||
                document.getElementById('mdnsHostname').value !== (savedConfig.mdns.hostname || '');
            break;
        }
        case 'led': {
            isDirty =
                document.getElementById('ledEnabled').checked !== (savedConfig.led.enabled !== false) ||
                parseInt(document.getElementById('ledPin').value) !== (savedConfig.led.pin || 2) ||
                parseInt(document.getElementById('ledBrightness').value) !== (savedConfig.led.brightness || 255);
            break;
        }
        case 'modeSettings': {
            isDirty =
                document.getElementById('defaultModeSelect').value !== (savedConfig.mode.defaultMode || 'follow') ||
                document.getElementById('autoBlinkEnabled').checked !== (savedConfig.mode.autoBlink !== false) ||
                document.getElementById('rememberLastMode').checked !== (savedConfig.mode.rememberLastMode === true) ||
                document.getElementById('mirrorPreview').checked !== (savedConfig.mode.mirrorPreview === true) ||
                parseInt(document.getElementById('blinkIntervalMin').value) !== (savedConfig.mode.blinkIntervalMin || 2000) ||
                parseInt(document.getElementById('blinkIntervalMax').value) !== (savedConfig.mode.blinkIntervalMax || 6000);
            break;
        }
        case 'impulseSettings': {
            isDirty =
                document.getElementById('autoImpulseEnabled').checked !== (savedConfig.impulse.autoImpulse !== false) ||
                parseInt(document.getElementById('impulseIntervalMin').value) * 1000 !== (savedConfig.impulse.impulseIntervalMin || 30000) ||
                parseInt(document.getElementById('impulseIntervalMax').value) * 1000 !== (savedConfig.impulse.impulseIntervalMax || 120000) ||
                getImpulseSelectionFromCheckboxes() !== (savedConfig.impulse.impulseSelection || '');
            break;
        }
    }

    dirtyState[sectionName] = isDirty;
    updateSectionState(sectionName);
}

function updateSectionState(sectionName) {
    const sectionEl = document.querySelector(`[data-section="${sectionName}"]`);
    if (!sectionEl) return;

    const isDirty = dirtyState[sectionName];

    // Update section border
    sectionEl.classList.toggle('dirty', isDirty);

    // Update dirty indicator dot
    const indicator = sectionEl.querySelector('.dirty-indicator');
    if (indicator) indicator.hidden = !isDirty;

    // Update button states
    const saveBtn = sectionEl.querySelector('.save-btn');
    const revertBtn = sectionEl.querySelector('.revert-btn');
    if (saveBtn) saveBtn.disabled = !isDirty;
    if (revertBtn) revertBtn.disabled = !isDirty;
}

function updateAllSectionStates() {
    Object.keys(dirtyState).forEach(updateSectionState);
}

// Save Handlers
function saveSection(sectionName) {
    switch(sectionName) {
        case 'wifiPrimary':
            saveWifiNetwork(0, 'wifiPrimary');
            break;
        case 'wifiSecondary':
            saveWifiNetwork(1, 'wifiSecondary');
            break;
        case 'ap':
            saveApConfig();
            break;
        case 'wifiTiming':
            saveWifiTiming();
            break;
        case 'mdns':
            saveMdnsConfig();
            break;
        case 'led':
            saveLedConfig();
            break;
        case 'modeSettings':
            saveModeSettings();
            break;
        case 'impulseSettings':
            saveImpulseSettings();
            break;
    }
}

// Helper to check admin lock before protected actions
function isAdminLocked() {
    if (adminState.pinConfigured && adminState.locked) {
        showToast('Cannot save: Admin lock is active', 'error');
        return true;
    }
    return false;
}

function saveWifiNetwork(index, sectionName) {
    if (isAdminLocked()) return;

    const prefix = index === 0 ? 'wifiPrimary' : 'wifiSecondary';
    const ssid = document.getElementById(`${prefix}Ssid`).value.trim();
    const password = document.getElementById(`${prefix}Password`).value;

    if (!ssid) {
        showToast('SSID is required', 'error');
        return;
    }

    send({
        type: 'setWifiNetwork',
        index: index,
        ssid: ssid,
        password: password || undefined
    });

    // Update savedConfig
    const netIndex = savedConfig.networks.findIndex(n => n.index === index);
    if (netIndex >= 0) {
        savedConfig.networks[netIndex].ssid = ssid;
        savedConfig.networks[netIndex].configured = true;
    } else {
        savedConfig.networks.push({index, ssid, configured: true});
    }

    // Clear password field, update placeholder
    document.getElementById(`${prefix}Password`).value = '';
    document.getElementById(`${prefix}Password`).placeholder = '\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022';

    dirtyState[sectionName] = false;
    updateSectionState(sectionName);
    showToast('Network saved', 'success');
}

function saveApConfig() {
    if (isAdminLocked()) return;

    const prefix = document.getElementById('apPrefix').value.trim() || 'LookIntoMyEyes';
    const password = document.getElementById('apPassword').value;
    const keepAP = document.getElementById('keepAP').checked;

    if (password && password.length < 8) {
        showToast('Password must be at least 8 characters', 'error');
        return;
    }

    send({
        type: 'setApConfig',
        ssidPrefix: prefix,
        password: password || undefined
    });

    // Also save keepAP
    send({ type: 'setKeepAP', enabled: keepAP });

    // Update savedConfig
    savedConfig.ap.ssidPrefix = prefix;
    if (password) savedConfig.ap.hasPassword = true;
    savedConfig.wifiTiming.keepAP = keepAP;

    // Clear password field
    document.getElementById('apPassword').value = '';
    document.getElementById('apPassword').placeholder = '\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022';

    dirtyState.ap = false;
    updateSectionState('ap');
    showToast('AP settings saved (reboot required)', 'success');
}

function saveWifiTiming() {
    if (isAdminLocked()) return;

    const grace = parseInt(document.getElementById('wifiGrace').value) || 3;
    const retries = parseInt(document.getElementById('wifiRetries').value) || 3;
    const retryDelay = parseInt(document.getElementById('wifiRetryDelay').value) || 10;
    const apScan = parseInt(document.getElementById('wifiAPScan').value) || 5;

    send({
        type: 'setWifiTiming',
        grace: grace,
        retries: retries,
        retryDelay: retryDelay,
        apScan: apScan,
        keepAP: savedConfig.wifiTiming.keepAP
    });

    // Update savedConfig
    savedConfig.wifiTiming.grace = grace;
    savedConfig.wifiTiming.retries = retries;
    savedConfig.wifiTiming.retryDelay = retryDelay;
    savedConfig.wifiTiming.apScan = apScan;

    dirtyState.wifiTiming = false;
    updateSectionState('wifiTiming');
    showToast('WiFi timing saved', 'success');
}

function saveMdnsConfig() {
    if (isAdminLocked()) return;

    const enabled = document.getElementById('mdnsEnabled').checked;
    let hostname = document.getElementById('mdnsHostname').value.trim();

    hostname = hostname.toLowerCase().replace(/[^a-z0-9-]/g, '');
    if (hostname.length === 0) hostname = 'animatronic-eyes';
    if (hostname.length > 63) hostname = hostname.substring(0, 63);

    document.getElementById('mdnsHostname').value = hostname;

    send({ type: 'setMdns', enabled: enabled, hostname: hostname });

    // Update savedConfig
    savedConfig.mdns.enabled = enabled;
    savedConfig.mdns.hostname = hostname;

    dirtyState.mdns = false;
    updateSectionState('mdns');
    showToast('mDNS settings saved (reboot required)', 'success');
}

function saveLedConfig() {
    if (isAdminLocked()) return;

    const enabled = document.getElementById('ledEnabled').checked;
    const pin = parseInt(document.getElementById('ledPin').value) || 2;
    const brightness = parseInt(document.getElementById('ledBrightness').value) || 255;

    send({ type: 'setLed', enabled: enabled, pin: pin, brightness: brightness });

    // Update savedConfig
    savedConfig.led.enabled = enabled;
    savedConfig.led.pin = pin;
    savedConfig.led.brightness = brightness;

    dirtyState.led = false;
    updateSectionState('led');
    showToast('LED settings saved', 'success');
}

function saveModeSettings() {
    const defaultMode = document.getElementById('defaultModeSelect').value;
    const autoBlink = document.getElementById('autoBlinkEnabled').checked;
    const rememberLastMode = document.getElementById('rememberLastMode').checked;
    const mirrorPreview = document.getElementById('mirrorPreview').checked;
    const blinkIntervalMin = parseInt(document.getElementById('blinkIntervalMin').value) || 2000;
    const blinkIntervalMax = parseInt(document.getElementById('blinkIntervalMax').value) || 6000;

    // Validate interval values
    if (blinkIntervalMin > blinkIntervalMax) {
        showToast('Min interval must be less than max', 'error');
        return;
    }

    // Send to device
    send({ type: 'setDefaultMode', mode: defaultMode });
    send({ type: 'setAutoBlink', enabled: autoBlink });
    send({ type: 'setRememberLastMode', enabled: rememberLastMode });
    send({ type: 'setMirrorPreview', enabled: mirrorPreview });
    send({ type: 'setBlinkInterval', min: blinkIntervalMin, max: blinkIntervalMax });

    // Update savedConfig
    savedConfig.mode.defaultMode = defaultMode;
    savedConfig.mode.autoBlink = autoBlink;
    savedConfig.mode.rememberLastMode = rememberLastMode;
    savedConfig.mode.mirrorPreview = mirrorPreview;
    savedConfig.mode.blinkIntervalMin = blinkIntervalMin;
    savedConfig.mode.blinkIntervalMax = blinkIntervalMax;

    dirtyState.modeSettings = false;
    updateSectionState('modeSettings');
    showToast('Mode settings saved', 'success');
}

function saveImpulseSettings() {
    const autoImpulse = document.getElementById('autoImpulseEnabled').checked;
    const impulseIntervalMin = parseInt(document.getElementById('impulseIntervalMin').value) * 1000 || 30000;
    const impulseIntervalMax = parseInt(document.getElementById('impulseIntervalMax').value) * 1000 || 120000;
    const impulseSelection = getImpulseSelectionFromCheckboxes();

    // Validate interval values
    if (impulseIntervalMin > impulseIntervalMax) {
        showToast('Min interval must be less than max', 'error');
        return;
    }

    // Send to device
    send({ type: 'setAutoImpulse', enabled: autoImpulse });
    send({ type: 'setImpulseInterval', min: impulseIntervalMin, max: impulseIntervalMax });
    send({ type: 'setImpulseSelection', selection: impulseSelection });

    // Update savedConfig
    savedConfig.impulse.autoImpulse = autoImpulse;
    savedConfig.impulse.impulseIntervalMin = impulseIntervalMin;
    savedConfig.impulse.impulseIntervalMax = impulseIntervalMax;
    savedConfig.impulse.impulseSelection = impulseSelection;

    dirtyState.impulseSettings = false;
    updateSectionState('impulseSettings');
    showToast('Impulse settings saved', 'success');
}

// Get comma-separated list of selected impulses from checkboxes
function getImpulseSelectionFromCheckboxes() {
    const container = document.getElementById('impulseSelection');
    if (!container) return '';

    const checkboxes = container.querySelectorAll('input[type="checkbox"]:checked');
    return Array.from(checkboxes).map(cb => cb.value).join(',');
}

// Set checkboxes based on comma-separated selection string
function setImpulseSelectionCheckboxes(selection) {
    const container = document.getElementById('impulseSelection');
    if (!container) return;

    const selected = selection ? selection.split(',').map(s => s.trim()) : [];

    container.querySelectorAll('input[type="checkbox"]').forEach(cb => {
        cb.checked = selected.includes(cb.value);
    });
}

// Populate impulse selection checkboxes from available impulses
function populateImpulseSelection(availableImpulses, currentSelection) {
    const container = document.getElementById('impulseSelection');
    if (!container) return;

    container.innerHTML = '';

    if (!availableImpulses || availableImpulses.length === 0) {
        container.innerHTML = '<span class="empty-message">No impulses available</span>';
        return;
    }

    const selected = currentSelection ? currentSelection.split(',').map(s => s.trim()) : [];

    availableImpulses.forEach(impulseName => {
        const label = document.createElement('label');
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.value = impulseName;
        checkbox.checked = selected.includes(impulseName);
        checkbox.addEventListener('change', () => checkSectionDirty('impulseSettings'));

        const displayName = impulseName.charAt(0).toUpperCase() + impulseName.slice(1);
        label.appendChild(checkbox);
        label.appendChild(document.createTextNode(displayName));
        container.appendChild(label);
    });
}

// Revert Handlers
function revertSection(sectionName) {
    switch(sectionName) {
        case 'wifiPrimary': {
            const saved = savedConfig.networks.find(n => n.index === 0) || {};
            document.getElementById('wifiPrimarySsid').value = saved.ssid || '';
            document.getElementById('wifiPrimaryPassword').value = '';
            break;
        }
        case 'wifiSecondary': {
            const saved = savedConfig.networks.find(n => n.index === 1) || {};
            document.getElementById('wifiSecondarySsid').value = saved.ssid || '';
            document.getElementById('wifiSecondaryPassword').value = '';
            break;
        }
        case 'ap': {
            document.getElementById('apPrefix').value = savedConfig.ap.ssidPrefix || '';
            document.getElementById('apPassword').value = '';
            document.getElementById('keepAP').checked = savedConfig.wifiTiming.keepAP !== false;
            break;
        }
        case 'wifiTiming': {
            document.getElementById('wifiGrace').value = savedConfig.wifiTiming.grace || 3;
            document.getElementById('wifiRetries').value = savedConfig.wifiTiming.retries || 3;
            document.getElementById('wifiRetryDelay').value = savedConfig.wifiTiming.retryDelay || 10;
            document.getElementById('wifiAPScan').value = savedConfig.wifiTiming.apScan || 5;
            break;
        }
        case 'mdns': {
            document.getElementById('mdnsEnabled').checked = savedConfig.mdns.enabled !== false;
            document.getElementById('mdnsHostname').value = savedConfig.mdns.hostname || '';
            break;
        }
        case 'led': {
            document.getElementById('ledEnabled').checked = savedConfig.led.enabled !== false;
            document.getElementById('ledPin').value = savedConfig.led.pin || 2;
            document.getElementById('ledBrightness').value = savedConfig.led.brightness || 255;
            document.getElementById('ledBrightnessValue').textContent = savedConfig.led.brightness || 255;
            break;
        }
        case 'modeSettings': {
            document.getElementById('defaultModeSelect').value = savedConfig.mode.defaultMode || 'follow';
            document.getElementById('autoBlinkEnabled').checked = savedConfig.mode.autoBlink !== false;
            document.getElementById('rememberLastMode').checked = savedConfig.mode.rememberLastMode === true;
            document.getElementById('mirrorPreview').checked = savedConfig.mode.mirrorPreview === true;
            document.getElementById('blinkIntervalMin').value = savedConfig.mode.blinkIntervalMin || 2000;
            document.getElementById('blinkIntervalMax').value = savedConfig.mode.blinkIntervalMax || 6000;
            break;
        }
        case 'impulseSettings': {
            document.getElementById('autoImpulseEnabled').checked = savedConfig.impulse.autoImpulse !== false;
            document.getElementById('impulseIntervalMin').value = (savedConfig.impulse.impulseIntervalMin || 30000) / 1000;
            document.getElementById('impulseIntervalMax').value = (savedConfig.impulse.impulseIntervalMax || 120000) / 1000;
            setImpulseSelectionCheckboxes(savedConfig.impulse.impulseSelection || '');
            break;
        }
    }

    dirtyState[sectionName] = false;
    updateSectionState(sectionName);
}

// Forget network
function forgetNetwork(index, sectionName) {
    const networkName = index === 0 ? 'primary' : 'secondary';
    if (!confirm(`Forget ${networkName} network?`)) return;

    send({ type: 'clearWifiNetwork', index: index });

    // Update savedConfig
    const netIndex = savedConfig.networks.findIndex(n => n.index === index);
    if (netIndex >= 0) {
        savedConfig.networks[netIndex].ssid = '';
        savedConfig.networks[netIndex].configured = false;
    }

    // Clear form
    const prefix = index === 0 ? 'wifiPrimary' : 'wifiSecondary';
    document.getElementById(`${prefix}Ssid`).value = '';
    document.getElementById(`${prefix}Password`).value = '';
    document.getElementById(`${prefix}Password`).placeholder = 'Password';

    dirtyState[sectionName] = false;
    updateSectionState(sectionName);
    showToast('Network forgotten', 'info');
}

// Config Section Setup
function setupConfigSections() {
    // Reload config button with loading state
    document.getElementById('reloadConfig').addEventListener('click', (e) => {
        const btn = e.currentTarget;
        btn.classList.add('loading');
        requestConfig();
        // Remove loading after a short delay (config response will update UI)
        setTimeout(() => {
            btn.classList.remove('loading');
            showToast('Configuration reloaded', 'success');
        }, 500);
    });

    // Section collapse toggles
    document.querySelectorAll('.config-section .section-header').forEach(header => {
        header.addEventListener('click', (e) => {
            // Don't collapse if clicking on dirty indicator
            if (e.target.classList.contains('dirty-indicator')) return;
            const section = header.closest('.config-section');
            const wasExpanded = !section.classList.contains('collapsed');
            section.classList.toggle('collapsed');

            // If collapsing the About section, expand WiFi Status
            if (section.dataset.section === 'about' && wasExpanded) {
                const wifiStatusSection = document.querySelector('.config-section[data-section="wifiStatus"]');
                if (wifiStatusSection) wifiStatusSection.classList.remove('collapsed');
            }
        });
    });

    // Input change listeners for dirty tracking
    const configInputs = [
        {id: 'wifiPrimarySsid', section: 'wifiPrimary'},
        {id: 'wifiPrimaryPassword', section: 'wifiPrimary'},
        {id: 'wifiSecondarySsid', section: 'wifiSecondary'},
        {id: 'wifiSecondaryPassword', section: 'wifiSecondary'},
        {id: 'apPrefix', section: 'ap'},
        {id: 'apPassword', section: 'ap'},
        {id: 'keepAP', section: 'ap'},
        {id: 'wifiGrace', section: 'wifiTiming'},
        {id: 'wifiRetries', section: 'wifiTiming'},
        {id: 'wifiRetryDelay', section: 'wifiTiming'},
        {id: 'wifiAPScan', section: 'wifiTiming'},
        {id: 'mdnsEnabled', section: 'mdns'},
        {id: 'mdnsHostname', section: 'mdns'},
        {id: 'ledEnabled', section: 'led'},
        {id: 'ledPin', section: 'led'},
        {id: 'ledBrightness', section: 'led'},
        {id: 'defaultModeSelect', section: 'modeSettings'},
        {id: 'autoBlinkEnabled', section: 'modeSettings'},
        {id: 'rememberLastMode', section: 'modeSettings'},
        {id: 'mirrorPreview', section: 'modeSettings'},
        {id: 'blinkIntervalMin', section: 'modeSettings'},
        {id: 'blinkIntervalMax', section: 'modeSettings'},
        {id: 'autoImpulseEnabled', section: 'impulseSettings'},
        {id: 'impulseIntervalMin', section: 'impulseSettings'},
        {id: 'impulseIntervalMax', section: 'impulseSettings'}
    ];

    configInputs.forEach(({id, section}) => {
        const el = document.getElementById(id);
        if (el) {
            el.addEventListener('input', () => checkDirty(section));
            el.addEventListener('change', () => checkDirty(section));
        }
    });

    // Remember checkbox toggles dropdown disabled state
    document.getElementById('rememberLastMode')?.addEventListener('change', updateDefaultModeDropdownState);

    // Save buttons
    document.querySelectorAll('.config-section .save-btn').forEach(btn => {
        const section = btn.closest('.config-section');
        const sectionName = section?.dataset.section;
        if (sectionName && sectionName !== 'wifiStatus' && sectionName !== 'system') {
            btn.addEventListener('click', () => saveSection(sectionName));
        }
    });

    // Revert buttons
    document.querySelectorAll('.config-section .revert-btn').forEach(btn => {
        const section = btn.closest('.config-section');
        const sectionName = section?.dataset.section;
        if (sectionName && sectionName !== 'wifiStatus' && sectionName !== 'system') {
            btn.addEventListener('click', () => revertSection(sectionName));
        }
    });

    // Forget buttons
    document.querySelector('[data-section="wifiPrimary"] .forget-btn')?.addEventListener('click', () => {
        forgetNetwork(0, 'wifiPrimary');
    });
    document.querySelector('[data-section="wifiSecondary"] .forget-btn')?.addEventListener('click', () => {
        forgetNetwork(1, 'wifiSecondary');
    });

    // Network scan buttons
    document.querySelectorAll('.scan-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const slot = parseInt(btn.dataset.slot);
            activeScanSlot = slot;
            const dropdown = document.querySelector(`.network-dropdown[data-slot="${slot}"]`);
            dropdown.classList.remove('hidden');
            dropdown.innerHTML = '<div class="network-scanning">Scanning...</div>';
            send({ type: 'scanNetworks' });
        });
    });

    // Password visibility toggles
    document.querySelectorAll('.toggle-password').forEach(btn => {
        btn.addEventListener('click', () => {
            const row = btn.closest('.password-row');
            const input = row.querySelector('input[type="password"], input[type="text"]');
            const isShowing = btn.classList.toggle('showing');
            input.type = isShowing ? 'text' : 'password';
        });
    });

    // Close dropdowns when clicking outside
    document.addEventListener('click', (e) => {
        if (!e.target.closest('.ssid-row') && !e.target.closest('.network-dropdown')) {
            document.querySelectorAll('.network-dropdown').forEach(d => d.classList.add('hidden'));
        }
    });
}

function showNetworkDropdown(networks) {
    if (activeScanSlot === null) return;

    const dropdown = document.querySelector(`.network-dropdown[data-slot="${activeScanSlot}"]`);
    const ssidInput = activeScanSlot === 0
        ? document.getElementById('wifiPrimarySsid')
        : document.getElementById('wifiSecondarySsid');
    const passwordInput = activeScanSlot === 0
        ? document.getElementById('wifiPrimaryPassword')
        : document.getElementById('wifiSecondaryPassword');
    const sectionName = activeScanSlot === 0 ? 'wifiPrimary' : 'wifiSecondary';

    if (!networks || networks.length === 0) {
        dropdown.innerHTML = '<div class="network-scanning">No networks found</div>';
        return;
    }

    // Sort by signal strength
    networks.sort((a, b) => b.rssi - a.rssi);

    dropdown.innerHTML = networks.map(net => `
        <div class="network-item" data-ssid="${net.ssid}" data-secure="${net.secure}">
            <span class="ssid">
                ${net.secure ? '<svg class="lock-icon" viewBox="0 0 24 24"><path fill="currentColor" d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm-6 9c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1 1.71 0 3.1 1.39 3.1 3.1v2z"/></svg>' : ''}
                ${net.ssid}
            </span>
            <span class="signal-rssi">${net.rssi} dBm</span>
        </div>
    `).join('');

    // Click handlers for network items
    dropdown.querySelectorAll('.network-item').forEach(item => {
        item.addEventListener('click', () => {
            ssidInput.value = item.dataset.ssid;
            dropdown.classList.add('hidden');
            passwordInput.value = '';
            passwordInput.focus();
            checkDirty(sectionName);
        });
    });
}

function updateUI() {
    updateIndicators();
    // updateServos();  // LEGACY: removed in v0.5 Eye Controller UI
    updateCalibrationCards();
    updateWifiStatus();
    updateEyeController();
    updateModeSelectors();
}

// ============================================================================
// Admin Lock UI
// ============================================================================

function updateAdminLockUI() {
    // Update connection status with lock indicator
    updateLockStatusIndicator();
    // Update tab lock icons
    updateTabLockIcons();
    // Update disabled state of protected controls
    updateProtectedControls();
    // Update PIN management section
    updatePinManagement();
    // Start/stop countdown timer if needed
    updateLockCountdown();
}

function updateLockStatusIndicator() {
    const statusEl = document.getElementById('connectionStatus');
    if (!statusEl) return;

    // Check WebSocket state first
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        // Not connected - don't show lock status
        return;
    }

    // Build base status with IP if available
    let baseText = 'Connected';
    if (state.wifi && state.wifi.ip) {
        baseText = `Connected - ${state.wifi.ip}`;
    }

    // Handle reconnecting state
    if (state.wifi && state.wifi.reconnecting) {
        const attempt = state.wifi.reconnectAttempt || '?';
        statusEl.textContent = `Reconnecting (${attempt})...`;
        statusEl.className = 'connection-status reconnecting';
        return;
    }

    // No PIN configured - show simple connected status
    if (!adminState.pinConfigured) {
        statusEl.textContent = baseText;
        statusEl.className = 'connection-status connected';
        return;
    }

    // PIN configured - show lock state
    if (adminState.locked) {
        // Locked: green background + lock icon
        statusEl.innerHTML = `<span class="status-icon">&#x1F512;</span> ${baseText}`;
        statusEl.className = 'connection-status connected locked';
    } else {
        // Unlocked: yellow background + hammer/wrench icon + countdown
        let lockText = baseText;
        if (adminState.remainingSeconds > 0) {
            const mins = Math.floor(adminState.remainingSeconds / 60);
            const secs = adminState.remainingSeconds % 60;
            const timeStr = mins > 0 ? `${mins}m ${secs}s` : `${secs}s`;
            lockText = `${baseText} (${timeStr})`;
        }
        statusEl.innerHTML = `<span class="status-icon">&#x1F6E0;</span> ${lockText}`;
        statusEl.className = 'connection-status connected unlocked';
    }
}

function updateTabLockIcons() {
    // Add lock icons to Calibration and Configuration tabs when locked
    const calibTab = document.querySelector('.tab[data-tab="calibration"]');
    const configTab = document.querySelector('.tab[data-tab="configuration"]');

    if (adminState.pinConfigured && adminState.locked) {
        if (calibTab && !calibTab.querySelector('.tab-lock')) {
            calibTab.innerHTML += '<span class="tab-lock">&#x1F512;</span>';
        }
        if (configTab && !configTab.querySelector('.tab-lock')) {
            configTab.innerHTML += '<span class="tab-lock">&#x1F512;</span>';
        }
    } else {
        // Remove lock icons
        document.querySelectorAll('.tab-lock').forEach(el => el.remove());
    }
}

function updateProtectedControls() {
    // Disable/enable protected form controls based on lock state
    const isLocked = adminState.pinConfigured && adminState.locked;
    const isRateLimited = adminState.lockoutSeconds > 0;

    // All inputs in configuration tab (except PIN management, reboot, and checkUpdateBtn which are always allowed)
    const configInputs = document.querySelectorAll('#configurationTab input:not(.pin-input), #configurationTab select, #configurationTab button:not(.pin-btn):not(#reboot):not(#checkUpdateBtn)');
    configInputs.forEach(input => {
        input.disabled = isLocked;
    });

    // Reboot button: allowed when locked, blocked only when rate limited
    const rebootBtn = document.getElementById('reboot');
    if (rebootBtn) {
        rebootBtn.disabled = isRateLimited;
    }

    // Calibration tab header buttons (Save All, Reset, Center All)
    const calibHeaderButtons = document.querySelectorAll('#calibrationTab .header-buttons button');
    calibHeaderButtons.forEach(btn => {
        btn.disabled = isLocked;
    });

    // All inputs in calibration cards (sliders, number inputs, checkboxes)
    const calibInputs = document.querySelectorAll('#calibrationTab .calibration-card input');
    calibInputs.forEach(input => {
        input.disabled = isLocked;
    });

    // Add locked overlay to calibration cards
    document.querySelectorAll('.calibration-card').forEach(card => {
        if (isLocked) {
            card.classList.add('locked');
        } else {
            card.classList.remove('locked');
        }
    });
}

function updateLockCountdown() {
    // Clear existing interval
    if (lockCountdownInterval) {
        clearInterval(lockCountdownInterval);
        lockCountdownInterval = null;
    }

    // Only run countdown if unlocked with remaining time
    if (!adminState.locked && adminState.remainingSeconds > 0) {
        lockCountdownInterval = setInterval(() => {
            adminState.remainingSeconds--;
            if (adminState.remainingSeconds <= 0) {
                clearInterval(lockCountdownInterval);
                lockCountdownInterval = null;
                // Optimistically set locked state before server confirms
                adminState.locked = true;
                updateLockStatusIndicator();
                updateTabLockIcons();
                updateProtectedControls();
                // Request fresh state from server to confirm
                send({ type: 'getAdminState' });
                return;
            }
            updateLockStatusIndicator();
        }, 1000);
    }
}

// ============================================================================
// PIN Modal
// ============================================================================

function showPinModal() {
    let modal = document.getElementById('pinModal');
    if (!modal) {
        // Create modal dynamically
        modal = document.createElement('div');
        modal.id = 'pinModal';
        modal.className = 'modal';
        modal.innerHTML = `
            <div class="modal-content pin-modal">
                <h3 id="pinModalTitle">Enter Admin PIN</h3>
                <div id="pinLockoutMsg" class="pin-lockout" style="display:none">
                    <div class="lockout-icon">&#x1F6AB;</div>
                    <div class="lockout-text">Too many failed attempts</div>
                    <div class="lockout-countdown">Try again in <span id="lockoutCountdown">0:00</span></div>
                </div>
                <div id="pinEntryArea">
                    <div class="pin-display">
                        <input type="password" id="pinInput" maxlength="6" inputmode="numeric" pattern="[0-9]*" readonly>
                    </div>
                    <div class="pin-keypad">
                        <button class="pin-key" data-key="1">1</button>
                        <button class="pin-key" data-key="2">2</button>
                        <button class="pin-key" data-key="3">3</button>
                        <button class="pin-key" data-key="4">4</button>
                        <button class="pin-key" data-key="5">5</button>
                        <button class="pin-key" data-key="6">6</button>
                        <button class="pin-key" data-key="7">7</button>
                        <button class="pin-key" data-key="8">8</button>
                        <button class="pin-key" data-key="9">9</button>
                        <button class="pin-key pin-clear" data-key="clear">C</button>
                        <button class="pin-key" data-key="0">0</button>
                        <button class="pin-key pin-back" data-key="back">&larr;</button>
                    </div>
                    <div class="pin-actions">
                        <button class="btn btn-secondary" onclick="hidePinModal()">Cancel</button>
                        <button class="btn btn-primary" onclick="submitPin()">Unlock</button>
                    </div>
                    <div id="pinError" class="pin-error"></div>
                </div>
                <div id="pinLockoutActions" class="pin-actions" style="display:none">
                    <button class="btn btn-secondary" onclick="hidePinModal()">Close</button>
                </div>
            </div>
        `;
        document.body.appendChild(modal);

        // Add keypad event listeners
        modal.querySelectorAll('.pin-key').forEach(btn => {
            btn.addEventListener('click', () => {
                const key = btn.dataset.key;
                const input = document.getElementById('pinInput');
                if (key === 'clear') {
                    input.value = '';
                } else if (key === 'back') {
                    input.value = input.value.slice(0, -1);
                } else if (input.value.length < 6) {
                    input.value += key;
                }
            });
        });
    }

    // Check if rate limited
    if (adminState.lockoutSeconds > 0) {
        // Show lockout message, hide PIN entry
        document.getElementById('pinModalTitle').textContent = 'Access Temporarily Blocked';
        document.getElementById('pinLockoutMsg').style.display = 'block';
        document.getElementById('pinEntryArea').style.display = 'none';
        document.getElementById('pinLockoutActions').style.display = 'flex';
        updateLockoutCountdownDisplay();
        startLockoutCountdown();
    } else {
        // Show PIN entry, hide lockout message
        document.getElementById('pinModalTitle').textContent = 'Enter Admin PIN';
        document.getElementById('pinLockoutMsg').style.display = 'none';
        document.getElementById('pinEntryArea').style.display = 'block';
        document.getElementById('pinLockoutActions').style.display = 'none';
        document.getElementById('pinInput').value = '';
        document.getElementById('pinError').textContent = '';
    }

    modal.style.display = 'flex';
}

let lockoutCountdownInterval = null;

function updateLockoutCountdownDisplay() {
    const countdownEl = document.getElementById('lockoutCountdown');
    if (!countdownEl) return;
    const mins = Math.floor(adminState.lockoutSeconds / 60);
    const secs = adminState.lockoutSeconds % 60;
    countdownEl.textContent = `${mins}:${secs.toString().padStart(2, '0')}`;
}

function startLockoutCountdown() {
    if (lockoutCountdownInterval) {
        clearInterval(lockoutCountdownInterval);
    }
    lockoutCountdownInterval = setInterval(() => {
        adminState.lockoutSeconds--;
        updateLockoutCountdownDisplay();
        if (adminState.lockoutSeconds <= 0) {
            clearInterval(lockoutCountdownInterval);
            lockoutCountdownInterval = null;
            // Refresh the modal to show PIN entry
            const modal = document.getElementById('pinModal');
            if (modal && modal.style.display === 'flex') {
                showPinModal();
            }
        }
    }, 1000);
}

function hidePinModal() {
    const modal = document.getElementById('pinModal');
    if (modal) modal.style.display = 'none';
}

let awaitingPinAuth = false;

function submitPin() {
    const pin = document.getElementById('pinInput').value;
    if (pin.length < 4) {
        document.getElementById('pinError').textContent = 'PIN must be at least 4 digits';
        return;
    }
    awaitingPinAuth = true;
    document.getElementById('pinError').textContent = '';
    send({ type: 'adminAuth', pin: pin });
    // Don't close modal - wait for server response
}

function adminLock() {
    send({ type: 'adminLock' });
}

// ============================================================================
// PIN Management (Config tab)
// ============================================================================

function updatePinManagement() {
    const statusEl = document.getElementById('pinStatus');
    const setPinBtn = document.getElementById('setPinBtn');
    const clearPinBtn = document.getElementById('clearPinBtn');
    const pinInput = document.getElementById('newPinInput');

    if (!statusEl) return;

    if (adminState.pinConfigured) {
        statusEl.textContent = adminState.locked ?
            'PIN configured - Locked' :
            'PIN configured - Unlocked';
        statusEl.className = 'pin-status configured';
        setPinBtn.textContent = 'Change PIN';
        clearPinBtn.style.display = 'inline-block';

        // Only allow changes when unlocked
        const canChange = !adminState.locked || adminState.isAPClient;
        setPinBtn.disabled = !canChange;
        clearPinBtn.disabled = !canChange;
        pinInput.disabled = !canChange;
    } else {
        statusEl.textContent = 'PIN not configured - All clients have full access';
        statusEl.className = 'pin-status not-configured';
        setPinBtn.textContent = 'Set PIN';
        clearPinBtn.style.display = 'none';
        setPinBtn.disabled = false;
        pinInput.disabled = false;
    }
}

function setAdminPin() {
    const input = document.getElementById('newPinInput');
    const pin = input.value.trim();

    if (!/^\d{4,6}$/.test(pin)) {
        showToast('PIN must be 4-6 digits', 'error');
        return;
    }

    send({ type: 'setAdminPin', pin: pin });
    input.value = '';
    showToast('PIN updated', 'success');
}

function clearAdminPin() {
    if (!confirm('Clear admin PIN?\n\nThis will disable protection - all clients will have full access.')) {
        return;
    }
    send({ type: 'clearAdminPin' });
    showToast('PIN cleared', 'success');
}

// Status Indicators
function updateIndicators() {
    // Device ID
    if (state.system && state.system.deviceId) {
        deviceIdEl.textContent = state.system.deviceId;
    }

    // AP indicator with detailed tooltip
    if (state.wifi && state.wifi.apActive) {
        apIndicatorEl.classList.add('active');
        apIndicatorEl.classList.remove('hidden');
        apIndicatorEl.title = `Active: ${state.wifi.apName} (${state.wifi.apIp})`;
    } else {
        apIndicatorEl.classList.remove('active');
        apIndicatorEl.title = 'Inactive';
    }

    // WiFi indicator with detailed tooltip
    if (state.wifi && state.wifi.connected) {
        wifiIndicatorEl.classList.add('active');
        wifiIndicatorEl.classList.remove('error', 'warning');
        wifiIndicatorEl.title = `Connected: ${state.wifi.ssid} (${state.wifi.ip})`;
    } else if (state.wifi && state.wifi.reconnecting) {
        wifiIndicatorEl.classList.remove('active');
        wifiIndicatorEl.classList.add('warning');
        wifiIndicatorEl.title = `Reconnecting to ${state.wifi.ssid} (attempt ${state.wifi.reconnectAttempt})`;
    } else {
        wifiIndicatorEl.classList.remove('active', 'warning');
        wifiIndicatorEl.title = 'Disconnected';
    }

    // UI indicator with detailed tooltip based on uiStatus
    // Status: "missing", "fw_too_old", "ui_too_old", "major_mismatch", "minor_mismatch", "ok"
    if (state.system) {
        const status = state.system.uiStatus || 'missing';
        const version = state.system.uiVersion || 'unknown';

        uiIndicatorEl.classList.remove('active', 'warning', 'error');

        switch (status) {
            case 'ok':
                uiIndicatorEl.classList.add('active');
                uiIndicatorEl.title = `UI v${version} (ok)`;
                uiIndicatorEl.dataset.tooltip = `UI v${version} (ok)`;
                break;
            case 'minor_mismatch':
                uiIndicatorEl.classList.add('warning');
                uiIndicatorEl.title = `UI v${version} (minor version mismatch)`;
                uiIndicatorEl.dataset.tooltip = `UI v${version} (minor version mismatch)`;
                break;
            case 'major_mismatch':
                uiIndicatorEl.classList.add('error');
                uiIndicatorEl.title = `UI v${version} (major version mismatch)`;
                uiIndicatorEl.dataset.tooltip = `UI v${version} (major version mismatch)`;
                break;
            case 'fw_too_old':
                uiIndicatorEl.classList.add('error');
                uiIndicatorEl.title = `UI v${version} (firmware too old)`;
                uiIndicatorEl.dataset.tooltip = `UI v${version} (firmware too old)`;
                break;
            case 'ui_too_old':
                uiIndicatorEl.classList.add('error');
                uiIndicatorEl.title = `UI v${version} (UI too old)`;
                uiIndicatorEl.dataset.tooltip = `UI v${version} (UI too old)`;
                break;
            case 'missing':
            default:
                uiIndicatorEl.classList.add('error');
                uiIndicatorEl.title = 'UI files missing or invalid';
                uiIndicatorEl.dataset.tooltip = 'UI files missing or invalid';
                break;
        }
    }

    // Reboot indicator with tooltip
    if (state.system && state.system.rebootRequired) {
        rebootIndicatorEl.classList.remove('hidden');
        rebootIndicatorEl.title = 'Settings changed - reboot required';
    } else {
        rebootIndicatorEl.classList.add('hidden');
    }

    // Update indicator with tooltip
    if (state.update && state.update.available && state.update.version) {
        updateIndicatorEl.classList.remove('hidden');
        updateIndicatorEl.title = `Update available: v${state.update.version}`;
    } else {
        updateIndicatorEl.classList.add('hidden');
    }
}

/* LEGACY: Servo Controls removed in v0.5 Eye Controller UI
 * Kept for reference - delete after v1.0
 *
function updateServos() {
    if (!state.servos || state.servos.length === 0) return;
    if (!servoControlsEl) return;

    if (servoControlsEl.children.length !== state.servos.length) {
        servoControlsEl.innerHTML = '';
        state.servos.forEach((servo, index) => {
            servoControlsEl.appendChild(createServoControl(servo, index));
        });
    } else {
        state.servos.forEach((servo, index) => {
            const container = servoControlsEl.children[index];
            const slider = container.querySelector('input[type="range"]');
            const values = container.querySelector('.servo-values');

            if (document.activeElement !== slider) {
                slider.min = servo.min;
                slider.max = servo.max;
                slider.value = servo.pos;
            }
            values.innerHTML = `
                <span class="min">${servo.min}</span>
                <span class="pos">${servo.pos}</span>
                <span class="max">${servo.max}</span>
            `;
        });
    }
}

function createServoControl(servo, index) {
    const div = document.createElement('div');
    div.className = 'servo-control';
    div.innerHTML = `
        <div class="servo-header">
            <span class="servo-name">${servo.name}</span>
            <div class="servo-values">
                <span class="min">${servo.min}</span>
                <span class="pos">${servo.pos}</span>
                <span class="max">${servo.max}</span>
            </div>
        </div>
        <div class="servo-slider-container">
            <div class="slider-tooltip"></div>
            <input type="range" min="${servo.min}" max="${servo.max}" value="${servo.pos}">
        </div>
    `;

    const slider = div.querySelector('input[type="range"]');
    const sliderTooltip = div.querySelector('.slider-tooltip');
    const sendPosition = throttle((pos) => {
        send({ type: 'setServo', index: index, position: pos });
    }, 50);

    const updateTooltip = () => {
        const val = parseInt(slider.value);
        const min = parseInt(slider.min);
        const max = parseInt(slider.max);
        const percent = (val - min) / (max - min);
        const sliderWidth = slider.offsetWidth;
        const thumbWidth = 20;
        const left = percent * (sliderWidth - thumbWidth) + thumbWidth / 2;
        sliderTooltip.style.left = left + 'px';
        sliderTooltip.textContent = val + '\u00B0';
    };

    slider.addEventListener('input', (e) => {
        const pos = parseInt(e.target.value);
        div.querySelector('.pos').textContent = pos;
        updateTooltip();
        sendPosition(pos);
    });

    slider.addEventListener('mousedown', () => { updateTooltip(); sliderTooltip.classList.add('visible'); });
    slider.addEventListener('touchstart', () => { updateTooltip(); sliderTooltip.classList.add('visible'); });
    slider.addEventListener('mouseup', () => sliderTooltip.classList.remove('visible'));
    slider.addEventListener('mouseleave', () => sliderTooltip.classList.remove('visible'));
    slider.addEventListener('touchend', () => sliderTooltip.classList.remove('visible'));

    return div;
}
END LEGACY */

// Calibration Cards
// Display order: L X,Y (0,1) → R X,Y (3,4) → L Lid (2) → R Lid (5)
const CALIBRATION_DISPLAY_ORDER = [0, 1, 3, 4, 2, 5];

function updateCalibrationCards() {
    if (!state.servos || state.servos.length === 0) return;

    if (!calibrationCardsEl.querySelector('.calibration-card')) {
        calibrationCardsEl.innerHTML = '';
        CALIBRATION_DISPLAY_ORDER.forEach(index => {
            if (state.servos[index]) {
                calibrationCardsEl.appendChild(createCalibrationCard(state.servos[index], index));
            }
        });
        // Apply admin lock state to newly created cards
        updateProtectedControls();
    } else {
        // Update display values from local calibration state
        localCalibration.forEach((cal, index) => {
            // Find card by data-index since order may differ
            const card = calibrationCardsEl.querySelector(`.calibration-card[data-index="${index}"]`);
            if (!card) return;

            // Update min/max/center displays
            card.querySelector('.min-col .value').textContent = cal.min + '\u00B0';
            card.querySelector('.max-col .value').textContent = cal.max + '\u00B0';
            card.querySelector('.calibration-center .value').textContent = cal.center + '\u00B0';

            // Update center slider
            const slider = card.querySelector('.calibration-center input[type="range"]');
            if (document.activeElement !== slider) {
                slider.min = cal.min;
                slider.max = cal.max;
                slider.value = cal.center;
            }

            // Update test slider bounds and sync value from actual servo position
            const testSlider = card.querySelector('.test-slider');
            const testValue = card.querySelector('.test-value');
            if (testSlider) {
                testSlider.min = cal.min;
                testSlider.max = cal.max;
                // Sync value from actual servo position if not focused
                if (document.activeElement !== testSlider && state.servos && state.servos[index]) {
                    const pos = state.servos[index].pos;
                    // Clamp to slider bounds
                    const clampedPos = Math.max(cal.min, Math.min(cal.max, pos));
                    testSlider.value = clampedPos;
                    if (testValue) testValue.textContent = clampedPos + '\u00B0';
                }
            }

            // Update pin/invert if not focused
            const pinInput = card.querySelector('.pin-input input');
            const invertInput = card.querySelector('.invert-toggle input');
            if (document.activeElement !== pinInput) {
                pinInput.value = cal.pin;
            }
            if (document.activeElement !== invertInput) {
                invertInput.checked = cal.invert;
            }

            // Update dirty state
            updateCalibrationCardDirty(index);
        });
    }
}

// Sync test sliders to actual servo positions (called when entering Calibration tab)
function syncTestSlidersToServoPositions() {
    if (!state.servos || state.servos.length === 0) return;

    state.servos.forEach((servo, index) => {
        const card = calibrationCardsEl?.querySelector(`.calibration-card[data-index="${index}"]`);
        if (!card) return;

        const testSlider = card.querySelector('.test-slider');
        const testValue = card.querySelector('.test-value');
        if (testSlider && testValue) {
            // Clamp to current slider bounds
            const pos = Math.max(
                parseInt(testSlider.min),
                Math.min(parseInt(testSlider.max), servo.pos)
            );
            testSlider.value = pos;
            testValue.textContent = pos + '\u00B0';
        }
    });
}

function updateCalibrationCardDirty(index) {
    const card = calibrationCardsEl.querySelector(`.calibration-card[data-index="${index}"]`);
    if (!card || !savedCalibration[index]) return;

    const cal = localCalibration[index];
    const saved = savedCalibration[index];

    const isDirty = cal.pin !== saved.pin ||
                    cal.min !== saved.min ||
                    cal.center !== saved.center ||
                    cal.max !== saved.max ||
                    cal.invert !== saved.invert;

    card.classList.toggle('dirty', isDirty);
}

function createCalibrationCard(servo, index) {
    const div = document.createElement('div');
    div.className = 'calibration-card';
    div.dataset.index = index;

    const cal = localCalibration[index] || {
        pin: servo.pin,
        min: servo.min,
        center: servo.center,
        max: servo.max,
        invert: servo.invert
    };

    div.innerHTML = `
        <div class="calibration-card-header">
            <span class="servo-name">${servo.name}</span>
            <div class="pin-invert">
                <div class="pin-input">
                    <label>Pin</label>
                    <input type="number" value="${cal.pin}" min="0" max="39">
                </div>
                <div class="invert-toggle">
                    <input type="checkbox" ${cal.invert ? 'checked' : ''}>
                    <label>Inv</label>
                </div>
            </div>
        </div>
        <div class="calibration-controls">
            <div class="calibration-minmax min-col">
                <span class="label">Min</span>
                <span class="value">${cal.min}\u00B0</span>
                <div class="btn-group">
                    <button class="btn-cal" data-action="min-dec">-</button>
                    <button class="btn-cal" data-action="min-inc">+</button>
                </div>
            </div>
            <div class="calibration-center">
                <span class="label">Center</span>
                <span class="value">${cal.center}\u00B0</span>
                <div class="slider-tooltip"></div>
                <input type="range" min="${cal.min}" max="${cal.max}" value="${cal.center}">
            </div>
            <div class="calibration-minmax max-col">
                <span class="label">Max</span>
                <span class="value">${cal.max}\u00B0</span>
                <div class="btn-group">
                    <button class="btn-cal" data-action="max-dec">-</button>
                    <button class="btn-cal" data-action="max-inc">+</button>
                </div>
            </div>
        </div>
        <div class="calibration-test">
            <span class="label">Test: <span class="test-value">${cal.center}\u00B0</span></span>
            <div class="slider-with-tooltip">
                <div class="slider-tooltip"></div>
                <input type="range" class="test-slider" min="${cal.min}" max="${cal.max}" value="${cal.center}">
            </div>
        </div>
    `;

    // Pin change
    div.querySelector('.pin-input input').addEventListener('change', (e) => {
        localCalibration[index].pin = parseInt(e.target.value) || 0;
    });

    // Invert change - send immediately for safety (servo moves to center)
    div.querySelector('.invert-toggle input').addEventListener('change', (e) => {
        const newInvert = e.target.checked;
        localCalibration[index].invert = newInvert;
        send({ type: 'setInvert', index: index, invert: newInvert });
    });

    // Min/max buttons
    div.querySelectorAll('.btn-cal').forEach(btn => {
        btn.addEventListener('click', () => {
            const action = btn.dataset.action;
            switch (action) {
                case 'min-dec': adjustMin(index, -1); break;
                case 'min-inc': adjustMin(index, 1); break;
                case 'max-dec': adjustMax(index, -1); break;
                case 'max-inc': adjustMax(index, 1); break;
            }
        });
    });

    // Center slider with live preview and drag tooltip
    const centerSlider = div.querySelector('.calibration-center input[type="range"]');
    const sliderTooltip = div.querySelector('.slider-tooltip');
    const throttledPreview = throttle((pos) => {
        send({ type: 'previewCalibration', index: index, position: pos });
    }, 50);

    // Position tooltip above slider thumb
    const updateTooltip = (slider, tooltip) => {
        const val = parseInt(slider.value);
        const min = parseInt(slider.min);
        const max = parseInt(slider.max);
        const percent = (val - min) / (max - min);
        const sliderWidth = slider.offsetWidth;
        const thumbWidth = 20; // Match CSS thumb width
        const left = percent * (sliderWidth - thumbWidth) + thumbWidth / 2;
        tooltip.style.left = left + 'px';
        tooltip.textContent = val + '\u00B0';
    };

    centerSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        localCalibration[index].center = val;
        div.querySelector('.calibration-center .value').textContent = val + '\u00B0';
        updateTooltip(centerSlider, sliderTooltip);
        throttledPreview(val);
    });

    // Show tooltip on drag start
    centerSlider.addEventListener('mousedown', () => {
        updateTooltip(centerSlider, sliderTooltip);
        sliderTooltip.classList.add('visible');
    });
    centerSlider.addEventListener('touchstart', () => {
        updateTooltip(centerSlider, sliderTooltip);
        sliderTooltip.classList.add('visible');
    });

    // Hide tooltip on drag end
    centerSlider.addEventListener('mouseup', () => sliderTooltip.classList.remove('visible'));
    centerSlider.addEventListener('mouseleave', () => sliderTooltip.classList.remove('visible'));
    centerSlider.addEventListener('touchend', () => sliderTooltip.classList.remove('visible'));

    // Test slider with live preview and drag tooltip
    const testSlider = div.querySelector('.test-slider');
    const testTooltip = div.querySelector('.calibration-test .slider-tooltip');
    const testValue = div.querySelector('.test-value');
    const throttledTestPreview = throttle((pos) => {
        send({ type: 'previewCalibration', index: index, position: pos });
    }, 50);

    testSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        testValue.textContent = val + '\u00B0';
        updateTooltip(testSlider, testTooltip);
        throttledTestPreview(val);
    });

    // Show tooltip on drag start
    testSlider.addEventListener('mousedown', () => {
        updateTooltip(testSlider, testTooltip);
        testTooltip.classList.add('visible');
    });
    testSlider.addEventListener('touchstart', () => {
        updateTooltip(testSlider, testTooltip);
        testTooltip.classList.add('visible');
    });

    // Hide tooltip on drag end
    testSlider.addEventListener('mouseup', () => testTooltip.classList.remove('visible'));
    testSlider.addEventListener('mouseleave', () => testTooltip.classList.remove('visible'));
    testSlider.addEventListener('touchend', () => testTooltip.classList.remove('visible'));

    return div;
}

function adjustMin(index, delta) {
    const cal = localCalibration[index];
    let newMin = cal.min + delta;

    // Clamp: 0 <= min <= center (no push/pull)
    newMin = Math.max(0, Math.min(newMin, cal.center));

    cal.min = newMin;
    updateCalibrationCards();

    // Live preview
    send({ type: 'previewCalibration', index: index, position: newMin });
}

function adjustMax(index, delta) {
    const cal = localCalibration[index];
    let newMax = cal.max + delta;

    // Clamp: center <= max <= 180 (no push/pull)
    newMax = Math.max(cal.center, Math.min(newMax, 180));

    cal.max = newMax;
    updateCalibrationCards();

    // Live preview
    send({ type: 'previewCalibration', index: index, position: newMax });
}

function saveCalibration() {
    if (isAdminLocked()) return;

    const servos = localCalibration.map((cal, index) => ({
        index: index,
        pin: cal.pin,
        min: cal.min,
        center: cal.center,
        max: cal.max,
        invert: cal.invert
    }));

    send({ type: 'saveAllCalibration', servos: servos });

    // Update saved state to clear dirty indicators
    savedCalibration = JSON.parse(JSON.stringify(localCalibration));

    // Update dirty states on all cards
    localCalibration.forEach((_, index) => updateCalibrationCardDirty(index));

    showToast('Calibration saved', 'success');
}

// Check if there's a text selection within the WiFi status grid
function hasSelectionInWifiStatus() {
    const selection = window.getSelection();
    if (!selection || selection.isCollapsed) return false;

    const statusGrid = document.getElementById('wifiStatus');
    if (!statusGrid) return false;

    // Check if selection is within the status grid
    const range = selection.getRangeAt(0);
    return statusGrid.contains(range.commonAncestorContainer);
}

// WiFi Status
function updateWifiStatus() {
    if (!state.wifi) return;

    const connected = state.wifi.connected;
    const reconnecting = state.wifi.reconnecting;
    const apActive = state.wifi.apActive;
    const attempt = state.wifi.reconnectAttempt || 0;

    // Skip status grid updates if user has text selected (for copy)
    const skipStatusUpdates = hasSelectionInWifiStatus();

    if (!skipStatusUpdates) {
        // WiFi Name
        const wifiNameEl = document.getElementById('wifiStatusName');
        const wifiIpEl = document.getElementById('wifiStatusIp');
        if (connected) {
            wifiNameEl.textContent = state.wifi.ssid || '-';
            wifiNameEl.className = 'status-value connected';
            wifiIpEl.textContent = state.wifi.ip || '-';
        } else if (reconnecting) {
            wifiNameEl.textContent = `${state.wifi.ssid} (reconnecting...)`;
            wifiNameEl.className = 'status-value';
            wifiIpEl.textContent = '-';
        } else {
            wifiNameEl.textContent = 'Not connected';
            wifiNameEl.className = 'status-value disconnected';
            wifiIpEl.textContent = '-';
        }

        // AP Name
        const apNameEl = document.getElementById('apStatusName');
        const apIpEl = document.getElementById('apStatusIp');
        if (apActive) {
            apNameEl.textContent = state.wifi.apName || '-';
            apNameEl.className = 'status-value connected';
            apIpEl.textContent = state.wifi.apIp || '-';
        } else {
            apNameEl.textContent = 'Not active';
            apNameEl.className = 'status-value disconnected';
            apIpEl.textContent = '-';
        }

        // mDNS
        const mdnsRow = document.getElementById('mdnsStatusRow');
        const mdnsNameEl = document.getElementById('mdnsStatusName');
        if (state.wifi.mdnsActive && state.wifi.mdnsHostname) {
            mdnsRow.hidden = false;
            mdnsNameEl.textContent = state.wifi.mdnsHostname + '.local';
        } else {
            mdnsRow.hidden = true;
        }
    }

    // Update connection status in header (consolidated with lock status)
    updateLockStatusIndicator();

    // Update WiFi section header status indicators
    updateWifiSectionHeaders();

    // Update AP name hint
    updateApNameHint();

    // Update mDNS full name hint
    updateMdnsFullNameHint();
}

function updateWifiSectionHeaders() {
    const primaryStatus = document.getElementById('wifiPrimaryStatus');
    const secondaryStatus = document.getElementById('wifiSecondaryStatus');

    // Check which network we're connected to
    const connected = state.wifi && state.wifi.connected;
    const ssid = state.wifi && state.wifi.ssid;

    // Primary network
    const primarySsid = document.getElementById('wifiPrimarySsid')?.value;
    const primaryConfigured = config.networks.find(n => n.index === 0)?.configured;
    if (connected && primarySsid && ssid === primarySsid) {
        primaryStatus.textContent = 'connected';
        primaryStatus.className = 'section-status connected';
        primaryStatus.hidden = false;
    } else if (!primaryConfigured) {
        primaryStatus.textContent = 'unconfigured';
        primaryStatus.className = 'section-status unconfigured';
        primaryStatus.hidden = false;
    } else {
        primaryStatus.hidden = true;
    }

    // Secondary network
    const secondarySsid = document.getElementById('wifiSecondarySsid')?.value;
    const secondaryConfigured = config.networks.find(n => n.index === 1)?.configured;
    if (connected && secondarySsid && ssid === secondarySsid) {
        secondaryStatus.textContent = 'connected';
        secondaryStatus.className = 'section-status connected';
        secondaryStatus.hidden = false;
    } else if (!secondaryConfigured) {
        secondaryStatus.textContent = 'unconfigured';
        secondaryStatus.className = 'section-status unconfigured';
        secondaryStatus.hidden = false;
    } else {
        secondaryStatus.hidden = true;
    }
}

function updateApNameHint() {
    const apPrefix = document.getElementById('apPrefix')?.value || config.ap.ssidPrefix || 'LookIntoMyEyes';
    const deviceId = state.system?.deviceId || 'XXXXXX';
    const hint = document.getElementById('apNameHint');
    if (hint) {
        hint.textContent = `Full AP name: ${apPrefix}-${deviceId}`;
    }
}

function updateMdnsFullNameHint() {
    const hostname = document.getElementById('mdnsHostname')?.value || config.mdns.hostname || 'animatronic-eyes';
    const deviceId = state.system?.deviceId || 'XXXXXX';
    const hint = document.getElementById('mdnsFullName');
    if (hint) {
        hint.textContent = `Full hostname: ${hostname}-${deviceId}.local`;
    }
}

// Event Listeners
function setupEventListeners() {
    // About icon - toggle about section in config tab
    document.getElementById('aboutIcon')?.addEventListener('click', () => {
        const aboutSection = document.querySelector('.config-section[data-section="about"]');
        const configTab = document.getElementById('configurationTab');
        const isConfigActive = configTab?.classList.contains('active');
        const isAboutExpanded = aboutSection && !aboutSection.classList.contains('collapsed');

        // If already on config tab with about expanded, collapse about and expand wifi status
        if (isConfigActive && isAboutExpanded) {
            aboutSection.classList.add('collapsed');
            const wifiStatusSection = document.querySelector('.config-section[data-section="wifiStatus"]');
            if (wifiStatusSection) wifiStatusSection.classList.remove('collapsed');
            return;
        }

        // Switch to configuration tab
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        document.querySelector('.tab[data-tab="configuration"]').classList.add('active');
        configTab.classList.add('active');

        // Collapse all sections, then expand about
        if (aboutSection) {
            document.querySelectorAll('#configurationTab .config-section').forEach(s => {
                s.classList.add('collapsed');
            });
            aboutSection.classList.remove('collapsed');
            // Scroll into view after a short delay to allow expansion
            setTimeout(() => {
                aboutSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
            }, 100);
        }
    });

    // Connection status click - show PIN modal or lock menu
    connectionStatusEl?.addEventListener('click', () => {
        if (!adminState.pinConfigured) return;  // No PIN = no lock functionality

        if (adminState.locked) {
            showPinModal();
        } else {
            // Show lock option (simple confirm for now)
            if (confirm('Lock admin access now?\n\nYou will need to enter your PIN to make changes.')) {
                adminLock();
            }
        }
    });

    // Center all servos (legacy - element may not exist in v0.5+ UI)
    document.getElementById('centerAll')?.addEventListener('click', () => {
        send({ type: 'centerAll' });
    });

    // Center all servos (Calibration tab)
    document.getElementById('centerAllCalib')?.addEventListener('click', () => {
        send({ type: 'centerAll' });
    });

    // Save calibration
    document.getElementById('saveCalibration').addEventListener('click', () => {
        saveCalibration();
    });

    // Reset calibration to factory defaults
    document.getElementById('resetCalibration')?.addEventListener('click', () => {
        if (confirm('Reset all servos to factory default calibration?\n\nThis will set Min/Center/Max to 89/90/91 and clear all invert settings.\n\nChanges are not saved until you click Save All.')) {
            // Reset local calibration to defaults
            localCalibration = localCalibration.map((cal, index) => ({
                pin: cal.pin,  // Keep existing pin
                min: 89,
                center: 90,
                max: 91,
                invert: false
            }));

            // Update all calibration cards to show new values
            updateCalibrationCards();

            // Mark all as dirty
            localCalibration.forEach((_, index) => {
                const card = document.querySelector(`.calibration-card[data-index="${index}"]`);
                if (card) card.classList.add('dirty');
            });

            showToast('Calibration reset to defaults (not saved yet)', 'info');
        }
    });

    // Admin PIN buttons
    document.getElementById('setPinBtn')?.addEventListener('click', setAdminPin);
    document.getElementById('clearPinBtn')?.addEventListener('click', clearAdminPin);

    // Update indicator click - opens GitHub releases
    updateIndicatorEl?.addEventListener('click', () => {
        window.open('https://github.com/Zappo-II/animatronic-eyes/releases', '_blank');
    });

    // Update check button
    document.getElementById('checkUpdateBtn')?.addEventListener('click', () => {
        send({ type: 'checkForUpdate' });
        showToast('Checking for updates...', 'info');
    });

    // Update check enabled toggle
    document.getElementById('updateCheckEnabled')?.addEventListener('change', (e) => {
        send({ type: 'setUpdateCheckEnabled', enabled: e.target.checked });
    });

    // Update check interval select
    document.getElementById('updateCheckInterval')?.addEventListener('change', (e) => {
        send({ type: 'setUpdateCheckInterval', interval: parseInt(e.target.value) });
    });

    // Reset connection with loading state
    document.getElementById('resetConnection').addEventListener('click', (e) => {
        if (confirm('Reset connection and reconnect to WiFi?')) {
            const btn = e.currentTarget;
            btn.classList.add('loading');
            send({ type: 'resetConnection' });
            setTimeout(() => {
                btn.classList.remove('loading');
                showToast('Connection reset initiated', 'success');
            }, 1000);
        }
    });

    // Firmware upload
    document.getElementById('uploadFirmware').addEventListener('click', () => {
        uploadFile(document.getElementById('firmwareFile'), '/update');
    });

    // UI upload
    document.getElementById('uploadUI').addEventListener('click', () => {
        uploadFile(document.getElementById('uiFile'), '/api/upload-ui');
    });

    // Reboot
    document.getElementById('reboot').addEventListener('click', () => {
        if (confirm('Reboot device?')) {
            send({ type: 'reboot' });
            showToast('Device rebooting...', 'info');
            setTimeout(() => location.reload(), 3000);
        }
    });

    // Factory reset
    document.getElementById('factoryReset').addEventListener('click', () => {
        if (confirm('Factory reset will erase all settings. Continue?')) {
            if (confirm('Are you really sure?')) {
                send({ type: 'factoryReset' });
                showToast('Factory reset complete. Device rebooting...', 'info');
                setTimeout(() => location.reload(), 3000);
            }
        }
    });

    // Wipe UI
    document.getElementById('wipeUI')?.addEventListener('click', async () => {
        if (!confirm('Wipe all UI files? You will need to upload a new UI image.')) return;
        try {
            const response = await fetch('/api/wipe-ui', { method: 'POST' });
            if (response.ok) {
                showToast('UI files wiped. Redirecting to recovery...', 'info');
                setTimeout(() => location.href = '/recovery', 2000);
            } else {
                showToast('Wipe failed: ' + await response.text(), 'error');
            }
        } catch (e) {
            showToast('Wipe request failed', 'error');
        }
    });

    // Update badges when checkboxes change
    document.getElementById('keepAP')?.addEventListener('change', updateApSectionBadge);
    document.getElementById('mdnsEnabled')?.addEventListener('change', updateMdnsSectionBadge);
    document.getElementById('ledEnabled')?.addEventListener('change', updateLedSectionBadge);

    // Update brightness display when slider changes (with tooltip)
    const brightnessSlider = document.getElementById('ledBrightness');
    const brightnessTooltip = brightnessSlider?.parentElement.querySelector('.slider-tooltip');

    const updateBrightnessTooltip = () => {
        if (!brightnessSlider || !brightnessTooltip) return;
        const val = parseInt(brightnessSlider.value);
        const min = parseInt(brightnessSlider.min);
        const max = parseInt(brightnessSlider.max);
        const percent = (val - min) / (max - min);
        const sliderWidth = brightnessSlider.offsetWidth;
        const thumbWidth = 20;
        const left = percent * (sliderWidth - thumbWidth) + thumbWidth / 2;
        brightnessTooltip.style.left = left + 'px';
        brightnessTooltip.textContent = val;
    };

    brightnessSlider?.addEventListener('input', (e) => {
        document.getElementById('ledBrightnessValue').textContent = e.target.value;
        updateBrightnessTooltip();
    });

    brightnessSlider?.addEventListener('mousedown', () => { updateBrightnessTooltip(); brightnessTooltip?.classList.add('visible'); });
    brightnessSlider?.addEventListener('touchstart', () => { updateBrightnessTooltip(); brightnessTooltip?.classList.add('visible'); });
    brightnessSlider?.addEventListener('mouseup', () => brightnessTooltip?.classList.remove('visible'));
    brightnessSlider?.addEventListener('mouseleave', () => brightnessTooltip?.classList.remove('visible'));
    brightnessSlider?.addEventListener('touchend', () => brightnessTooltip?.classList.remove('visible'));

    // Update hints when inputs change
    document.getElementById('apPrefix')?.addEventListener('input', updateApNameHint);
    document.getElementById('mdnsHostname')?.addEventListener('input', updateMdnsFullNameHint);

    // Mode selector in Control tab
    document.getElementById('modeSelect')?.addEventListener('change', (e) => {
        const mode = e.target.value;
        // Lock to prevent state broadcast from reverting dropdown before server processes
        modeSelectorLockedUntil = Date.now() + 1000;  // 1 second lock
        // Send the mode name directly - server handles follow vs auto
        send({ type: 'setMode', mode: mode });
    });

    // Backup download button
    document.getElementById('downloadBackup')?.addEventListener('click', downloadBackup);

    // Restore backup button (triggers file input)
    document.getElementById('restoreBackupBtn')?.addEventListener('click', () => {
        document.getElementById('restoreFile')?.click();
    });

    // Restore file input change handler
    document.getElementById('restoreFile')?.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (file) {
            restoreBackup(file);
        }
        // Reset input so same file can be selected again
        e.target.value = '';
    });
}

// Helper Functions
function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(obj));
    }
}

async function fetchVersion() {
    try {
        const res = await fetch('/api/version');
        const data = await res.json();

        let html = '<div class="system-info-row"><span>Firmware</span><span>' + data.version + '</span></div>';
        html += '<div class="system-info-row"><span>Min UI Required</span><span>' + (data.minUiVersion || '-') + '</span></div>';

        // UI Version with status indicator
        let uiStatusColor = '#7fbf7f'; // green
        let uiStatusText = '';
        switch (data.uiStatus) {
            case 'ok':
                uiStatusText = '';
                break;
            case 'minor_mismatch':
                uiStatusColor = '#f39c12';
                uiStatusText = ' (minor mismatch)';
                break;
            case 'major_mismatch':
                uiStatusColor = '#e94560';
                uiStatusText = ' (major mismatch)';
                break;
            case 'fw_too_old':
                uiStatusColor = '#e94560';
                uiStatusText = ' (firmware too old)';
                break;
            case 'ui_too_old':
                uiStatusColor = '#e94560';
                uiStatusText = ' (UI too old)';
                break;
            case 'missing':
            default:
                uiStatusColor = '#e94560';
                uiStatusText = ' (missing)';
                break;
        }
        html += '<div class="system-info-row"><span>UI Version</span><span>' + (data.uiVersion || 'Unknown');
        if (uiStatusText) {
            html += '<span style="color:' + uiStatusColor + '">' + uiStatusText + '</span>';
        }
        html += '</span></div>';
        html += '<div class="system-info-row"><span>UI Requires Firmware</span><span>' + (data.uiMinFirmware || '-') + '</span></div>';

        html += '<div class="system-info-row"><span>Free Heap</span><span>' + (data.freeHeap / 1024).toFixed(1) + ' KB</span></div>';

        if (data.rebootRequired) {
            html += '<div class="system-info-row" style="color:#f39c12"><span>Status</span><span>Reboot required</span></div>';
        }

        versionInfoEl.innerHTML = html;

        // Populate About section
        const aboutVersions = document.getElementById('aboutVersions');
        const aboutDevice = document.getElementById('aboutDevice');
        if (aboutVersions) {
            aboutVersions.textContent = `v${data.version} (Firmware) / v${data.uiVersion || 'Unknown'} (UI)`;
        }
        if (aboutDevice) {
            const chipInfo = data.chipModel ? `${data.chipModel} (rev ${data.chipRevision})` : 'ESP32';
            const deviceId = data.deviceId || '';
            aboutDevice.textContent = `Running on ${chipInfo}${deviceId ? ' - ' + deviceId : ''}`;
        }
    } catch (e) {
        versionInfoEl.innerHTML = '<div class="system-info-row"><span>Error</span><span>Could not fetch version info</span></div>';
    }
}

function uploadFile(fileInput, endpoint) {
    const progressContainer = document.getElementById('uploadProgress');
    const progressFill = progressContainer.querySelector('.progress-fill');
    const progressText = progressContainer.querySelector('.progress-text');

    if (!fileInput.files.length) {
        showToast('Please select a file', 'error');
        return;
    }

    const file = fileInput.files[0];
    progressContainer.classList.remove('hidden');
    progressFill.style.width = '0%';
    progressText.textContent = '0%';

    let uploadComplete = false;
    let handled = false;

    const showSuccess = () => {
        if (handled) return;
        handled = true;
        progressText.textContent = 'Upload complete! Rebooting...';
        showToast('Upload complete! Rebooting...', 'success');
        setTimeout(() => location.reload(), 5000);
    };

    const showError = (msg) => {
        if (handled) return;
        handled = true;
        progressText.textContent = 'Upload failed: ' + msg;
        showToast('Upload failed: ' + msg, 'error');
        setTimeout(() => progressContainer.classList.add('hidden'), 3000);
    };

    const xhr = new XMLHttpRequest();
    xhr.open('POST', endpoint);

    xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
            const pct = Math.round((e.loaded / e.total) * 100);
            progressFill.style.width = pct + '%';
            if (pct === 100) {
                uploadComplete = true;
                progressText.textContent = 'Processing...';
            } else {
                progressText.textContent = pct + '%';
            }
        }
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
}

// Console Functions
function addConsoleLine(line, autoScroll = true) {
    const output = document.getElementById('consoleOutput');
    if (!output) return;

    const lineEl = document.createElement('div');
    lineEl.className = 'console-line';

    // Color based on tag
    const tagMatch = line.match(/\[([^\]]+)\]\s*\[([^\]]+)\]/);
    if (tagMatch) {
        const tag = tagMatch[2].toLowerCase();
        if (tag.includes('error') || line.toLowerCase().includes('error')) {
            lineEl.classList.add('log-error');
        } else if (tag.includes('warn') || line.toLowerCase().includes('warning')) {
            lineEl.classList.add('log-warn');
        } else if (tag === 'wifi' || tag === 'wifimanager') {
            lineEl.classList.add('log-wifi');
        } else if (tag === 'ws' || tag === 'websocket') {
            lineEl.classList.add('log-ws');
        } else if (tag === 'ota') {
            lineEl.classList.add('log-ota');
        } else if (tag === 'calibration') {
            lineEl.classList.add('log-calibration');
        } else if (tag === 'control') {
            lineEl.classList.add('log-control');
        }
    }

    lineEl.textContent = line;
    output.appendChild(lineEl);

    // Limit console lines to prevent memory issues
    const maxLines = 500;
    while (output.children.length > maxLines) {
        output.removeChild(output.firstChild);
    }

    // Auto-scroll if enabled
    if (autoScroll && document.getElementById('consoleAutoscroll')?.checked) {
        output.scrollTop = output.scrollHeight;
    }
}

function requestLogHistory() {
    send({ type: 'getLogHistory' });
}

function clearConsole() {
    const output = document.getElementById('consoleOutput');
    if (output) {
        output.innerHTML = '';
    }
}

function setupConsole() {
    // Clear button
    document.getElementById('consoleClear')?.addEventListener('click', clearConsole);
}

// Eye Controller
function setupEyeController() {
    const gazePad = document.getElementById('gazePad');
    const gazeIndicator = document.getElementById('gazeIndicator');
    const gazeZSlider = document.getElementById('gazeZ');
    const lidLeftSlider = document.getElementById('lidLeft');
    const lidRightSlider = document.getElementById('lidRight');
    const couplingSlider = document.getElementById('coupling');

    if (!gazePad) return;  // Eye controller UI not present

    // Throttled gaze send function
    const throttledGaze = throttle((x, y, z) => {
        send({ type: 'setGaze', x: x, y: y, z: z });
    }, 50);

    // Throttled lids send function
    const throttledLids = throttle((left, right) => {
        send({ type: 'setLids', left: left, right: right });
    }, 50);

    // Gaze pad helpers
    function getGazeFromEvent(e, rect) {
        const clientX = e.touches ? e.touches[0].clientX : e.clientX;
        const clientY = e.touches ? e.touches[0].clientY : e.clientY;

        // Calculate position relative to pad center (-100 to +100)
        const x = ((clientX - rect.left) / rect.width - 0.5) * 200;
        const y = -((clientY - rect.top) / rect.height - 0.5) * 200;  // Invert Y (up = positive)

        return {
            x: Math.max(-100, Math.min(100, Math.round(x))),
            y: Math.max(-100, Math.min(100, Math.round(y)))
        };
    }

    function updateGazePadUI(x, y) {
        // Position indicator (convert -100..+100 to 0%..100%)
        const left = (x + 100) / 2;
        const top = (100 - y) / 2;  // Invert Y for CSS (top = 0)
        gazeIndicator.style.left = left + '%';
        gazeIndicator.style.top = top + '%';

        // Update value displays
        document.getElementById('gazeXValue').textContent = x;
        document.getElementById('gazeYValue').textContent = y;
    }

    // Helper to lock controls during/after interaction
    function lockControls() {
        controlsLockedUntil = Date.now() + CONTROL_LOCK_MS;
    }

    function areControlsLocked() {
        return Date.now() < controlsLockedUntil;
    }

    // Mode player pause/resume for manual control (auto modes)
    let modePlayerPaused = false;
    let modePlayerResumeTimeout = null;

    function pauseModePlayerForControl() {
        // Clear any pending resume
        if (modePlayerResumeTimeout) {
            clearTimeout(modePlayerResumeTimeout);
            modePlayerResumeTimeout = null;
        }
        // Pause if not already paused
        if (!modePlayerPaused) {
            modePlayerPaused = true;
            send({ type: 'pauseModePlayer', paused: true });
        }
    }

    function resumeModePlayerDelayed() {
        // Resume after a delay (allows user to continue interacting without rapid pause/resume)
        if (modePlayerResumeTimeout) {
            clearTimeout(modePlayerResumeTimeout);
        }
        modePlayerResumeTimeout = setTimeout(() => {
            if (modePlayerPaused && !gazePadActive && !lidSlidersActive) {
                modePlayerPaused = false;
                send({ type: 'pauseModePlayer', paused: false });
            }
            modePlayerResumeTimeout = null;
        }, 500);  // Resume 500ms after last interaction
    }

    // Gaze pad event handlers
    function handleGazePadStart(e) {
        e.preventDefault();
        gazePadActive = true;
        lockControls();
        pauseModePlayerForControl();
        const rect = gazePad.getBoundingClientRect();
        const gaze = getGazeFromEvent(e, rect);
        updateGazePadUI(gaze.x, gaze.y);
        const z = parseInt(gazeZSlider.value);
        throttledGaze(gaze.x, gaze.y, z);
    }

    function handleGazePadMove(e) {
        if (!gazePadActive) return;
        e.preventDefault();
        lockControls();
        const rect = gazePad.getBoundingClientRect();
        const gaze = getGazeFromEvent(e, rect);
        updateGazePadUI(gaze.x, gaze.y);
        const z = parseInt(gazeZSlider.value);
        throttledGaze(gaze.x, gaze.y, z);
    }

    function handleGazePadEnd() {
        gazePadActive = false;
        lockControls();  // Extend lock after release to allow throttled command to complete
        resumeModePlayerDelayed();
    }

    // Gaze pad touch/mouse events
    gazePad.addEventListener('mousedown', handleGazePadStart);
    gazePad.addEventListener('touchstart', handleGazePadStart, { passive: false });
    document.addEventListener('mousemove', handleGazePadMove);
    document.addEventListener('touchmove', handleGazePadMove, { passive: false });
    document.addEventListener('mouseup', handleGazePadEnd);
    document.addEventListener('touchend', handleGazePadEnd);

    // Z (depth) slider
    gazeZSlider?.addEventListener('mousedown', pauseModePlayerForControl);
    gazeZSlider?.addEventListener('touchstart', pauseModePlayerForControl, { passive: true });
    gazeZSlider?.addEventListener('mouseup', resumeModePlayerDelayed);
    gazeZSlider?.addEventListener('touchend', resumeModePlayerDelayed);
    gazeZSlider?.addEventListener('input', (e) => {
        lockControls();
        const z = parseInt(e.target.value);
        document.getElementById('gazeZValue').textContent = z;
        // Get current X/Y from UI
        const x = parseInt(document.getElementById('gazeXValue').textContent) || 0;
        const y = parseInt(document.getElementById('gazeYValue').textContent) || 0;
        throttledGaze(x, y, z);
    });

    // Lid sliders with link support
    const lidLinkCheckbox = document.getElementById('lidLink');
    const lidLinkLabel = document.getElementById('lidLinkLabel');

    // Update tooltip based on link state
    const updateLinkTooltip = () => {
        if (lidLinkLabel) {
            lidLinkLabel.title = lidLinkCheckbox?.checked ? 'Unlink lids' : 'Link lids';
        }
    };
    lidLinkCheckbox?.addEventListener('change', updateLinkTooltip);

    // Lid slider interaction tracking (mousedown/touchstart to mouseup/touchend)
    const handleLidSliderStart = () => {
        lidSlidersActive = true;
        lockControls();
        pauseModePlayerForControl();
    };
    const handleLidSliderEnd = () => {
        lidSlidersActive = false;
        lockControls();  // Extend lock after release
        resumeModePlayerDelayed();
    };

    [lidLeftSlider, lidRightSlider].forEach(slider => {
        if (!slider) return;
        slider.addEventListener('mousedown', handleLidSliderStart);
        slider.addEventListener('touchstart', handleLidSliderStart, { passive: true });
        slider.addEventListener('mouseup', handleLidSliderEnd);
        slider.addEventListener('touchend', handleLidSliderEnd);
    });

    lidLeftSlider?.addEventListener('input', (e) => {
        lockControls();
        const left = parseInt(e.target.value);
        let right = parseInt(lidRightSlider.value);

        // If linked, sync right slider to left
        if (lidLinkCheckbox?.checked) {
            right = left;
            lidRightSlider.value = right;
            document.getElementById('lidRightValue').textContent = right;
        }

        document.getElementById('lidLeftValue').textContent = left;
        throttledLids(left, right);
    });

    lidRightSlider?.addEventListener('input', (e) => {
        lockControls();
        const right = parseInt(e.target.value);
        let left = parseInt(lidLeftSlider.value);

        // If linked, sync left slider to right
        if (lidLinkCheckbox?.checked) {
            left = right;
            lidLeftSlider.value = left;
            document.getElementById('lidLeftValue').textContent = left;
        }

        document.getElementById('lidRightValue').textContent = right;
        throttledLids(left, right);
    });

    // Coupling slider
    couplingSlider?.addEventListener('mousedown', pauseModePlayerForControl);
    couplingSlider?.addEventListener('touchstart', pauseModePlayerForControl, { passive: true });
    couplingSlider?.addEventListener('mouseup', resumeModePlayerDelayed);
    couplingSlider?.addEventListener('touchend', resumeModePlayerDelayed);
    couplingSlider?.addEventListener('input', (e) => {
        lockControls();
        const rawValue = parseInt(e.target.value);  // -100 to +100
        const coupling = rawValue / 100;  // Convert to -1.0 to +1.0
        document.getElementById('couplingValue').textContent = coupling.toFixed(1);
        send({ type: 'setCoupling', value: coupling });
    });

    // Center button
    document.getElementById('centerEyes')?.addEventListener('click', () => {
        send({ type: 'centerEyes' });
    });

    // Animate blink from current lid position (not fixed keyframes)
    // Sets blinkAnimationActive flag to prevent WS state from fighting with animation
    function animateBlinkPreview(eyeElement, lidValue, duration = 200) {
        blinkAnimationActive = true;
        const currentScale = Math.max(0, Math.min(1, (100 - lidValue) / 200));
        const eyelids = eyeElement.querySelectorAll('.eyelid');
        eyelids.forEach(lid => {
            const animation = lid.animate([
                { transform: `scaleY(${currentScale})` },
                { transform: 'scaleY(1)', offset: 0.35 },
                { transform: 'scaleY(1)', offset: 0.5 },
                { transform: `scaleY(${currentScale})` }
            ], { duration: duration, easing: 'ease-out' });
            // Clear flag when animation completes
            animation.onfinish = () => {
                blinkAnimationActive = false;
            };
        });
        // Safety timeout in case animation doesn't fire onfinish
        setTimeout(() => { blinkAnimationActive = false; }, duration + 50);
    }

    // Blink button (both eyes)
    document.getElementById('blinkBtn')?.addEventListener('click', () => {
        send({ type: 'blink', duration: 200 });
        // Trigger preview animation from current lid positions
        const leftEye = document.querySelector('.eye-preview .left-eye');
        const rightEye = document.querySelector('.eye-preview .right-eye');
        const leftLid = state.eye?.lidLeft ?? 0;
        const rightLid = state.eye?.lidRight ?? 0;
        if (leftEye) animateBlinkPreview(leftEye, leftLid, 200);
        if (rightEye) animateBlinkPreview(rightEye, rightLid, 200);
    });

    // Wink left eye only
    document.getElementById('blinkLeftBtn')?.addEventListener('click', () => {
        send({ type: 'blinkLeft', duration: 200 });
        const leftEye = document.querySelector('.eye-preview .left-eye');
        const leftLid = state.eye?.lidLeft ?? 0;
        if (leftEye) animateBlinkPreview(leftEye, leftLid, 200);
    });

    // Wink right eye only
    document.getElementById('blinkRightBtn')?.addEventListener('click', () => {
        send({ type: 'blinkRight', duration: 200 });
        const rightEye = document.querySelector('.eye-preview .right-eye');
        const rightLid = state.eye?.lidRight ?? 0;
        if (rightEye) animateBlinkPreview(rightEye, rightLid, 200);
    });

    // Auto-blink toggle (runtime override, doesn't save to config)
    document.getElementById('autoBlinkToggle')?.addEventListener('click', () => {
        const btn = document.getElementById('autoBlinkToggle');
        const isCurrentlyActive = btn.classList.contains('active');
        // Toggle: if active, turn off; if inactive, turn on
        send({ type: 'setAutoBlinkOverride', enabled: !isCurrentlyActive });
    });

    // Impulse button - trigger random impulse
    document.getElementById('impulseBtn')?.addEventListener('click', () => {
        send({ type: 'triggerImpulse' });
    });

    // Auto-impulse toggle (runtime override, doesn't save to config)
    document.getElementById('autoImpulseToggle')?.addEventListener('click', () => {
        const btn = document.getElementById('autoImpulseToggle');
        const isCurrentlyActive = btn.classList.contains('active');
        // Toggle: if active, turn off; if inactive, turn on
        send({ type: 'setAutoImpulseOverride', enabled: !isCurrentlyActive });
    });

    // Add slider tooltip behavior for Z, Lid, and Coupling sliders
    [gazeZSlider, lidLeftSlider, lidRightSlider, couplingSlider].forEach(slider => {
        if (!slider) return;
        const tooltip = slider.parentElement?.querySelector('.slider-tooltip');
        if (!tooltip) return;

        const isCoupling = slider === couplingSlider;

        const updateTooltip = () => {
            const val = parseInt(slider.value);
            const min = parseInt(slider.min);
            const max = parseInt(slider.max);
            const percent = (val - min) / (max - min);
            const sliderWidth = slider.offsetWidth;
            const thumbWidth = 20;
            const left = percent * (sliderWidth - thumbWidth) + thumbWidth / 2;
            tooltip.style.left = left + 'px';
            // Coupling shows -1.0 to +1.0, others show raw value
            tooltip.textContent = isCoupling ? (val / 100).toFixed(1) : val;
        };

        slider.addEventListener('input', updateTooltip);
        slider.addEventListener('mousedown', () => { updateTooltip(); tooltip.classList.add('visible'); });
        slider.addEventListener('touchstart', () => { updateTooltip(); tooltip.classList.add('visible'); });
        slider.addEventListener('mouseup', () => tooltip.classList.remove('visible'));
        slider.addEventListener('mouseleave', () => tooltip.classList.remove('visible'));
        slider.addEventListener('touchend', () => tooltip.classList.remove('visible'));
    });
}

function updateEyeController() {
    if (!state.eye || Object.keys(state.eye).length === 0) return;

    const eye = state.eye;

    // Check if controls are locked (user recently interacted)
    const isLocked = Date.now() < controlsLockedUntil;

    // Don't update gaze UI while user is actively dragging or controls are locked
    if (!gazePadActive && !isLocked) {
        // Update gaze indicator position
        const gazeIndicator = document.getElementById('gazeIndicator');
        if (gazeIndicator) {
            const left = (eye.gazeX + 100) / 2;
            const top = (100 - eye.gazeY) / 2;
            gazeIndicator.style.left = left + '%';
            gazeIndicator.style.top = top + '%';
        }

        // Update gaze value displays
        const gazeXValue = document.getElementById('gazeXValue');
        const gazeYValue = document.getElementById('gazeYValue');
        if (gazeXValue) gazeXValue.textContent = Math.round(eye.gazeX);
        if (gazeYValue) gazeYValue.textContent = Math.round(eye.gazeY);
    }

    // Update Z slider if not locked
    const gazeZSlider = document.getElementById('gazeZ');
    if (gazeZSlider && !isLocked) {
        gazeZSlider.value = Math.round(eye.gazeZ);
        document.getElementById('gazeZValue').textContent = Math.round(eye.gazeZ);
    }

    // Update lid sliders if not actively sliding and not locked
    // (lidSlidersActive + isLocked are sufficient - no need for activeElement check)
    const lidLeftSlider = document.getElementById('lidLeft');
    const lidRightSlider = document.getElementById('lidRight');
    if (!lidSlidersActive && !isLocked) {
        if (lidLeftSlider) {
            lidLeftSlider.value = Math.round(eye.lidLeft);
            document.getElementById('lidLeftValue').textContent = Math.round(eye.lidLeft);
        }
        if (lidRightSlider) {
            lidRightSlider.value = Math.round(eye.lidRight);
            document.getElementById('lidRightValue').textContent = Math.round(eye.lidRight);
        }
    }

    // Update coupling slider if not locked
    const couplingSlider = document.getElementById('coupling');
    if (couplingSlider && !isLocked) {
        couplingSlider.value = Math.round(eye.coupling * 100);
        document.getElementById('couplingValue').textContent = eye.coupling.toFixed(1);
    }

    // Update eye preview (always update - this is visual feedback only)
    updateEyePreview(eye);
}

function updateEyePreview(eye) {
    const preview = document.querySelector('.eye-preview');
    if (!preview) return;

    // Calculate vergence (replicate firmware logic)
    // Use ?? instead of || so that 0 values aren't treated as falsy
    const maxVergence = eye.maxVergence ?? 100;  // Firmware default
    const coupling = eye.coupling ?? 1;           // Firmware default (linked with vergence)
    const gazeZ = eye.gazeZ ?? 0;
    const gazeX = eye.gazeX ?? 0;
    const gazeY = eye.gazeY ?? 0;
    const mirrorPreview = eye.mirrorPreview ?? false;

    // Vergence: 0 at z=100 (far), maxVergence at z=-100 (close)
    const vergence = maxVergence * (100 - gazeZ) / 200;

    // Per-eye X positions with vergence
    let leftEyeX = gazeX + vergence * coupling;
    let rightEyeX = gazeX - vergence * coupling;

    // Mirror mode: flip X axis so preview shows eyes from viewer's perspective
    // (physical eyes looking "their left" appears as viewer's right)
    if (mirrorPreview) {
        leftEyeX = -leftEyeX;
        rightEyeX = -rightEyeX;
    }

    // Per-eye Y positions with vertical divergence (Feldman mode when coupling < 0)
    const maxVerticalDivergence = 50;
    const verticalDivergence = (coupling < 0) ? maxVerticalDivergence * (-coupling) : 0;
    const leftEyeY = Math.max(-100, Math.min(100, gazeY + verticalDivergence));
    const rightEyeY = Math.max(-100, Math.min(100, gazeY - verticalDivergence));

    // Update pupils
    // Map gaze (-100 to +100) to pupil offset within eyeball
    // Eyeball is 70px, pupil is 32px, so max movement is ~15px from center
    const maxPupilOffset = 15;

    const leftPupil = preview.querySelector('.left-eye .pupil');
    const rightPupil = preview.querySelector('.right-eye .pupil');

    if (leftPupil) {
        const offsetX = (leftEyeX / 100) * maxPupilOffset;
        const offsetY = -(leftEyeY / 100) * maxPupilOffset; // Invert Y (up = negative in CSS)
        leftPupil.style.transform = `translate(calc(-50% + ${offsetX}px), calc(-50% + ${offsetY}px))`;
    }

    if (rightPupil) {
        const offsetX = (rightEyeX / 100) * maxPupilOffset;
        const offsetY = -(rightEyeY / 100) * maxPupilOffset;
        rightPupil.style.transform = `translate(calc(-50% + ${offsetX}px), calc(-50% + ${offsetY}px))`;
    }

    // Update eyelids (skip during local blink animation to avoid fighting)
    if (!blinkAnimationActive) {
        // Lid value: -100 = closed (scaleY=1), +100 = open (scaleY=0)
        // Map: lidValue -> (100 - lidValue) / 200 for scale (0 to 1)
        const leftLid = eye.lidLeft || 0;
        const rightLid = eye.lidRight || 0;

        const leftLidScale = Math.max(0, Math.min(1, (100 - leftLid) / 200));
        const rightLidScale = Math.max(0, Math.min(1, (100 - rightLid) / 200));

        const leftTopLid = preview.querySelector('.left-eye .eyelid-top');
        const leftBottomLid = preview.querySelector('.left-eye .eyelid-bottom');
        const rightTopLid = preview.querySelector('.right-eye .eyelid-top');
        const rightBottomLid = preview.querySelector('.right-eye .eyelid-bottom');

        if (leftTopLid) leftTopLid.style.transform = `scaleY(${leftLidScale})`;
        if (leftBottomLid) leftBottomLid.style.transform = `scaleY(${leftLidScale})`;
        if (rightTopLid) rightTopLid.style.transform = `scaleY(${rightLidScale})`;
        if (rightBottomLid) rightBottomLid.style.transform = `scaleY(${rightLidScale})`;
    }
}

// Mode System Functions
let modeDropdownsPopulated = false;

function capitalizeFirst(str) {
    return str.charAt(0).toUpperCase() + str.slice(1);
}

function populateModeDropdowns(availableModes) {
    // Only populate once (modes don't change at runtime)
    if (modeDropdownsPopulated) {
        // Just update selection state
        updateModeSelectors();
        return;
    }

    const modeSelect = document.getElementById('modeSelect');
    const defaultModeSelect = document.getElementById('defaultModeSelect');

    if (!modeSelect || !defaultModeSelect) return;

    // Clear existing options except 'follow'
    while (modeSelect.options.length > 1) {
        modeSelect.remove(1);
    }
    while (defaultModeSelect.options.length > 1) {
        defaultModeSelect.remove(1);
    }

    // Add auto modes (skip 'follow' as it's already in HTML)
    availableModes.forEach(mode => {
        // Mode can be a string or object with name/displayName
        const modeName = typeof mode === 'string' ? mode : mode.name;
        const displayName = typeof mode === 'string' ? capitalizeFirst(mode) : (mode.displayName || mode.name);

        // Skip 'follow' - it's already in the static HTML
        if (modeName === 'follow') return;

        // Control tab selector
        const opt1 = document.createElement('option');
        opt1.value = modeName;
        opt1.textContent = displayName;
        modeSelect.appendChild(opt1);

        // Config tab selector
        const opt2 = document.createElement('option');
        opt2.value = modeName;
        opt2.textContent = displayName;
        defaultModeSelect.appendChild(opt2);
    });

    modeDropdownsPopulated = true;

    // Set initial selection from state
    updateModeSelectors();

    // Set default mode from config if available
    if (savedConfig.mode && savedConfig.mode.defaultMode) {
        defaultModeSelect.value = savedConfig.mode.defaultMode;
    }
}

function updateModeSelectors() {
    const modeSelect = document.getElementById('modeSelect');
    if (!modeSelect || !state.mode) return;

    // Update Control tab mode selector to reflect current mode (skip if recently changed by user)
    if (Date.now() >= modeSelectorLockedUntil) {
        if (state.mode.current === 'follow') {
            modeSelect.value = 'follow';
        } else if (state.mode.current) {
            modeSelect.value = state.mode.current;
        }
    }

    // If "Remember" is checked, also update the Config tab's default mode dropdown
    const rememberCheckbox = document.getElementById('rememberLastMode');
    const defaultModeSelect = document.getElementById('defaultModeSelect');
    if (rememberCheckbox?.checked && defaultModeSelect && state.mode.current) {
        defaultModeSelect.value = state.mode.current;
        // Also update savedConfig so dirty state tracking works correctly
        if (savedConfig.mode) {
            savedConfig.mode.defaultMode = state.mode.current;
        }
    }

    // Update auto-blink toggle button state
    const autoBlinkBtn = document.getElementById('autoBlinkToggle');
    if (autoBlinkBtn) {
        const isActive = state.mode.autoBlinkActive;
        autoBlinkBtn.classList.toggle('active', isActive);

        // Disable toggle in Auto modes (auto-blink is mode-controlled)
        const isAutoMode = state.mode.isAuto;
        autoBlinkBtn.disabled = isAutoMode;
        autoBlinkBtn.title = isAutoMode
            ? 'Auto-blink controlled by mode'
            : 'Toggle auto-blink (Follow mode only)';
    }

    // Update impulse UI state
    updateImpulseState();
}

function updateImpulseState() {
    if (!state.impulse) return;

    // Update auto-impulse toggle button state
    const autoImpulseBtn = document.getElementById('autoImpulseToggle');
    if (autoImpulseBtn) {
        const isActive = state.impulse.autoImpulseActive;
        autoImpulseBtn.classList.toggle('active', isActive);

        // Disable toggle in Auto modes (like auto-blink)
        const isAutoMode = state.mode?.isAuto;
        autoImpulseBtn.disabled = isAutoMode;
        autoImpulseBtn.title = isAutoMode
            ? 'Auto-impulse disabled in auto modes'
            : 'Toggle auto-impulse (Follow mode only)';
    }

    // Update impulse button state
    const impulseBtn = document.getElementById('impulseBtn');
    if (impulseBtn) {
        const hasSelection = (state.impulse.impulseSelection || '').trim().length > 0;
        impulseBtn.disabled = !hasSelection || state.impulse.playing;
        impulseBtn.title = !hasSelection
            ? 'No impulses selected'
            : state.impulse.playing
                ? 'Impulse in progress'
                : 'Trigger random impulse';
    }

    // Populate impulse selection checkboxes if available list is present
    // NOTE: Available impulses now received via 'availableImpulses' message on connect
    if (availableImpulsesList.length > 0) {
        const container = document.getElementById('impulseSelection');
        if (container && container.children.length === 0) {
            populateImpulseSelection(availableImpulsesList, savedConfig.impulse?.impulseSelection || '');
            updateProtectedControls();
        }
    }
}

// Backup/Restore Functions
async function downloadBackup() {
    try {
        showToast('Creating backup...', 'info');
        const response = await fetch('/api/backup');
        if (!response.ok) {
            throw new Error(await response.text());
        }

        const backup = await response.json();
        const blob = new Blob([JSON.stringify(backup, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);

        // Create filename with device ID and timestamp
        const deviceId = state.system?.deviceId || 'unknown';
        const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
        const filename = `animatronic-eyes-${deviceId}-${timestamp}.json`;

        // Trigger download
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);

        showToast('Backup downloaded', 'success');
    } catch (e) {
        console.error('Backup failed:', e);
        showToast('Backup failed: ' + e.message, 'error');
    }
}

async function restoreBackup(file) {
    if (!confirm('Restore backup? This will overwrite all current settings.')) {
        return;
    }

    try {
        showToast('Restoring backup...', 'info');

        const text = await file.text();
        const backup = JSON.parse(text);

        // Validate backup structure
        if (!backup.version || !backup.timestamp) {
            throw new Error('Invalid backup file format');
        }

        const response = await fetch('/api/restore', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(backup)
        });

        if (!response.ok) {
            throw new Error(await response.text());
        }

        showToast('Backup restored! Device will reboot...', 'success');

        // Wait a moment then reload
        setTimeout(() => {
            location.reload();
        }, 3000);
    } catch (e) {
        console.error('Restore failed:', e);
        showToast('Restore failed: ' + e.message, 'error');
    }
}
