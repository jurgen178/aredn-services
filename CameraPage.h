#pragma once

#include <Arduino.h>

static const char CAMERA_PAGE[] PROGMEM = R"AREDN_CAMERA(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#173b33">
  <title>Camera Snapshot · AREDN Services</title>
  <style>
    :root {
      color-scheme: light;
      font-family: "Trebuchet MS", "Segoe UI", sans-serif;
      color: #172722;
      background: #f3f5ef;
    }
    * { box-sizing: border-box; }
    body {
      min-width: 320px;
      min-height: 100vh;
      margin: 0;
      background-image: radial-gradient(#cbd5cc 0.65px, transparent 0.65px);
      background-size: 16px 16px;
    }
    header {
      padding: 28px 24px;
      color: #f6f7f2;
      background: #173b33;
      border-bottom: 5px solid #c55b36;
    }
    .header-inner, main {
      width: min(1000px, 100%);
      margin: 0 auto;
    }
    .header-inner {
      display: flex;
      align-items: center;
      justify-content: flex-start;
      gap: 20px;
    }
    .header-label {
      margin: 0 0 7px;
      color: #c8d9ce;
      font-size: 0.76rem;
      font-weight: 700;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }
    .header-label-link {
      color: inherit;
      text-decoration: none;
    }
    .header-label-link:hover, .header-label-link:focus-visible {
      text-decoration: underline;
      text-underline-offset: 3px;
    }
    .header-label-link:focus-visible { outline: 2px solid #e2a34c; outline-offset: 3px; }
    h1 { margin: 0; font-size: clamp(1.8rem, 6vw, 2.5rem); }
    .intro { margin: 8px 0 0; color: #d8e3dc; }
    .experimental-note { margin: 8px 0 0; color: #e2a34c; font-size: 0.72rem; line-height: 1.35; }
    .experimental-note a { color: inherit; }
    main { padding: 30px 24px; }
    .panel {
      padding: 22px;
      background: #fff;
      border: 1px solid #d7ded7;
      border-radius: 16px;
      box-shadow: 0 8px 24px rgb(23 39 34 / 7%);
    }
    .panel-head {
      display: flex;
      align-items: baseline;
      justify-content: space-between;
      gap: 16px;
      margin-bottom: 16px;
    }
    h2 { margin: 0; font-size: 1.15rem; }
    .status {
      margin: 0;
      color: #5c6e66;
      font-size: 0.85rem;
    }
    .panel-actions {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .camera-frame {
      position: relative;
      overflow: hidden;
      width: 100%;
      aspect-ratio: 4 / 3;
      display: grid;
      place-items: center;
      background-color: #192b2a;
      border: 1px solid #344b45;
      border-radius: 10px;
    }
    .camera-content {
      position: relative;
      overflow: hidden;
      width: 100%;
      height: 100%;
      aspect-ratio: 4 / 3;
      background-color: #192b2a;
    }
    .camera-frame:fullscreen {
      width: 100vw;
      height: 100vh;
      max-width: none;
      aspect-ratio: auto;
      background: #000;
      border: 0;
      border-radius: 0;
    }
    .camera-frame:fullscreen .camera-content {
      width: min(100vw, 133.3333vh);
      height: min(100vh, 75vw);
    }
    .camera-image {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      object-fit: contain;
    }
    .camera-image[hidden] { display: none; }
    .fullscreen-button {
      padding: 8px 11px;
      color: #173b33;
      font: inherit;
      font-size: 0.82rem;
      background: #e4eee8;
      border: 1px solid #c8d9ce;
      border-radius: 8px;
      cursor: pointer;
    }
    .fullscreen-button:hover { background: #d8e7dd; }
    .fullscreen-button:focus-visible {
      outline: 3px solid #f0a24b;
      outline-offset: 2px;
    }
    .fullscreen-button:disabled {
      cursor: not-allowed;
      opacity: 0.7;
    }
    .fullscreen-exit-button {
      position: absolute;
      z-index: 2;
      top: 10px;
      right: 10px;
      color: #f6f7f2;
      background: rgb(23 39 34 / 88%);
      border-color: rgb(255 255 255 / 45%);
    }
    .fullscreen-exit-button:not([hidden]) { display: block; }
    .fullscreen-exit-button:hover { background: #173b33; }
    .fullscreen-status {
      margin: 10px 0 0;
      color: #a2382d;
      font-size: 0.85rem;
    }
    .camera-placeholder {
      position: absolute;
      inset: 0;
      display: grid;
      align-content: center;
      justify-items: center;
      padding: 20px;
      color: #e4eee8;
      text-align: center;
      pointer-events: none;
    }
    .camera-placeholder[hidden] { display: none; }
    .camera-placeholder strong {
      font-size: clamp(1rem, 4vw, 1.4rem);
      letter-spacing: 0.06em;
      text-transform: uppercase;
    }
    .camera-placeholder span {
      margin-top: 8px;
      color: #b7c9bf;
      font-size: clamp(0.78rem, 2.5vw, 0.95rem);
    }
    .camera-overlay {
      position: absolute;
      inset: 0;
      z-index: 2;
      pointer-events: none;
    }
    .camera-overlay[hidden] { display: none; }
    .camera-overlay::after {
      position: absolute;
      inset: 38% 0 0;
      background: linear-gradient(180deg, transparent, rgb(9 19 20 / 82%));
      content: "";
    }
    .map-labels {
      position: absolute;
      inset: 0;
      z-index: 3;
      width: 100%;
      height: 100%;
      display: none;
      pointer-events: none;
    }
    .map-labels.is-visible { display: block; }
    .map-place-line {
      stroke: var(--marker-color, #f0a24b);
      stroke-width: 0.25;
      opacity: 0.9;
    }
    .map-place-badge {
      fill: rgb(9 19 20 / 82%);
      stroke: rgb(255 255 255 / 30%);
      stroke-width: 0.2;
    }
    .map-place-dot {
      fill: var(--marker-color, #f0a24b);
      stroke: #fff;
      stroke-width: 0.35;
    }
    .map-place-text {
      fill: var(--text-color, #fff);
      stroke: rgb(8 18 19 / 90%);
      stroke-width: 0.65;
      stroke-linejoin: round;
      paint-order: stroke;
      font-family: "Segoe UI", sans-serif;
      font-size: 2px;
      font-weight: 700;
    }
    .overlay-badge {
      position: absolute;
      z-index: 1;
      top: clamp(12px, 2.5vw, 24px);
      left: clamp(12px, 2.5vw, 24px);
      padding: 7px 11px;
      color: #f6f7f2;
      font-size: clamp(0.62rem, 1.4vw, 0.76rem);
      font-weight: 700;
      letter-spacing: 0.1em;
      background: rgb(12 27 27 / 68%);
      border: 1px solid rgb(255 255 255 / 24%);
      border-radius: 999px;
      backdrop-filter: blur(10px);
      -webkit-backdrop-filter: blur(10px);
    }
    .overlay-bottom {
      position: absolute;
      z-index: 1;
      right: clamp(14px, 3vw, 28px);
      bottom: clamp(14px, 3vw, 28px);
      left: clamp(14px, 3vw, 28px);
      display: flex;
      align-items: flex-end;
      justify-content: space-between;
      gap: 16px;
      color: #fff;
    }
    .overlay-location, .overlay-capture {
      display: grid;
      gap: 4px;
      min-width: 0;
      text-shadow: 0 1px 8px rgb(0 0 0 / 40%);
    }
    .overlay-kicker {
      color: #c8d9ce;
      font-size: clamp(0.58rem, 1.3vw, 0.7rem);
      font-weight: 700;
      letter-spacing: 0.12em;
      text-transform: uppercase;
    }
    .overlay-location strong {
      font-size: clamp(1rem, 2.7vw, 1.5rem);
      font-weight: 600;
      letter-spacing: -0.02em;
    }
    .overlay-capture { justify-items: end; text-align: right; }
    .overlay-capture time { white-space: nowrap; }
    #captureDate {
      color: #d8e3dc;
      font-size: clamp(0.72rem, 1.6vw, 0.88rem);
    }
    #captureTime {
      font-size: clamp(1rem, 2.7vw, 1.5rem);
      font-variant-numeric: tabular-nums;
      font-weight: 600;
      letter-spacing: 0.02em;
    }
    .note {
      margin: 16px 0 0;
      color: #5c6e66;
      line-height: 1.5;
    }
    @media (max-width: 620px) {
      .header-inner, .panel-head { align-items: flex-start; flex-direction: column; }
      .panel-actions { width: 100%; justify-content: space-between; }
      .panel { padding: 16px; }
      main { padding: 20px 14px; }
    }
  </style>
</head>
<body>
  <header>
    <div class="header-inner">
      <div>
        <p class="header-label"><a class="header-label-link" href="/" title="Open the AREDN services portal">AREDN · Bellevue Downtown View</a></p>
        <h1>Camera Snapshot</h1>
        <p class="intro">A still image from Bellevue Downtown, refreshed about every 30 seconds.</p>
        <p class="experimental-note">EXPERIMENTAL ESP32 WEB SERVICE · <a href="https://github.com/jurgen178/aredn-services" target="_blank" rel="noopener noreferrer">Project on GitHub</a></p>
      </div>
    </div>
  </header>
  <main>
    <section class="panel" aria-labelledby="previewTitle">
      <div class="panel-head">
        <h2 id="previewTitle">Camera preview</h2>
        <div class="panel-actions">
          <p id="cameraStatus" class="status" role="status">Camera not configured</p>
          <button id="refreshButton" class="fullscreen-button" type="button">Get image now</button>
          <button id="labelsButton" class="fullscreen-button" type="button" disabled>Hide labels</button>
          <button id="fullscreenButton" class="fullscreen-button" type="button">Fullscreen</button>
        </div>
      </div>
      <div id="cameraFrame" class="camera-frame" role="region" aria-label="Camera preview">
        <button id="fullscreenExitButton" class="fullscreen-button fullscreen-exit-button" type="button" hidden>Exit fullscreen</button>
        <div class="camera-content">
          <img id="cameraImage" class="camera-image" alt="Latest camera snapshot of Bellevue Downtown" hidden>
          <svg id="mapLabels" class="map-labels" viewBox="0 0 100 75" preserveAspectRatio="none" role="img" aria-label="Landmark labels">
            <!-- Point and label coordinates are percentages from the top-left; copy a group to add a place. -->
            <g class="map-place" data-point-x="25" data-point-y="43" data-label-x="35" data-label-y="38" style="--marker-color: #f0a24b; --text-color: #fff;">
              <line class="map-place-line"></line>
              <rect class="map-place-badge" rx="1.2"></rect>
              <circle class="map-place-dot" cx="25%" cy="43%" r="0.9"></circle>
              <text class="map-place-text" x="35%" y="38%" text-anchor="middle" dominant-baseline="middle">Hospital</text>
            </g>
          </svg>
          <div id="cameraPlaceholder" class="camera-placeholder">
            <strong>Camera snapshot pending</strong>
            <span>Configure the Reolink camera through the device setup access point.</span>
          </div>
          <div id="cameraOverlay" class="camera-overlay" hidden aria-label="Snapshot information">
            <span class="overlay-badge">AREDN CAMERA</span>
            <div class="overlay-bottom">
              <div class="overlay-location">
                <span class="overlay-kicker">Camera view</span>
                <strong>Bellevue Downtown</strong>
              </div>
              <div class="overlay-capture">
                <span class="overlay-kicker">Captured · UTC</span>
                <time id="captureDate"></time>
                <time id="captureTime"></time>
              </div>
            </div>
          </div>
        </div>
      </div>
      <p id="fullscreenStatus" class="fullscreen-status" role="status" hidden></p>
      <p class="note">The ESP32 refreshes one shared camera snapshot every 30 seconds.</p>
    </section>
  </main>
  <script>
    const cameraFrame = document.getElementById('cameraFrame');
    const fullscreenButton = document.getElementById('fullscreenButton');
    const fullscreenExitButton = document.getElementById('fullscreenExitButton');
    const fullscreenStatus = document.getElementById('fullscreenStatus');
    const cameraImage = document.getElementById('cameraImage');
    const mapLabels = document.getElementById('mapLabels');
    const cameraPlaceholder = document.getElementById('cameraPlaceholder');
    const cameraOverlay = document.getElementById('cameraOverlay');
    const cameraStatus = document.getElementById('cameraStatus');
    const refreshButton = document.getElementById('refreshButton');
    const labelsButton = document.getElementById('labelsButton');
    const captureDate = document.getElementById('captureDate');
    const captureTime = document.getElementById('captureTime');
    let currentVersion = 0;
    let currentImageUrl = '';
    let imageRequestInProgress = false;
    let imageRequestToken = 0;
    let labelsEnabled = true;
    const fullscreenSupported =
      document.fullscreenEnabled !== false &&
      typeof cameraFrame.requestFullscreen === 'function' &&
      typeof document.exitFullscreen === 'function';

    async function synchronizeDeviceTime() {
      try {
        const response = await fetch('/api/time/sync', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams({ epoch: String(Math.floor(Date.now() / 1000)) })
        });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
      } catch (error) {
        console.error('Device clock synchronization failed:', error);
      }
    }

    function updateFullscreenButton() {
      const isFullscreen = document.fullscreenElement === cameraFrame;
      fullscreenButton.hidden = isFullscreen;
      fullscreenExitButton.hidden = !isFullscreen;
      fullscreenButton.setAttribute('aria-label', 'View camera fullscreen');
      fullscreenButton.setAttribute('aria-pressed', String(isFullscreen));
      fullscreenExitButton.setAttribute('aria-label', 'Exit camera fullscreen');
    }

    function describeCameraStatus(status) {
      if (status.image_available) {
        if (status.state === 'stale') return `Showing the last snapshot · ${status.age_seconds}s old · ${status.error}`;
        if (status.state === 'updating') return `Updating snapshot · last image ${status.age_seconds}s old`;
        return `Snapshot updated ${status.age_seconds}s ago`;
      }
      if (status.state === 'not_configured') return 'Camera not configured';
      if (status.state === 'waiting_for_network') return 'Waiting for the server network connection';
      if (status.state === 'storage_error') return 'Camera configuration storage is unavailable';
      if (status.state === 'updating') return 'Connecting to the camera...';
      return `Camera snapshot unavailable · ${status.error}`;
    }

    function layoutMapLabels() {
      const viewBox = mapLabels.viewBox.baseVal;
      for (const place of mapLabels.querySelectorAll('.map-place')) {
        const pointX = Number(place.dataset.pointX);
        const pointY = Number(place.dataset.pointY);
        const labelX = Number(place.dataset.labelX);
        const labelY = Number(place.dataset.labelY);
        const line = place.querySelector('.map-place-line');
        const marker = place.querySelector('.map-place-dot');
        const badge = place.querySelector('.map-place-badge');
        const label = place.querySelector('.map-place-text');
        const pointSvgX = viewBox.x + viewBox.width * pointX / 100;
        const pointSvgY = viewBox.y + viewBox.height * pointY / 100;

        marker.setAttribute('cx', `${pointX}%`);
        marker.setAttribute('cy', `${pointY}%`);
        label.setAttribute('x', `${labelX}%`);
        label.setAttribute('y', `${labelY}%`);
        line.setAttribute('x1', pointSvgX);
        line.setAttribute('y1', pointSvgY);

        const bounds = label.getBBox();
        const paddingX = 0.6;
        const paddingY = 0.4;
        badge.setAttribute('x', bounds.x - paddingX);
        badge.setAttribute('y', bounds.y - paddingY);
        badge.setAttribute('width', bounds.width + paddingX * 2);
        badge.setAttribute('height', bounds.height + paddingY * 2);

        const left = bounds.x - paddingX;
        const right = bounds.x + bounds.width + paddingX;
        const top = bounds.y - paddingY;
        const bottom = bounds.y + bounds.height + paddingY;
        const edgeCenters = [
          { x: left, y: (top + bottom) / 2 },
          { x: right, y: (top + bottom) / 2 },
          { x: (left + right) / 2, y: top },
          { x: (left + right) / 2, y: bottom }
        ];
        const nearestEdge = edgeCenters.reduce((nearest, candidate) =>
          Math.hypot(candidate.x - pointSvgX, candidate.y - pointSvgY) <
                  Math.hypot(nearest.x - pointSvgX, nearest.y - pointSvgY)
              ? candidate
              : nearest);
        line.setAttribute('x2', nearestEdge.x);
        line.setAttribute('y2', nearestEdge.y);
      }
    }

    function updateLabelsButton() {
      labelsButton.textContent = labelsEnabled ? 'Hide labels' : 'Show labels';
      labelsButton.disabled = !currentImageUrl;
      mapLabels.classList.toggle('is-visible', Boolean(currentImageUrl) && labelsEnabled);
      if (currentImageUrl && labelsEnabled) layoutMapLabels();
    }

    async function loadSnapshot(version, captureEpoch) {
      if (imageRequestInProgress || version <= currentVersion) return;
      imageRequestInProgress = true;
      const requestToken = ++imageRequestToken;
      let nextImageUrl = '';
      try {
        const response = await fetch(`/camera/image.jpg?v=${version}`, { cache: 'no-store' });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const imageBlob = await response.blob();
        if (imageBlob.type && imageBlob.type !== 'image/jpeg') {
          throw new Error('The server did not return a JPEG image.');
        }
        if (requestToken !== imageRequestToken) return;

        nextImageUrl = URL.createObjectURL(imageBlob);
        const previousImageUrl = currentImageUrl;
        await new Promise((resolve, reject) => {
          cameraImage.onload = resolve;
          cameraImage.onerror = () => reject(new Error('The snapshot is not a valid image.'));
          cameraImage.src = nextImageUrl;
        });
        currentImageUrl = nextImageUrl;
        currentVersion = version;
        cameraImage.hidden = false;
        cameraPlaceholder.hidden = true;
        cameraOverlay.hidden = false;
        updateLabelsButton();
        const capturedAt = captureEpoch > 0 ? new Date(captureEpoch * 1000) : null;
        captureDate.textContent = capturedAt
          ? new Intl.DateTimeFormat('en-GB', {
            timeZone: 'UTC',
            dateStyle: 'medium'
          }).format(capturedAt)
          : 'Controller time unavailable';
        captureTime.textContent = capturedAt
          ? new Intl.DateTimeFormat('en-GB', {
            timeZone: 'UTC',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hourCycle: 'h23'
          }).format(capturedAt)
          : '--:--:--';
        if (previousImageUrl) URL.revokeObjectURL(previousImageUrl);
      } catch (error) {
        if (nextImageUrl) URL.revokeObjectURL(nextImageUrl);
        if (requestToken === imageRequestToken && !currentImageUrl) {
          cameraStatus.textContent = `Could not load camera snapshot: ${error.message}`;
        }
      } finally {
        imageRequestInProgress = false;
      }
    }

    async function refreshCameraStatus() {
      if (document.hidden) return null;
      try {
        const response = await fetch('/camera/api/status', { cache: 'no-store' });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const status = await response.json();
        cameraStatus.textContent = describeCameraStatus(status);
        if (!status.image_available) {
          ++imageRequestToken;
          imageRequestInProgress = false;
          cameraImage.hidden = true;
          cameraPlaceholder.hidden = false;
          cameraOverlay.hidden = true;
          if (currentImageUrl) URL.revokeObjectURL(currentImageUrl);
          currentImageUrl = '';
          updateLabelsButton();
          currentVersion = status.version;
          return status;
        }
        if (status.version !== currentVersion) {
          await loadSnapshot(status.version, status.capture_epoch);
        }
        return status;
      } catch (error) {
        cameraStatus.textContent = `Could not read camera status: ${error.message}`;
        return null;
      }
    }

    async function requestCameraSnapshot() {
      refreshButton.disabled = true;
      cameraStatus.textContent = 'Requesting a fresh snapshot...';
      try {
        const response = await fetch('/camera/api/snapshot', { method: 'POST' });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || `HTTP ${response.status}`);

        const deadline = Date.now() + 20000;
        while (Date.now() < deadline) {
          const status = await refreshCameraStatus();
          if (status && status.state !== 'updating') {
            if (!status.image_available) {
              throw new Error(`Snapshot unavailable: ${status.error}`);
            }
            return;
          }
          await new Promise(resolve => window.setTimeout(resolve, 1000));
        }
        throw new Error('Timed out waiting for the camera snapshot.');
      } catch (error) {
        cameraStatus.textContent = `Could not get a fresh camera snapshot: ${error.message}`;
      } finally {
        refreshButton.disabled = false;
      }
    }

    async function toggleFullscreen() {
      if (!fullscreenSupported) {
        return;
      }

      fullscreenStatus.hidden = true;
      try {
        if (document.fullscreenElement === cameraFrame) {
          await document.exitFullscreen();
        } else {
          await cameraFrame.requestFullscreen();
        }
      } catch {
        fullscreenStatus.textContent = 'Fullscreen could not be changed in this browser.';
        fullscreenStatus.hidden = false;
      }
    }

    refreshButton.addEventListener('click', requestCameraSnapshot);
    fullscreenButton.addEventListener('click', toggleFullscreen);
    fullscreenExitButton.addEventListener('click', toggleFullscreen);
    labelsButton.addEventListener('click', () => {
      labelsEnabled = !labelsEnabled;
      updateLabelsButton();
    });

    document.addEventListener('fullscreenchange', updateFullscreenButton);
    document.addEventListener('visibilitychange', () => {
      if (!document.hidden) refreshCameraStatus();
    });
    if (!fullscreenSupported) {
      fullscreenButton.disabled = true;
      fullscreenExitButton.disabled = true;
      fullscreenButton.title = 'Fullscreen is not supported by this browser.';
      fullscreenStatus.textContent = 'Fullscreen is not supported by this browser.';
      fullscreenStatus.hidden = false;
    }
    updateFullscreenButton();
    synchronizeDeviceTime();
    refreshCameraStatus();
    window.setInterval(refreshCameraStatus, 15000);
  </script>
</body>
</html>
)AREDN_CAMERA";
