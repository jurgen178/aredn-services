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
      flex: 0 0 clamp(12rem, 24vw, 22rem);
      min-width: 0;
      margin: 0;
      color: #5c6e66;
      font-size: 0.85rem;
      overflow-wrap: anywhere;
    }
    .panel-actions {
      display: flex;
      min-width: 0;
      flex-wrap: wrap;
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
      width: min(100vw, calc(100vh * var(--camera-crop-ratio, 1.333333)));
      height: min(100vh, calc(100vw / var(--camera-crop-ratio, 1.333333)));
    }
    .camera-image {
      position: absolute;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
    }
    .camera-image[hidden] { display: none; }
    .fullscreen-button {
      flex: 0 0 auto;
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
    .map-labels {
      position: absolute;
      top: 0;
      left: 0;
      z-index: 3;
      width: 100%;
      height: 100%;
      display: none;
      font-size: clamp(0.72rem, 1.6vw, 0.88rem);
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
      gap: 3px;
      min-width: 0;
      justify-items: start;
    }
    .overlay-kicker, .overlay-location strong, #captureDate, #captureTime {
      padding: 1px 6px;
      background: rgb(12 27 27 / 28%);
      border-radius: 5px;
      text-shadow: 0 1px 3px rgb(0 0 0 / 70%);
      white-space: nowrap;
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
      .panel-actions { width: 100%; justify-content: flex-start; }
      .status { flex-basis: 100%; }
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
        <p class="intro">A camera view of Bellevue Downtown, updated every 30 s.</p>
        <p class="experimental-note">EXPERIMENTAL ESP32 WEB SERVICE · <a href="https://github.com/jurgen178/aredn-services" target="_blank" rel="noopener noreferrer">Project on GitHub</a></p>
      </div>
    </div>
  </header>
  <main>
    <section class="panel" aria-labelledby="cameraImageTitle">
      <div class="panel-head">
        <h2 id="cameraImageTitle">Latest camera image</h2>
        <div class="panel-actions">
          <p id="cameraStatus" class="status" role="status">Waiting for camera image...</p>
          <button id="labelsButton" class="fullscreen-button" type="button" disabled>Show labels</button>
          <button id="screenshotButton" class="fullscreen-button" type="button" disabled>Save screenshot</button>
          <button id="fullscreenButton" class="fullscreen-button" type="button">Fullscreen</button>
        </div>
      </div>
      <div id="cameraFrame" class="camera-frame" role="region" aria-label="Camera preview">
        <button id="fullscreenExitButton" class="fullscreen-button fullscreen-exit-button" type="button" hidden>Exit fullscreen</button>
        <div class="camera-content">
          <img id="cameraImage" class="camera-image" alt="Latest camera snapshot of Bellevue Downtown" hidden>
          <svg id="mapLabels" class="map-labels" viewBox="0 0 100 75" preserveAspectRatio="none" role="img" aria-label="Landmark labels">
            <!-- Point and label coordinates are source-image pixels from the top-left. -->
            <g class="map-place"
              data-point-x="1040" data-point-y="1160"
              data-label-x="850" data-label-y="1260"
              style="--marker-color: #f0a24b; --text-color: #fff;">
              <line class="map-place-line"></line>
              <rect class="map-place-badge" rx="1.2"></rect>
              <circle class="map-place-dot" r="0.9"></circle>
              <text class="map-place-text" 
                    text-anchor="middle" 
                    dominant-baseline="middle">Overlake Hospital</text>
            </g>
            <g class="map-place"
              data-point-x="1280" data-point-y="1180"
              data-label-x="1460" data-label-y="1040"
              style="--marker-color: #55c7a5; --text-color: #fff;">
              <line class="map-place-line"></line>
              <rect class="map-place-badge" rx="1.2"></rect>
              <circle class="map-place-dot" r="0.9"></circle>
              <text class="map-place-text"
                    text-anchor="middle"
                    dominant-baseline="middle">Seattle Childrens Urgent Care</text>
            </g>
            <g class="map-place"
              data-point-x="930" data-point-y="990"
              data-label-x="950" data-label-y="870"
              style="--marker-color: #3974f2; --text-color: #fff;">
              <line class="map-place-line"></line>
              <rect class="map-place-badge" rx="1.2"></rect>
              <circle class="map-place-dot" r="0.9"></circle>
              <text class="map-place-text"
                    text-anchor="middle"
                    dominant-baseline="middle">Sea-Tac Northbound Flight Path</text>
            </g>
          </svg>
          <div id="cameraPlaceholder" class="camera-placeholder">
            <strong>Camera snapshot pending</strong>
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
    const cameraBrandLabel = cameraOverlay.querySelector('.overlay-badge');
    const cameraStatus = document.getElementById('cameraStatus');
    const labelsButton = document.getElementById('labelsButton');
    const screenshotButton = document.getElementById('screenshotButton');
    const captureDate = document.getElementById('captureDate');
    const captureTime = document.getElementById('captureTime');
    const cameraCrop = {
      sourceWidth: 2560,
      sourceHeight: 1920,
      left: 704,
      top: 640,
      width: 1333,
      height: 805
    };
    let screenshotInProgress = false;
    let currentVersion = 0;
    let currentImageUrl = '';
    let imageRequestInProgress = false;
    let imageRequestToken = 0;
    let imageRequestController = null;
    let imageRetryTimer = null;
    let imageRetryDelayMs = 2000;
    let lastImageDownloadMs = 0;
    let labelsEnabled = false;
    let latestCameraStatus = null;
    let latestStatusReceivedAt = 0;
    let lastRenderedCameraStatus = '';
    // Transient reason why the image is not up to date; empty in normal operation.
    let cameraActivity = '';
    // Capture time of the displayed image. Date.now() keeps counting while the
    // computer sleeps, unlike performance.now().
    let displayedCapturedAtMs = null;
    let statusRequestInProgress = false;
    let statusRequestToken = 0;
    let statusRequestController = null;
    const fullscreenSupported =
      document.fullscreenEnabled !== false &&
      typeof cameraFrame.requestFullscreen === 'function' &&
      typeof document.exitFullscreen === 'function';

    // Diagnostic output for the browser console (F12). Plain strings only, so the
    // lines can be copied and pasted as they are.
    let lastLoggedNotice = null;
    let skipLogged = false;
    function debugLog(message) {
      const now = new Date();
      const time = now.toTimeString().slice(0, 8) +
        '.' + String(now.getMilliseconds()).padStart(3, '0');
      console.log(`[camera ${time}] ${message}`);
    }

    function describeError(error) {
      return `${error?.name || 'Error'}: ${error?.message || error}`;
    }

    function describeBrowserState() {
      return `tab=${document.visibilityState}, online=${navigator.onLine}`;
    }

    async function fetchWithTimeout(url, options, timeoutMs, readResponse, controller) {
      const timeoutId = window.setTimeout(() => controller.abort(), timeoutMs);
      try {
        const response = await fetch(url, { ...options, signal: controller.signal });
        return await readResponse(response);
      } catch (error) {
        if (controller.signal.aborted) {
          throw new Error(`Request timed out after ${timeoutMs / 1000} seconds.`);
        }
        throw error;
      } finally {
        window.clearTimeout(timeoutId);
      }
    }

    async function fetchSnapshotBlob(url, controller, progress) {
      const idleTimeoutMs = Math.max(30000, Math.min(60000, lastImageDownloadMs * 2));
      let timeoutReason = '';
      let idleTimer;
      function resetIdleTimer() {
        window.clearTimeout(idleTimer);
        idleTimer = window.setTimeout(() => {
          timeoutReason = `No image data received for ${idleTimeoutMs / 1000} seconds.`;
          controller.abort();
        }, idleTimeoutMs);
      }
      const totalTimer = window.setTimeout(() => {
        timeoutReason = 'Image download exceeded the 120 second safety limit.';
        controller.abort();
      }, 120000);
      resetIdleTimer();
      try {
        const response = await fetch(url, { cache: 'no-store', signal: controller.signal });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const contentLength = response.headers?.get('Content-Length');
        if (contentLength && /^\d+$/.test(contentLength)) {
          const parsed = Number(contentLength);
          if (Number.isSafeInteger(parsed)) progress.expected = parsed;
        }
        const contentType = response.headers?.get('Content-Type')?.split(';')[0].trim().toLowerCase();
        if (contentType && contentType !== 'image/jpeg') {
          throw new Error('The server did not return a JPEG image.');
        }
        const contentEncoding = response.headers?.get('Content-Encoding')?.trim().toLowerCase();
        function checkLength() {
          if (progress.expected !== null && (!contentEncoding || contentEncoding === 'identity') &&
              progress.received !== progress.expected) {
            throw new Error(`Incomplete image: received ${progress.received} of ${progress.expected} bytes.`);
          }
        }
        if (!response.body?.getReader) {
          const blob = await response.blob();
          progress.received = blob.size;
          if (blob.type && blob.type !== 'image/jpeg') {
            throw new Error('The server did not return a JPEG image.');
          }
          checkLength();
          return blob;
        }
        const reader = response.body.getReader();
        const chunks = [];
        for (;;) {
          const { done, value } = await reader.read();
          if (done) break;
          if (value.byteLength > 0) {
            chunks.push(value);
            progress.received += value.byteLength;
            resetIdleTimer();
          }
        }
        checkLength();
        return new Blob(chunks, { type: 'image/jpeg' });
      } catch (error) {
        if (timeoutReason) throw new Error(timeoutReason);
        throw error;
      } finally {
        window.clearTimeout(idleTimer);
        window.clearTimeout(totalTimer);
      }
    }

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
        debugLog(`clock sync FAILED: ${describeError(error)} (${describeBrowserState()})`);
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
      if (!status) return '';
      if (status.status === 'not_configured') return 'Camera not configured';
      if (status.status === 'waiting_for_network') {
        return 'Network unavailable; waiting to retry...';
      }
      if (status.status === 'storage_error') {
        return 'Camera service temporarily unavailable.';
      }

      switch (status.error) {
        case 'camera_unreachable':
          return 'Camera cannot be reached; retrying...';
        case 'camera_request_failed':
          return 'Camera request failed; retrying...';
        case 'snapshot_timeout':
          return 'Camera took too long to send an image; retrying...';
        case 'invalid_snapshot':
          return 'Camera returned an invalid image; retrying...';
        case 'snapshot_too_large':
          return 'Camera image is too large; retrying...';
        case 'psram_unavailable':
        case 'snapshot_memory_unavailable':
          return 'Unable to prepare a new camera image.';
        case 'snapshot_busy':
          return 'Camera image is busy; trying again shortly...';
        case 'invalid_credentials':
          return 'Camera settings need to be checked.';
        default:
          if (status.error && status.error !== 'none') {
            return 'Could not get a new camera image; retrying...';
          }
          return status.status === 'updating'
            ? 'Checking for a newer camera image...'
            : '';
      }
    }

    // Show the image age. A short hiccup (for example a request that fails while
    // the network is still reconnecting) must stay invisible, so an explanation is
    // only added once the image is overdue by more than one missed update.
    function describeCameraStatus() {
      const activity = cameraActivity || describeCameraIssue(latestCameraStatus);
      if (displayedCapturedAtMs !== null) {
        const age = Math.max(0, Math.floor((Date.now() - displayedCapturedAtMs) / 1000));
        const base = `Updated ${formatElapsedTime(age)} ago`;
        const overdueAfter = 2 * (latestCameraStatus?.refresh_seconds || 30);
        const shown = Boolean(activity) && age > overdueAfter;
        const notice = `${activity || 'none'} (${shown ? 'shown' : 'hidden, image not overdue'})`;
        if (notice !== lastLoggedNotice) {
          lastLoggedNotice = notice;
          debugLog(`notice: ${notice}, image age ${age} s`);
        }
        return shown ? `${base} · ${activity}` : base;
      }
      const notice = `${activity || 'waiting for image'} (shown, no image)`;
      if (notice !== lastLoggedNotice) {
        lastLoggedNotice = notice;
        debugLog(`notice: ${notice}`);
      }
      return activity || 'Waiting for camera image...';
    }

    function showCameraStatus() {
      lastRenderedCameraStatus = describeCameraStatus();
      cameraStatus.textContent = lastRenderedCameraStatus;
    }

    function renderCameraStatus(status) {
      latestCameraStatus = status;
      cameraActivity = imageRequestInProgress ? 'Loading new image...' : '';
      latestStatusReceivedAt = Date.now();
      showCameraStatus();
    }

    function updateCameraStatusAge() {
      if (document.hidden || cameraStatus.textContent !== lastRenderedCameraStatus) {
        return;
      }
      showCameraStatus();
    }

    function layoutMapLabels() {
      const viewBox = mapLabels.viewBox.baseVal;
      const mapWidth = mapLabels.getBoundingClientRect().width;
      if (!mapWidth) return;
      const screenFontSize = Number.parseFloat(getComputedStyle(mapLabels).fontSize);
      const fontSize = screenFontSize * viewBox.width / mapWidth;
      const targetSizeScale = fontSize / 2;
      const toSvgX = sourceX =>
        viewBox.x + viewBox.width * (sourceX - cameraCrop.left) / cameraCrop.width;
      const toSvgY = sourceY =>
        viewBox.y + viewBox.height * (sourceY - cameraCrop.top) / cameraCrop.height;
      for (const place of mapLabels.querySelectorAll('.map-place')) {
        const pointX = Number(place.dataset.pointX);
        const pointY = Number(place.dataset.pointY);
        const labelX = Number(place.dataset.labelX);
        const labelY = Number(place.dataset.labelY);
        const line = place.querySelector('.map-place-line');
        const marker = place.querySelector('.map-place-dot');
        const badge = place.querySelector('.map-place-badge');
        const label = place.querySelector('.map-place-text');
        const pointSvgX = toSvgX(pointX);
        const pointSvgY = toSvgY(pointY);
        const labelSvgX = toSvgX(labelX);
        const labelSvgY = toSvgY(labelY);

        label.style.fontSize = `${fontSize}px`;
        label.setAttribute('x', labelSvgX);
        label.setAttribute('y', labelSvgY);
        const leftSpace = labelSvgX - viewBox.x;
        const rightSpace = viewBox.x + viewBox.width - labelSvgX;
        const textAnchor = leftSpace < rightSpace * 0.75
          ? 'start'
          : rightSpace < leftSpace * 0.75
            ? 'end'
            : 'middle';
        label.setAttribute('text-anchor', textAnchor);
        const availableWidth = textAnchor === 'start'
          ? rightSpace
          : textAnchor === 'end'
            ? leftSpace
            : 2 * Math.min(leftSpace, rightSpace);
        const textWidth = Math.max(label.getComputedTextLength(), label.getBBox().width);
        const sizeScale = Math.min(
          targetSizeScale,
          Math.max(0, availableWidth) / (textWidth / targetSizeScale + 1.65)
        );
        label.style.fontSize = `${2 * sizeScale}px`;
        label.style.strokeWidth = `${0.65 * sizeScale}px`;
        line.style.strokeWidth = `${0.25 * sizeScale}px`;
        marker.style.strokeWidth = `${0.35 * sizeScale}px`;
        marker.setAttribute('r', 0.9 * sizeScale);
        badge.style.strokeWidth = `${0.2 * sizeScale}px`;
        badge.setAttribute('rx', 1.2 * sizeScale);
        marker.setAttribute('cx', pointSvgX);
        marker.setAttribute('cy', pointSvgY);
        line.setAttribute('x1', pointSvgX);
        line.setAttribute('y1', pointSvgY);

        const bounds = label.getBBox();
        const paddingX = 0.4 * sizeScale;
        const paddingY = 0.25 * sizeScale;
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
      screenshotButton.disabled = !currentImageUrl || screenshotInProgress;
      mapLabels.classList.toggle('is-visible', Boolean(currentImageUrl) && labelsEnabled);
      cameraBrandLabel.hidden = !labelsEnabled;
      if (currentImageUrl && labelsEnabled) layoutMapLabels();
    }

    function getScreenshotBox(element, rootRect) {
      const rect = element.getBoundingClientRect();
      return {
        x: rect.left - rootRect.left,
        y: rect.top - rootRect.top,
        width: rect.width,
        height: rect.height
      };
    }

    function drawRoundedRectangle(context, x, y, width, height, radius) {
      const cornerRadius = Math.min(radius, width / 2, height / 2);
      context.beginPath();
      context.moveTo(x + cornerRadius, y);
      context.arcTo(x + width, y, x + width, y + height, cornerRadius);
      context.arcTo(x + width, y + height, x, y + height, cornerRadius);
      context.arcTo(x, y + height, x, y, cornerRadius);
      context.arcTo(x, y, x + width, y, cornerRadius);
      context.closePath();
    }

    function drawScreenshotOverlay(context, canvas) {
      if (cameraOverlay.hidden) return;

      const cameraContent = cameraImage.parentElement;
      const contentRect = cameraContent.getBoundingClientRect();
      const scaleX = canvas.width / contentRect.width;
      const scaleY = canvas.height / contentRect.height;
      const width = contentRect.width;
      const height = contentRect.height;
      context.save();
      context.scale(scaleX, scaleY);

      const overlayText = cameraOverlay.querySelectorAll(
        '.overlay-badge, .overlay-kicker, .overlay-location strong, #captureDate, #captureTime'
      );
      for (const element of overlayText) {
        if (element.hidden) continue;
        const text = element.textContent.trim();
        if (!text) continue;
        const box = getScreenshotBox(element, contentRect);
        const style = getComputedStyle(element);
        drawRoundedRectangle(
          context,
          box.x,
          box.y,
          box.width,
          box.height,
          Number.parseFloat(style.borderTopLeftRadius) || 0
        );
        context.fillStyle = style.backgroundColor;
        context.fill();
        const borderWidth = Number.parseFloat(style.borderTopWidth) || 0;
        if (borderWidth > 0) {
          context.lineWidth = borderWidth;
          context.strokeStyle = style.borderTopColor;
          context.stroke();
        }
        const fontSize = Number.parseFloat(style.fontSize);
        context.font = `${style.fontStyle} ${style.fontWeight} ${fontSize}px ${style.fontFamily}`;
        context.fillStyle = style.color;
        context.textBaseline = 'middle';
        context.textAlign = style.textAlign === 'right'
          ? 'right'
          : style.textAlign === 'center'
            ? 'center'
            : 'left';
        const leftInset =
          Number.parseFloat(style.paddingLeft) + Number.parseFloat(style.borderLeftWidth);
        const rightInset =
          Number.parseFloat(style.paddingRight) + Number.parseFloat(style.borderRightWidth);
        const contentLeft = box.x + leftInset;
        const contentRight = box.x + box.width - rightInset;
        const displayText = style.textTransform === 'uppercase' ? text.toUpperCase() : text;
        const characters = Array.from(displayText);
        const letterSpacing = Number.parseFloat(style.letterSpacing) || 0;
        const textWidth = letterSpacing === 0
          ? context.measureText(displayText).width
          : characters.reduce(
            (width, character) => width + context.measureText(character).width,
            0
          ) + letterSpacing * Math.max(0, characters.length - 1);
        let x = contentLeft;
        if (context.textAlign === 'right') x = contentRight - textWidth;
        if (context.textAlign === 'center') {
          x = (contentLeft + contentRight - textWidth) / 2;
        }
        context.textAlign = 'left';
        if (!element.classList.contains('overlay-badge')) {
          context.shadowColor = 'rgba(0, 0, 0, 0.7)';
          context.shadowBlur = 3;
          context.shadowOffsetY = 1;
        }
        if (letterSpacing === 0) {
          context.fillText(displayText, x, box.y + box.height / 2);
        } else {
          for (const character of characters) {
            context.fillText(character, x, box.y + box.height / 2);
            x += context.measureText(character).width + letterSpacing;
          }
        }
        context.shadowColor = 'transparent';
        context.shadowBlur = 0;
        context.shadowOffsetY = 0;
      }
      context.restore();
    }

    async function drawMapLabelsToCanvas(context, width, height) {
      if (!mapLabels.classList.contains('is-visible')) return;

      const svgNamespace = 'http://www.w3.org/2000/svg';
      const labelsSvg = mapLabels.cloneNode(true);
      labelsSvg.setAttribute('xmlns', svgNamespace);
      labelsSvg.setAttribute('width', String(width));
      labelsSvg.setAttribute('height', String(height));
      const style = document.createElementNS(svgNamespace, 'style');
      style.textContent = `
        .map-place-line { stroke: var(--marker-color, #f0a24b); stroke-width: 0.25; opacity: 0.9; }
        .map-place-badge { fill: rgb(9 19 20 / 82%); stroke: rgb(255 255 255 / 30%); stroke-width: 0.2; }
        .map-place-dot { fill: var(--marker-color, #f0a24b); stroke: #fff; stroke-width: 0.35; }
        .map-place-text { fill: var(--text-color, #fff); stroke: rgb(8 18 19 / 90%); stroke-width: 0.65; stroke-linejoin: round; paint-order: stroke; font-family: "Segoe UI", sans-serif; font-size: 2px; font-weight: 700; }
      `;
      labelsSvg.insertBefore(style, labelsSvg.firstChild);
      const labelsBlob = new Blob(
        [new XMLSerializer().serializeToString(labelsSvg)],
        { type: 'image/svg+xml;charset=utf-8' }
      );
      const labelsUrl = URL.createObjectURL(labelsBlob);
      try {
        const labelsImage = new Image();
        await new Promise((resolve, reject) => {
          const timeoutId = window.setTimeout(
            () => reject(new Error('The map labels could not be rendered in time.')),
            10000
          );
          labelsImage.onload = () => {
            window.clearTimeout(timeoutId);
            resolve();
          };
          labelsImage.onerror = () => {
            window.clearTimeout(timeoutId);
            reject(new Error('The map labels could not be rendered.'));
          };
          labelsImage.src = labelsUrl;
        });
        context.drawImage(labelsImage, 0, 0, width, height);
      } finally {
        URL.revokeObjectURL(labelsUrl);
      }
    }

    async function saveCameraScreenshot() {
      if (!currentImageUrl || !cameraImage.naturalWidth || screenshotInProgress) return;
      screenshotInProgress = true;
      screenshotButton.disabled = true;
      try {
        const scaleX = cameraImage.naturalWidth / cameraCrop.sourceWidth;
        const scaleY = cameraImage.naturalHeight / cameraCrop.sourceHeight;
        const cropWidth = Math.round(cameraCrop.width * scaleX);
        const cropHeight = Math.round(cameraCrop.height * scaleY);
        const canvas = document.createElement('canvas');
        canvas.width = cropWidth;
        canvas.height = cropHeight;
        const context = canvas.getContext('2d');
        if (!context) throw new Error('Could not create the screenshot canvas.');

        context.drawImage(
          cameraImage,
          cameraCrop.left * scaleX,
          cameraCrop.top * scaleY,
          cameraCrop.width * scaleX,
          cameraCrop.height * scaleY,
          0,
          0,
          cropWidth,
          cropHeight
        );
        drawScreenshotOverlay(context, canvas);
        await drawMapLabelsToCanvas(context, cropWidth, cropHeight);

        const screenshotBlob = await new Promise((resolve, reject) => {
          canvas.toBlob(blob => {
            if (blob) {
              resolve(blob);
            } else {
              reject(new Error('Could not encode the screenshot.'));
            }
          }, 'image/png');
        });
        const screenshotUrl = URL.createObjectURL(screenshotBlob);
        const downloadLink = document.createElement('a');
        downloadLink.href = screenshotUrl;
        downloadLink.download = `camera-snapshot-${new Date().toISOString().replace(/[:.]/g, '-')}.png`;
        document.body.appendChild(downloadLink);
        downloadLink.click();
        downloadLink.remove();
        window.setTimeout(() => URL.revokeObjectURL(screenshotUrl), 1000);
        cameraStatus.textContent = 'Screenshot saved.';
      } catch (error) {
        console.error('Screenshot failed:', error);
        cameraStatus.textContent = 'Could not save the screenshot.';
      } finally {
        screenshotInProgress = false;
        screenshotButton.disabled = !currentImageUrl;
      }
    }

    function applyCameraCrop(naturalWidth, naturalHeight) {
      const scaleX = naturalWidth / cameraCrop.sourceWidth;
      const scaleY = naturalHeight / cameraCrop.sourceHeight;
      const left = cameraCrop.left / cameraCrop.sourceWidth;
      const top = cameraCrop.top / cameraCrop.sourceHeight;
      const width = cameraCrop.width / cameraCrop.sourceWidth;
      const height = cameraCrop.height / cameraCrop.sourceHeight;
      const imageWidth = 100 / width;
      const imageHeight = 100 / height;

      cameraFrame.style.aspectRatio = `${cameraCrop.width * scaleX} / ${cameraCrop.height * scaleY}`;
      cameraFrame.style.setProperty(
        '--camera-crop-ratio',
        String(cameraCrop.width * scaleX / (cameraCrop.height * scaleY))
      );
      cameraImage.style.left = `${-left * imageWidth}%`;
      cameraImage.style.top = `${-top * imageHeight}%`;
      cameraImage.style.width = `${imageWidth}%`;
      cameraImage.style.height = `${imageHeight}%`;
      mapLabels.setAttribute(
        'viewBox',
        `0 0 100 ${100 * cameraCrop.height * scaleY / (cameraCrop.width * scaleX)}`
      );
    }

    function clearImageRetry() {
      if (imageRetryTimer !== null) window.clearTimeout(imageRetryTimer);
      imageRetryTimer = null;
    }

    function scheduleImageRetry() {
      if (document.hidden || imageRetryTimer !== null ||
          !latestCameraStatus?.image_available ||
          latestCameraStatus.version === currentVersion) return;
      const delay = imageRetryDelayMs;
      imageRetryDelayMs = Math.min(imageRetryDelayMs * 2, 30000);
      debugLog(`image: retry scheduled in ${delay / 1000} s`);
      imageRetryTimer = window.setTimeout(() => {
        imageRetryTimer = null;
        if (document.hidden || !latestCameraStatus?.image_available) return;
        loadSnapshot(
          latestCameraStatus.version,
          latestCameraStatus.capture_epoch,
          latestStatusReceivedAt - latestCameraStatus.age_seconds * 1000
        );
      }, delay);
    }

    async function loadSnapshot(version, captureEpoch, capturedAtMs) {
      // The version restarts at 0 after a device reboot, so compare for inequality.
      if (imageRequestInProgress || version === currentVersion) return;
      clearImageRetry();
      imageRequestInProgress = true;
      const requestToken = ++imageRequestToken;
      const requestController = new AbortController();
      imageRequestController = requestController;
      let nextImageUrl = '';
      cameraActivity = 'Loading new image...';
      showCameraStatus();
      const loadStartedAt = Date.now();
      const progress = { received: 0, expected: null };
      debugLog(`image v${version}: download started`);
      try {
        const imageBlob = await fetchSnapshotBlob(
          `/camera/image.jpg?v=${version}`,
          requestController,
          progress
        );
        if (requestToken !== imageRequestToken) {
          debugLog(`image v${version}: download result discarded (superseded)`);
          return;
        }
        debugLog(
          `image v${version}: ${imageBlob.size} bytes downloaded in ` +
          `${Date.now() - loadStartedAt} ms`
        );
        const downloadMs = Date.now() - loadStartedAt;
        lastImageDownloadMs = lastImageDownloadMs
          ? Math.round((lastImageDownloadMs * 3 + downloadMs) / 4)
          : downloadMs;

        nextImageUrl = URL.createObjectURL(imageBlob);
        const decodedImage = new Image();
        await new Promise((resolve, reject) => {
          const decodeTimeoutId = window.setTimeout(
            () => reject(new Error('The snapshot could not be decoded in time.')),
            10000
          );
          decodedImage.onload = () => {
            window.clearTimeout(decodeTimeoutId);
            resolve();
          };
          decodedImage.onerror = () => {
            window.clearTimeout(decodeTimeoutId);
            reject(new Error('The snapshot is not a valid image.'));
          };
          decodedImage.src = nextImageUrl;
        });
        if (requestToken !== imageRequestToken) {
          debugLog(`image v${version}: decoded image discarded (superseded)`);
          URL.revokeObjectURL(nextImageUrl);
          nextImageUrl = '';
          return;
        }

        const previousImageUrl = currentImageUrl;
        applyCameraCrop(decodedImage.naturalWidth, decodedImage.naturalHeight);
        cameraImage.src = nextImageUrl;
        currentImageUrl = nextImageUrl;
        nextImageUrl = '';
        currentVersion = version;
        imageRetryDelayMs = 2000;
        clearImageRetry();
        displayedCapturedAtMs = capturedAtMs;
        cameraActivity = '';
        showCameraStatus();
        debugLog(`image v${version}: shown after ${Date.now() - loadStartedAt} ms in total`);
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
        console.error('Camera snapshot load failed:', error);
        debugLog(
          `image v${version}: FAILED after ${Date.now() - loadStartedAt} ms: ` +
          `${describeError(error)} (${describeBrowserState()}` +
          `${requestToken === imageRequestToken ? '' : ', superseded'}), ` +
          `received ${progress.received}/${progress.expected ?? '?'} bytes`
        );
        if (requestToken === imageRequestToken) {
          cameraActivity = 'New image could not be loaded; retrying...';
          showCameraStatus();
          scheduleImageRetry();
        }
      } finally {
        if (requestToken === imageRequestToken) {
          imageRequestInProgress = false;
          imageRequestController = null;
        }
      }
    }

    async function refreshCameraStatus() {
      if (document.hidden) return null;
      if (statusRequestInProgress) {
        if (!skipLogged) {
          skipLogged = true;
          debugLog('status: poll skipped, previous request is still running');
        }
        return null;
      }
      skipLogged = false;
      statusRequestInProgress = true;
      const requestToken = ++statusRequestToken;
      const requestController = new AbortController();
      statusRequestController = requestController;
      try {
        const requestStartedAt = Date.now();
        let status;
        try {
          status = await fetchWithTimeout(
            '/camera/api/status',
            { cache: 'no-store' },
            10000,
            async response => {
              if (!response.ok) throw new Error(`HTTP ${response.status}`);
              return response.json();
            },
            requestController
          );
        } catch (error) {
          if (requestToken !== statusRequestToken) {
            debugLog('status: request abandoned (superseded)');
            return null;
          }
          console.error('Camera status request failed:', error);
          debugLog(
            `status: FAILED after ${Date.now() - requestStartedAt} ms: ` +
            `${describeError(error)} (${describeBrowserState()})`
          );
          cameraActivity = 'Status unavailable; retrying...';
          showCameraStatus();
          return null;
        }
        if (requestToken !== statusRequestToken) {
          debugLog('status: result discarded (superseded)');
          return null;
        }
        debugLog(
          `status: ok in ${Date.now() - requestStartedAt} ms, ` +
          `state=${status.status}, error=${status.error}, camera http=${status.http_status}, ` +
          `image=${status.image_available}, version=${status.version} ` +
          `(shown: ${currentVersion}), age=${status.age_seconds} s, ` +
          `next level=${status.resolution_level}, camera fetch=${status.last_fetch_ms} ms/` +
          `${status.last_fetch_bytes} bytes, download timeout=${status.download_timeout_ms} ms`
        );
        renderCameraStatus(status);
        if (!status.image_available) {
          clearImageRetry();
          imageRetryDelayMs = 2000;
          ++imageRequestToken;
          imageRequestController?.abort();
          imageRequestController = null;
          imageRequestInProgress = false;
          cameraImage.hidden = true;
          cameraPlaceholder.hidden = false;
          cameraOverlay.hidden = true;
          if (currentImageUrl) URL.revokeObjectURL(currentImageUrl);
          currentImageUrl = '';
          displayedCapturedAtMs = null;
          cameraActivity = '';
          showCameraStatus();
          updateLabelsButton();
          currentVersion = status.version;
          return status;
        }
        if (status.version !== currentVersion && imageRetryTimer === null) {
          loadSnapshot(
            status.version,
            status.capture_epoch,
            latestStatusReceivedAt - status.age_seconds * 1000
          );
        }
        return status;
      } catch (error) {
        // Not a network problem: something went wrong while processing a reply.
        console.error('Camera page error:', error);
        debugLog(`page error while processing status: ${describeError(error)}`);
        return null;
      } finally {
        if (requestToken === statusRequestToken) {
          statusRequestInProgress = false;
          statusRequestController = null;
        }
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

    screenshotButton.addEventListener('click', saveCameraScreenshot);
    cameraImage.addEventListener('contextmenu', event => event.preventDefault());
    fullscreenButton.addEventListener('click', toggleFullscreen);
    fullscreenExitButton.addEventListener('click', toggleFullscreen);
    labelsButton.addEventListener('click', () => {
      labelsEnabled = !labelsEnabled;
      updateLabelsButton();
    });

    document.addEventListener('fullscreenchange', updateFullscreenButton);
    window.addEventListener('resize', () => {
      if (currentImageUrl && labelsEnabled) layoutMapLabels();
    });
    document.addEventListener('visibilitychange', () => {
      debugLog(`tab became ${document.visibilityState}`);
      if (document.hidden) {
        clearImageRetry();
        return;
      }

      if (statusRequestInProgress || imageRequestInProgress) {
        debugLog('tab visible again: restarting requests that were still running');
      }
      ++statusRequestToken;
      statusRequestController?.abort();
      statusRequestInProgress = false;
      ++imageRequestToken;
      imageRequestController?.abort();
      imageRequestInProgress = false;
      refreshCameraStatus();
      scheduleImageRetry();
    });
    window.addEventListener('online', () => debugLog('browser reports: network online'));
    window.addEventListener('offline', () => debugLog('browser reports: network offline'));
    if (!fullscreenSupported) {
      fullscreenButton.disabled = true;
      fullscreenExitButton.disabled = true;
      fullscreenButton.title = 'Fullscreen is not supported by this browser.';
      fullscreenStatus.textContent = 'Fullscreen is not supported by this browser.';
      fullscreenStatus.hidden = false;
    }
    updateFullscreenButton();
    debugLog(`page started (${describeBrowserState()}); open the Console in F12 to follow these messages`);
    synchronizeDeviceTime();
    showCameraStatus();
    refreshCameraStatus();
    window.setInterval(refreshCameraStatus, 5000);
    window.setInterval(updateCameraStatusAge, 1000);
  </script>
</body>
</html>
)AREDN_CAMERA";
