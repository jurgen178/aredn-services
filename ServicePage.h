#pragma once

#include <Arduino.h>

static const char SERVICE_PAGE[] PROGMEM = R"AREDNSERVICE(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#173b33">
  <title>AREDN Server</title>
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
      padding: 38px 24px 34px;
      color: #f6f7f2;
      background: #173b33;
      border-bottom: 5px solid #c55b36;
    }
    .header-inner, main {
      width: min(900px, 100%);
      margin: 0 auto;
    }
    .header-label {
      margin: 0 0 8px;
      color: #c8d9ce;
      font-size: 0.76rem;
      font-weight: 700;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }
    h1 { margin: 0; font-size: clamp(2rem, 7vw, 3rem); }
    header p:last-child {
      max-width: 55ch;
      margin: 12px 0 0;
      color: #d8e3dc;
      line-height: 1.5;
    }
    header p.experimental-note {
      margin: 8px 0 0;
      color: #e2a34c;
      font-size: 0.72rem;
      line-height: 1.35;
    }
    .experimental-note a { color: inherit; }
    main { padding: 34px 24px; }
    .services {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr));
      gap: 18px;
    }
    .service {
      display: block;
      min-height: 190px;
      padding: 24px;
      color: inherit;
      text-decoration: none;
      background: #fff;
      border: 1px solid #d7ded7;
      border-radius: 16px;
      box-shadow: 0 8px 24px rgb(23 39 34 / 7%);
      transition: transform 140ms ease, border-color 140ms ease;
    }
    .service:hover, .service:focus-visible {
      border-color: #173b33;
      transform: translateY(-2px);
    }
    .service:focus-visible { outline: 3px solid #c55b36; outline-offset: 3px; }
    .service-mark {
      display: inline-grid;
      width: 38px;
      height: 38px;
      place-items: center;
      color: #173b33;
      font-size: 1.1rem;
      font-weight: 700;
      background: #e4eee8;
      border-radius: 11px;
    }
    h2 { margin: 20px 0 8px; font-size: 1.3rem; }
    .service p { margin: 0; color: #5c6e66; line-height: 1.5; }
    footer {
      margin-top: 28px;
      color: #5c6e66;
      font-size: 0.85rem;
    }
  </style>
</head>
<body>
  <header>
    <div class="header-inner">
      <p class="header-label">AREDN · Local services</p>
      <h1>AREDN Server</h1>
      <p>Choose a service available on this local AREDN node.</p>
      <p class="experimental-note">EXPERIMENTAL ESP32 WEB SERVICE · <a href="https://github.com/jurgen178/aredn-services" target="_blank" rel="noopener noreferrer">Project on GitHub</a></p>
    </div>
  </header>
  <main>
    <nav class="services" aria-label="Available services">
      <a class="service" href="/msg-board">
        <span class="service-mark" aria-hidden="true">M</span>
        <h2>Message Board</h2>
        <p>Read and post local network messages.</p>
      </a>
      <a class="service" href="/camera">
        <span class="service-mark" aria-hidden="true">C</span>
        <h2>Camera Snapshot</h2>
        <p>View the latest image from Bellevue Downtown, refreshed about every 30 seconds.</p>
      </a>
      <a class="service" href="/tools">
        <span class="service-mark" aria-hidden="true">T</span>
        <h2>Radio Tools</h2>
        <p>Calculate Maidenhead locator distances and radio link budgets.</p>
      </a>
    </nav>
    <footer>Services are hosted locally by the AREDN Server.</footer>
  </main>
  <script>
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

    synchronizeDeviceTime();
  </script>
</body>
</html>
)AREDNSERVICE";
