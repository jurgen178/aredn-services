#pragma once

#include <Arduino.h>

static const char CAMERA_PAGE[] PROGMEM = R"AREDN_CAMERA(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#173b33">
  <title>Camera · AREDN Server</title>
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
    .eyebrow {
      margin: 0 0 7px;
      color: #c8d9ce;
      font-size: 0.76rem;
      font-weight: 700;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }
    .eyebrow-link {
      color: inherit;
      text-decoration: none;
    }
    .eyebrow-link:hover, .eyebrow-link:focus-visible {
      text-decoration: underline;
      text-underline-offset: 3px;
    }
    .eyebrow-link:focus-visible { outline: 2px solid #e2a34c; outline-offset: 3px; }
    h1 { margin: 0; font-size: clamp(1.8rem, 6vw, 2.5rem); }
    .intro { margin: 8px 0 0; color: #d8e3dc; }
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
      aspect-ratio: 16 / 9;
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
      aspect-ratio: 16 / 9;
      background-color: #192b2a;
      background-image:
        linear-gradient(rgb(255 255 255 / 5%) 1px, transparent 1px),
        linear-gradient(90deg, rgb(255 255 255 / 5%) 1px, transparent 1px);
      background-size: 32px 32px;
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
      width: min(100vw, 177.7778vh);
      height: min(100vh, 56.25vw);
    }
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
    .overlay {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
    }
    .overlay line {
      fill: none;
      stroke: #f0a24b;
      stroke-width: 0.5;
      vector-effect: non-scaling-stroke;
    }
    .overlay-label {
      position: absolute;
      left: var(--x);
      top: var(--y);
      padding: 3px 6px;
      color: #fff3dd;
      font-size: clamp(0.68rem, 1.5vw, 0.85rem);
      font-weight: 700;
      white-space: nowrap;
      background: rgb(23 39 34 / 78%);
      border-left: 2px solid #f0a24b;
      pointer-events: none;
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
        <p class="eyebrow"><a class="eyebrow-link" href="/" title="Open the AREDN services portal">AREDN · Bellevue Downtown View</a></p>
        <h1>Camera</h1>
        <p class="intro">A still image with lightweight browser-side overlays.</p>
      </div>
    </div>
  </header>
  <main>
    <section class="panel" aria-labelledby="previewTitle">
      <div class="panel-head">
        <h2 id="previewTitle">Camera preview</h2>
        <div class="panel-actions">
          <p id="cameraStatus" class="status" role="status">Camera not configured</p>
          <button id="fullscreenButton" class="fullscreen-button" type="button">Fullscreen</button>
        </div>
      </div>
      <div id="cameraFrame" class="camera-frame" role="region" aria-label="Camera preview">
        <button id="fullscreenExitButton" class="fullscreen-button fullscreen-exit-button" type="button" hidden>Exit fullscreen</button>
        <div class="camera-content">
          <div class="camera-placeholder">
            <strong>Camera connection pending</strong>
            <span>This preview will show the latest JPEG snapshot.</span>
          </div>
          <svg class="overlay" viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden="true">
            <line x1="10%" y1="30%" x2="90%" y2="30%"></line>
            <line x1="10%" y1="70%" x2="90%" y2="80%"></line>
          </svg>
          <span class="overlay-label" style="--x: 10%; --y: 32%;">TEXT 1</span>
          <span class="overlay-label" style="--x: 60%; --y: 62%;">TEXT 2</span>
        </div>
      </div>
      <p id="fullscreenStatus" class="fullscreen-status" role="status" hidden></p>
      <p class="note">Overlay positions are percentages from the image's top-left corner (0%, 0%). For labels, edit <code>--x</code> and <code>--y</code>; for lines, edit <code>x1</code>/<code>y1</code> and <code>x2</code>/<code>y2</code>. The overlay is drawn in the browser, not into the camera image.</p>
    </section>
  </main>
  <script>
    const cameraFrame = document.getElementById('cameraFrame');
    const fullscreenButton = document.getElementById('fullscreenButton');
    const fullscreenExitButton = document.getElementById('fullscreenExitButton');
    const fullscreenStatus = document.getElementById('fullscreenStatus');
    const fullscreenSupported =
      document.fullscreenEnabled !== false &&
      typeof cameraFrame.requestFullscreen === 'function' &&
      typeof document.exitFullscreen === 'function';

    function updateFullscreenButton() {
      const isFullscreen = document.fullscreenElement === cameraFrame;
      fullscreenButton.hidden = isFullscreen;
      fullscreenExitButton.hidden = !isFullscreen;
      fullscreenButton.setAttribute('aria-label', 'View camera fullscreen');
      fullscreenButton.setAttribute('aria-pressed', String(isFullscreen));
      fullscreenExitButton.setAttribute('aria-label', 'Exit camera fullscreen');
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

    fullscreenButton.addEventListener('click', toggleFullscreen);
    fullscreenExitButton.addEventListener('click', toggleFullscreen);

    document.addEventListener('fullscreenchange', updateFullscreenButton);
    if (!fullscreenSupported) {
      fullscreenButton.disabled = true;
      fullscreenExitButton.disabled = true;
      fullscreenButton.title = 'Fullscreen is not supported by this browser.';
      fullscreenStatus.textContent = 'Fullscreen is not supported by this browser.';
      fullscreenStatus.hidden = false;
    }
    updateFullscreenButton();
  </script>
</body>
</html>
)AREDN_CAMERA";
