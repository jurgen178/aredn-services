#pragma once

#include <Arduino.h>

static const char SETUP_PAGE[] PROGMEM = R"AREDN_SETUP(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#173b33">
  <title>Setup · AREDN Server</title>
  <style>
    :root { color-scheme: light; font-family: "Segoe UI", sans-serif; color: #172722; background: #f3f5ef; }
    * { box-sizing: border-box; }
    body { min-width: 320px; min-height: 100vh; margin: 0; background-image: radial-gradient(#cbd5cc 0.65px, transparent 0.65px); background-size: 16px 16px; }
    header { padding: 32px 20px; color: #f6f7f2; background: #173b33; border-bottom: 5px solid #c55b36; }
    .header-inner, main { width: min(760px, 100%); margin: 0 auto; }
    .eyebrow { margin: 0 0 6px; color: #c8d9ce; font-size: 0.76rem; font-weight: 700; letter-spacing: 0.08em; text-transform: uppercase; }
    h1 { margin: 0; font-size: clamp(1.9rem, 7vw, 2.7rem); }
    header p:last-child { margin: 10px 0 0; color: #d8e3dc; line-height: 1.5; }
    main { padding: 28px 20px; }
    .settings { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 260px), 1fr)); gap: 16px; }
    .setting { display: block; min-height: 150px; padding: 22px; color: inherit; text-decoration: none; background: #fff; border: 1px solid #d7ded7; border-radius: 12px; box-shadow: 0 8px 24px rgb(23 39 34 / 7%); transition: transform 140ms ease, border-color 140ms ease; }
    .setting:hover, .setting:focus-visible { border-color: #173b33; transform: translateY(-2px); }
    .setting:focus-visible { outline: 3px solid #c55b36; outline-offset: 3px; }
    h2 { margin: 0 0 8px; color: #173b33; font-size: 1.25rem; }
    .setting p { margin: 0; color: #5c6e66; line-height: 1.5; }
    footer { margin-top: 22px; color: #5c6e66; font-size: 0.88rem; }
  </style>
</head>
<body>
  <header>
    <div class="header-inner">
      <p class="eyebrow">AREDN · Device configuration</p>
      <h1>Setup</h1>
      <p>Choose what you want to configure on this server.</p>
    </div>
  </header>
  <main>
    <nav class="settings" aria-label="Setup options">
      <a class="setting" href="/setup/wifi">
        <h2>Wi-Fi network</h2>
        <p>Connect the server to the AREDN network or update its Wi-Fi settings.</p>
      </a>
      <a class="setting" href="/setup/camera">
        <h2>Reolink camera</h2>
        <p>Set the camera address and credentials, and check the snapshot connection.</p>
      </a>
    </nav>
    <footer>This setup page is available only while connected to the device setup access point.</footer>
  </main>
</body>
</html>
)AREDN_SETUP";
