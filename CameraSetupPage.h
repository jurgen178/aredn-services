#pragma once

#include <Arduino.h>

static const char CAMERA_SETUP_PAGE[] PROGMEM = R"CAMSETUP(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#173b33">
  <title>Camera Setup · AREDN Services</title>
  <style>
    :root { color-scheme: light; font-family: "Segoe UI", sans-serif; color: #172722; background: #f3f5ef; }
    * { box-sizing: border-box; }
    body { max-width: 620px; margin: 0 auto; padding: 24px 16px; }
    main { padding: 24px; border: 1px solid #d7ded7; border-radius: 10px; background: #fff; }
    h1 { margin-top: 0; color: #173b33; }
    p { line-height: 1.5; }
    label { display: block; margin: 16px 0 6px; font-weight: 700; }
    input { box-sizing: border-box; width: 100%; min-height: 44px; padding: 9px 11px; border: 1px solid #aebdb3; border-radius: 5px; font: inherit; }
    .password-visibility { display: flex; align-items: center; gap: 8px; margin: 8px 0 0; font-weight: 400; }
    .password-visibility input { width: auto; min-height: auto; margin: 0; }
    button { min-height: 44px; margin: 18px 8px 0 0; padding: 0 16px; border: 0; border-radius: 5px; color: #fff; background: #173b33; font: inherit; font-weight: 700; cursor: pointer; }
    button.secondary { color: #173b33; background: #e4eee8; }
    button:disabled { cursor: wait; opacity: 0.65; }
    .hint { margin: 5px 0 0; color: #5c6e66; font-size: 0.9rem; }
    #status { min-height: 24px; color: #5c6e66; }
    #status.error { color: #a2382d; }
    .warning { padding: 12px; border-left: 3px solid #c55b36; background: #fbf3ed; }
    a { color: #173b33; }
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
    <h1>Reolink camera setup</h1>
    <p>Set the camera's reserved IPv4 address and its HTTP port. The camera password is stored on this device and is never sent to camera-page visitors.</p>
    <p class="warning">Enable the camera's HTTP port in Reolink settings. Use a dedicated camera account if possible. The camera connection uses HTTP, so keep it on a trusted local network.</p>
    <form id="camera-form">
      <label for="host">Camera IPv4 address</label>
      <input id="host" name="host" inputmode="decimal" maxlength="15" placeholder="192.168.1.50" required>
      <label for="port">HTTP port</label>
      <input id="port" name="port" type="number" min="1" max="65535" value="80" required>
      <label for="username">Reolink username</label>
      <input id="username" name="username" maxlength="32" autocomplete="username" placeholder="admin or a dedicated viewer account" required>
      <label for="password">Reolink password</label>
      <input id="password" name="password" type="password" maxlength="64" autocomplete="new-password">
      <label class="password-visibility" for="show-password">
        <input id="show-password" type="checkbox">
        <span>Show password</span>
      </label>
      <p class="hint">On later saves, leave the password blank to keep the saved password.</p>
      <button id="save" type="submit">Save camera settings</button>
      <button id="clear" class="secondary" type="button">Clear saved settings</button>
    </form>
    <p id="status" role="status" aria-live="polite">Loading camera settings.</p>
      <p id="camera-state" role="status">Camera status: checking</p>
  </main>
  <script>
    const form = document.getElementById('camera-form');
    const statusElement = document.getElementById('status');
    const saveButton = document.getElementById('save');
    const clearButton = document.getElementById('clear');
    const cameraState = document.getElementById('camera-state');
    const passwordInput = document.getElementById('password');
    const showPassword = document.getElementById('show-password');

    function showStatus(message, isError = false) {
      statusElement.textContent = message;
      statusElement.classList.toggle('error', isError);
    }

    // Units are separated from the number by a space, as in the SI convention.
    function formatElapsedTime(totalSeconds) {
      if (totalSeconds < 60) return `${totalSeconds} s`;

      const seconds = String(totalSeconds % 60).padStart(2, '0');
      const totalMinutes = Math.floor(totalSeconds / 60);
      if (totalMinutes < 60) return `${totalMinutes} min ${seconds} s`;

      const hours = Math.floor(totalMinutes / 60);
      const minutes = String(totalMinutes % 60).padStart(2, '0');
      return `${hours} h ${minutes} min ${seconds} s`;
    }

    function describeCameraIssue(status) {
      if (status.status === 'not_configured') return 'Camera not configured.';
      if (status.status === 'waiting_for_network') {
        return 'Network unavailable; waiting to retry.';
      }
      if (status.status === 'storage_error') {
        return 'Camera service temporarily unavailable.';
      }

      switch (status.error) {
        case 'camera_unreachable':
          return 'Waiting for camera response; retrying.';
        case 'camera_request_failed':
          return 'Camera request failed; retrying.';
        case 'snapshot_timeout':
          return 'Camera took too long to send an image; retrying.';
        case 'invalid_snapshot':
          return 'Camera returned an invalid image; retrying.';
        case 'snapshot_too_large':
          return 'Camera image is too large; retrying.';
        case 'psram_unavailable':
        case 'snapshot_memory_unavailable':
          return 'Unable to prepare a new camera image.';
        case 'snapshot_busy':
          return 'Camera image is busy; trying again shortly.';
        case 'invalid_credentials':
          return 'Camera settings need to be checked.';
        default:
          if (status.error && status.error !== 'none') {
            return 'Could not get a new camera image; retrying.';
          }
          return status.status === 'updating'
            ? 'Checking for new camera image.'
            : '';
      }
    }

    showPassword.addEventListener('change', () => {
      passwordInput.type = showPassword.checked ? 'text' : 'password';
    });

    async function loadSettings() {
      try {
        const response = await fetch('/api/camera/config', { cache: 'no-store' });
        const settings = await response.json();
        if (!response.ok) throw new Error(settings.error || `HTTP ${response.status}`);
        document.getElementById('host').value = settings.host || '';
        document.getElementById('port').value = settings.port || 80;
        document.getElementById('username').value = settings.username || '';
        showStatus(settings.configured
          ? 'Camera settings are saved. Leave the password blank to keep it.'
          : 'Enter the camera address and credentials to configure snapshots.');
      } catch (error) {
        showStatus(`Could not load camera settings: ${error.message}`, true);
      }
    }

    async function refreshCameraState() {
      try {
        const response = await fetch('/camera/api/status', { cache: 'no-store' });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const status = await response.json();
        const issue = describeCameraIssue(status);
        // The setup page is a diagnostic page, so keep the technical reason visible.
        const technical = status.error && status.error !== 'none'
          ? [status.error, status.http_status ? `camera HTTP ${status.http_status}` : '']
            .filter(Boolean).join(', ')
          : '';
        const details = `${issue}${technical ? ` (${technical})` : ''}`;
        cameraState.textContent = status.image_available
          ? `Latest image is ${formatElapsedTime(status.age_seconds)} old.${details ? ` ${details}` : ''}`
          : details || 'Waiting for camera image.';
      } catch (error) {
        console.error('Camera status request failed:', error);
        cameraState.textContent = `Camera status temporarily unavailable (${error.message}).`;
      }
    }

    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      saveButton.disabled = true;
      clearButton.disabled = true;
      showStatus('Saving camera settings.');
      try {
        const response = await fetch('/api/camera/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams(new FormData(form))
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || `HTTP ${response.status}`);
        document.getElementById('password').value = '';
        showStatus('Settings saved. The server is requesting a camera snapshot.');
      } catch (error) {
        showStatus(`Could not save camera settings: ${error.message}`, true);
      } finally {
        saveButton.disabled = false;
        clearButton.disabled = false;
      }
    });

    clearButton.addEventListener('click', async () => {
      if (!window.confirm('Clear the saved camera address and credentials?')) return;
      saveButton.disabled = true;
      clearButton.disabled = true;
      try {
        const response = await fetch('/api/camera/config', { method: 'DELETE' });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || `HTTP ${response.status}`);
        form.reset();
        document.getElementById('port').value = '80';
        document.getElementById('username').value = '';
        showStatus('Saved camera settings cleared.');
      } catch (error) {
        showStatus(`Could not clear camera settings: ${error.message}`, true);
      } finally {
        saveButton.disabled = false;
        clearButton.disabled = false;
      }
    });

    loadSettings();
    refreshCameraState();
    window.setInterval(refreshCameraState, 5000);
  </script>
</body>
</html>
)CAMSETUP";
