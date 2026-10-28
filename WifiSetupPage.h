#pragma once

#include <Arduino.h>

static const char WIFI_SETUP_PAGE[] PROGMEM = R"AREDNWIFI(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#173b33">
  <title>AREDN Wi-Fi Setup</title>
  <style>
    :root { color-scheme: light; font-family: "Segoe UI", sans-serif; color: #172722; background: #f3f5ef; }
    body { box-sizing: border-box; max-width: 560px; margin: 0 auto; padding: 24px 16px; }
    main { padding: 24px; border: 1px solid #d7ded7; border-radius: 8px; background: #fff; }
    h1 { margin-top: 0; color: #173b33; }
    p { line-height: 1.5; }
    label { display: block; margin: 18px 0 6px; font-weight: 700; }
    input, select { box-sizing: border-box; width: 100%; min-height: 46px; padding: 10px; border: 1px solid #aebdb3; border-radius: 5px; font: inherit; }
    button { min-height: 46px; margin-top: 20px; padding: 0 18px; border: 0; border-radius: 5px; color: #fff; background: #173b33; font: inherit; font-weight: 700; }
    button:disabled { opacity: 0.65; }
    .password-field { position: relative; }
    .password-field input { padding-right: 52px; }
    .password-toggle { position: absolute; top: 0; right: 0; width: 46px; min-height: 46px; margin: 0; padding: 0; border: 0; color: #173b33; background: transparent; }
    .password-toggle:focus-visible { outline: 2px solid #173b33; outline-offset: -2px; }
    .password-toggle svg { display: block; width: 22px; height: 22px; margin: auto; fill: none; stroke: currentColor; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
    #status { min-height: 24px; margin: 8px 0 14px; color: #5c6e66; }
    #status.error { color: #a2382d; }
    #scan-status { min-height: 24px; color: #5c6e66; font-size: 0.9rem; }
    .hint { margin-top: 6px; color: #5c6e66; font-size: 0.9rem; }
    .current-connection { margin: 20px 0 28px; padding: 18px; border: 1px solid #d7ded7; border-left: 4px solid #173b33; border-radius: 8px; background: #f3f7f3; }
    .current-connection h2 { margin: 0; color: #173b33; font-size: 1.1rem; }
    .connection-details { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 190px), 1fr)); gap: 10px; margin: 0; }
    .connection-details div { min-width: 0; padding: 11px 12px; border: 1px solid #e0e7e0; border-radius: 6px; background: #fff; }
    .connection-details dt { color: #5c6e66; font-size: 0.82rem; font-weight: 700; }
    .connection-details dd { margin: 4px 0 0; color: #172722; font-weight: 700; font-variant-numeric: tabular-nums; overflow-wrap: anywhere; }
    .setup-nav { margin: 0 0 18px; }
    .setup-link {
      display: inline-flex;
      min-height: 38px;
      align-items: center;
      gap: 8px;
      padding: 0 12px;
      border: 1px solid #d7ded7;
      border-radius: 999px;
      color: #173b33;
      background: #e4eee8;
      font-size: 0.9rem;
      font-weight: 700;
      text-decoration: none;
      transition: background-color 140ms ease, border-color 140ms ease;
    }
    .setup-link:hover { border-color: #b8c9be; background: #d8e7dd; }
    .setup-link:focus-visible { outline: 3px solid #c55b36; outline-offset: 2px; }
    .setup-link svg { width: 16px; height: 16px; fill: none; stroke: currentColor; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
  </style>
</head>
<body>
  <main>
    <nav class="setup-nav" aria-label="Setup navigation">
      <a class="setup-link" href="/setup">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M19 12H5m7 7-7-7 7-7"></path></svg>
        <span>All setup options</span>
      </a>
    </nav>
    <h1>AREDN Wi-Fi Setup</h1>
    <p>The device scans for nearby networks automatically. Select yours from the list or enter a hidden network name manually.</p>
    <section class="current-connection" aria-labelledby="current-connection-heading">
      <h2 id="current-connection-heading">Current connection</h2>
      <p id="status" role="status" aria-live="polite">Checking device status.</p>
      <dl class="connection-details">
        <div>
          <dt>Router Wi-Fi address</dt>
          <dd id="router-address">Checking</dd>
        </div>
        <div>
          <dt>Setup access point address</dt>
          <dd id="setup-address">Checking</dd>
        </div>
      </dl>
    </section>
    <label for="networks">Available Wi-Fi networks</label>
    <select id="networks">
      <option value="">Choose a network</option>
      <option value="__scan__">Scan for networks</option>
    </select>
    <p id="scan-status" role="status" aria-live="polite"></p>
    <form id="wifi-form">
      <label for="ssid">Wi-Fi network name (SSID)</label>
      <input id="ssid" name="ssid" maxlength="32" autocomplete="off" required>
      <p class="hint">A selected network fills this field. For a hidden network, type its name here.</p>
      <label for="password">Wi-Fi password</label>
      <div class="password-field">
        <input id="password" name="password" type="password" maxlength="64" autocomplete="off" autocapitalize="off" spellcheck="false">
        <button id="password-toggle" class="password-toggle" type="button" aria-label="Show Wi-Fi password" aria-pressed="false">
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <path d="M2 12s3.5-6 10-6 10 6 10 6-3.5 6-10 6S2 12 2 12Z"></path>
            <circle cx="12" cy="12" r="2.5"></circle>
          </svg>
        </button>
      </div>
      <button id="submit" type="submit">Save and connect</button>
    </form>
  </main>
  <script>
    const statusElement = document.getElementById('status');
    const routerAddressElement = document.getElementById('router-address');
    const setupAddressElement = document.getElementById('setup-address');
    const submitButton = document.getElementById('submit');
    const scanStatus = document.getElementById('scan-status');
    const networksSelect = document.getElementById('networks');
    const ssidInput = document.getElementById('ssid');
    const passwordInput = document.getElementById('password');
    const passwordToggle = document.getElementById('password-toggle');
    let checkingConnection = false;
    let scanningNetworks = false;

    function showStatus(message, isError = false) {
      statusElement.textContent = message;
      statusElement.classList.toggle('error', isError);
    }

    passwordToggle.addEventListener('click', () => {
      const isVisible = passwordInput.type === 'text';
      passwordInput.type = isVisible ? 'password' : 'text';
      passwordToggle.setAttribute('aria-label', isVisible ? 'Show Wi-Fi password' : 'Hide Wi-Fi password');
      passwordToggle.setAttribute('aria-pressed', String(!isVisible));
    });

    async function refreshStatus() {
      try {
        const response = await fetch('/api/wifi/status', { cache: 'no-store' });
        if (!response.ok) throw new Error('status request failed');
        const status = await response.json();
        const connected = status.state === 'connected' && status.wifi_connected;
        routerAddressElement.textContent = connected
          ? status.sta_ip
          : status.state === 'connecting' ? 'Connecting.' : 'Not connected.';
        setupAddressElement.textContent = status.ap_ip || 'Unavailable';
        if (status.state === 'connected' && status.wifi_connected) {
          checkingConnection = false;
          submitButton.disabled = scanningNetworks;
          showStatus('Connected to router Wi-Fi.');
        } else if (status.state === 'connecting') {
          checkingConnection = true;
          submitButton.disabled = true;
          showStatus('Trying to connect to the Wi-Fi network.');
        } else if (status.state === 'failed') {
          checkingConnection = false;
          submitButton.disabled = scanningNetworks;
          showStatus('Could not connect. Check the network name and password, then try again.', true);
        } else if (status.state === 'connected') {
          checkingConnection = false;
          submitButton.disabled = scanningNetworks;
          showStatus('Router Wi-Fi is disconnected. The setup access point is still available.', true);
        } else {
          checkingConnection = false;
          submitButton.disabled = scanningNetworks;
          showStatus('Waiting for Wi-Fi details.');
        }
      } catch (error) {
        if (!checkingConnection) showStatus('Could not read device status. Check that you are connected to the setup access point.', true);
      }
    }

    function delay(milliseconds) {
      return new Promise((resolve) => window.setTimeout(resolve, milliseconds));
    }

    function describeScanError(error) {
      if (error === 'wifi_scan_failed') return 'the ESP32 reported a scan failure; check the device serial log';
      if (error === 'wifi_scan_busy') return 'the Wi-Fi scanner is busy; try again shortly';
      if (error === 'scan timed out') return 'the scan timed out';
      return error;
    }

    async function scanNetworks() {
      if (scanningNetworks) return;
      scanningNetworks = true;
      submitButton.disabled = true;
      scanStatus.textContent = 'Scanning nearby networks.';
      try {
        const startResponse = await fetch('/api/wifi/scan', { method: 'POST' });
        if (!startResponse.ok) {
          const error = await startResponse.json();
          throw new Error(error.error || `scan could not start (HTTP ${startResponse.status})`);
        }

        for (let attempt = 0; attempt < 45; attempt += 1) {
          await delay(1000);
          scanStatus.textContent = `Scanning nearby networks (${attempt + 1} s).`;
          let response;
          try {
            response = await fetch('/api/wifi/networks', { cache: 'no-store' });
          } catch (error) {
            if (attempt === 44) throw new Error('setup access point was temporarily unreachable');
            scanStatus.textContent =
              `Scan running (${attempt + 1} s). Reconnecting to the setup access point.`;
            continue;
          }
          if (response.status === 202) continue;
          if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || `scan failed (HTTP ${response.status})`);
          }
          let result;
          try {
            result = await response.json();
          } catch (error) {
            if (attempt === 44) throw new Error('scan result response was interrupted');
            scanStatus.textContent =
              `Scan running (${attempt + 1} s). Reconnecting to the setup access point.`;
            continue;
          }
          if (result.state === 'scanning') continue;
          if (result.state !== 'complete') throw new Error('scan results are unavailable');

          const strongestNetworks = new Map();
          for (const network of result.networks) {
            const key = JSON.stringify([network.ssid, network.secure]);
            const previous = strongestNetworks.get(key);
            if (previous === undefined || network.rssi > previous.rssi) {
              strongestNetworks.set(key, network);
            }
          }
          const visibleNetworks = [...strongestNetworks.values()]
            .sort((left, right) => right.rssi - left.rssi);
          const options = [
            new Option('Choose a network', '')
          ];
          for (const [index, network] of visibleNetworks.entries()) {
            const security = network.secure ? 'secured' : 'open';
            const option = new Option(
              `${network.ssid} (${network.rssi} dBm, ${security}, channel ${network.channel})`,
              `network-${index}`
            );
            option.dataset.ssid = network.ssid;
            option.dataset.secure = String(network.secure);
            options.push(option);
          }
          options.push(new Option('Scan for networks', '__scan__'));
          networksSelect.replaceChildren(...options);
          networksSelect.value = '';
          scanStatus.textContent = visibleNetworks.length === 0
            ? 'No networks found. Enter the SSID manually.'
            : `${visibleNetworks.length} network(s) found.`;
          return;
        }
        throw new Error('scan timed out');
      } catch (error) {
        const detail = error instanceof Error ? error.message : 'unknown scan error';
        scanStatus.textContent =
          `Could not scan: ${describeScanError(detail)}. Try again or enter the SSID manually.`;
      } finally {
        scanningNetworks = false;
        await refreshStatus();
      }
    }

    networksSelect.addEventListener('change', () => {
      if (networksSelect.value === '__scan__') {
        networksSelect.value = '';
        scanNetworks();
        return;
      }
      const selectedOption = networksSelect.selectedOptions[0];
      const selectedSsid = selectedOption.dataset.ssid;
      if (!selectedSsid) return;
      ssidInput.value = selectedSsid;
      if (selectedOption.dataset.secure === 'false') {
        passwordInput.value = '';
      }
    });
    ssidInput.addEventListener('input', () => {
      const selectedSsid = networksSelect.selectedOptions[0].dataset.ssid;
      if (selectedSsid && ssidInput.value !== selectedSsid) {
        networksSelect.value = '';
      }
    });

    document.getElementById('wifi-form').addEventListener('submit', async (event) => {
      event.preventDefault();
      submitButton.disabled = true;
      showStatus('Sending Wi-Fi details.');
      try {
        const response = await fetch('/api/wifi/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams(new FormData(event.currentTarget))
        });
        if (!response.ok) {
          const result = await response.json();
          throw new Error(result.error || 'configuration request failed');
        }
        checkingConnection = true;
        showStatus('Wi-Fi details received. Connecting.');
        passwordInput.value = '';
        await refreshStatus();
      } catch (error) {
        checkingConnection = false;
        submitButton.disabled = false;
        showStatus('Could not save the Wi-Fi details. Check the connection and try again.', true);
      }
    });

    scanNetworks();
    refreshStatus();
    window.setInterval(refreshStatus, 1500);
  </script>
</body>
</html>
)AREDNWIFI";
