#pragma once

#include <Arduino.h>

static const char TOOLS_PAGE[] PROGMEM = R"AREDN_TOOLS(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#173b33">
  <title>Radio Tools · AREDN Services</title>
  <style>
    :root { color-scheme: light; font-family: "Trebuchet MS", "Segoe UI", sans-serif; color: #172722; background: #f3f5ef; }
    * { box-sizing: border-box; }
    body { min-width: 320px; min-height: 100vh; margin: 0; background-image: radial-gradient(#cbd5cc 0.65px, transparent 0.65px); background-size: 16px 16px; }
    header { padding: 32px 20px; color: #f6f7f2; background: #173b33; border-bottom: 5px solid #c55b36; }
    .header-inner, main { width: min(900px, 100%); margin: 0 auto; }
    .header-label { margin: 0 0 7px; color: #c8d9ce; font-size: 0.76rem; font-weight: 700; letter-spacing: 0.08em; text-transform: uppercase; }
    .header-label-link { color: inherit; text-decoration: none; }
    .header-label-link:hover, .header-label-link:focus-visible { text-decoration: underline; text-underline-offset: 3px; }
    .header-label-link:focus-visible { outline: 2px solid #e2a34c; outline-offset: 3px; }
    h1 { margin: 0; font-size: clamp(1.9rem, 7vw, 2.7rem); }
    header p:last-child { margin: 10px 0 0; color: #d8e3dc; line-height: 1.5; }
    header p.experimental-note { margin: 8px 0 0; color: #e2a34c; font-size: 0.72rem; line-height: 1.35; }
    .experimental-note a { color: inherit; }
    main { padding: 28px 20px; }
    .tool-panel { margin-bottom: 22px; padding: 22px; border: 1px solid #d7ded7; border-radius: 14px; background: #fff; box-shadow: 0 8px 24px rgb(23 39 34 / 7%); }
    h2 { margin: 0; color: #173b33; font-size: 1.35rem; }
    .intro { margin: 8px 0 18px; color: #5c6e66; line-height: 1.5; }
    .field-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 220px), 1fr)); gap: 14px; }
    label { display: block; margin-bottom: 6px; font-weight: 700; }
    input { width: 100%; min-height: 44px; padding: 9px 11px; border: 1px solid #aebdb3; border-radius: 6px; font: inherit; }
    input:focus-visible, button:focus-visible { outline: 3px solid #c55b36; outline-offset: 2px; }
    .hint, .model-note { color: #5c6e66; font-size: 0.9rem; line-height: 1.5; }
    .hint { margin: 8px 0 0; }
    .model-note { margin: 12px 0 0; }
    button { min-height: 44px; margin-top: 18px; padding: 0 17px; border: 0; border-radius: 6px; color: #fff; background: #173b33; font: inherit; font-weight: 700; cursor: pointer; }
    button:hover { background: #245447; }
    .status { min-height: 24px; margin: 14px 0 0; color: #5c6e66; }
    .status.error { color: #a2382d; }
    .results { margin-top: 16px; }
    .results h3 { margin: 0 0 10px; color: #173b33; font-size: 1rem; }
    .result-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 180px), 1fr)); gap: 10px; margin: 0; }
    .result-grid div { min-width: 0; padding: 12px; border: 1px solid #d7ded7; border-radius: 8px; background: #f3f7f3; }
    dt { color: #5c6e66; font-size: 0.85rem; }
    dd { margin: 5px 0 0; color: #173b33; font-size: 1.1rem; font-weight: 700; font-variant-numeric: tabular-nums; overflow-wrap: anywhere; }
    [hidden] { display: none !important; }
    @media (prefers-reduced-motion: reduce) {
      *, *::before, *::after { scroll-behavior: auto !important; }
    }
  </style>
</head>
<body>
  <header>
    <div class="header-inner">
      <p class="header-label"><a class="header-label-link" href="/" title="Open the AREDN services portal">AREDN · Radio Tools</a></p>
      <h1>Radio Tools</h1>
      <p>Small calculators for planning and operating radio links.</p>
      <p class="experimental-note">EXPERIMENTAL ESP32 WEB SERVICE · <a href="https://github.com/jurgen178/aredn-services" target="_blank" rel="noopener noreferrer">Project on GitHub</a></p>
    </div>
  </header>
  <main>
    <section id="maidenhead" class="tool-panel" aria-labelledby="maidenhead-title">
      <h2 id="maidenhead-title">Maidenhead Locator</h2>
      <p class="intro">Calculate distance and initial bearing between two locator centers. Actual stations may be anywhere within their grid squares.</p>
      <form id="locator-form" novalidate>
        <div class="field-grid">
          <div>
            <label for="locator-one">Station 1 locator</label>
            <input id="locator-one" name="locatorOne" type="text" minlength="2" maxlength="12" autocomplete="off" autocapitalize="characters" placeholder="CN87" required>
          </div>
          <div>
            <label for="locator-two">Station 2 locator</label>
            <input id="locator-two" name="locatorTwo" type="text" minlength="2" maxlength="12" autocomplete="off" autocapitalize="characters" placeholder="EN82" required>
          </div>
        </div>
        <p class="hint">Use an even number of characters from 2 to 12, for example CN87 or CN87xi.</p>
        <button type="submit">Calculate distance</button>
      </form>
      <p id="locator-status" class="status" role="status" aria-live="polite"></p>
      <section id="locator-results" class="results" aria-labelledby="locator-results-title" hidden>
        <h3 id="locator-results-title">Results based on locator centers</h3>
        <dl class="result-grid">
          <div><dt>Distance</dt><dd id="locator-distance"></dd></div>
          <div><dt>Initial bearing, station 1 to station 2</dt><dd id="locator-bearing"></dd></div>
          <div><dt>Station 1 center</dt><dd id="locator-center-one"></dd></div>
          <div><dt>Station 2 center</dt><dd id="locator-center-two"></dd></div>
        </dl>
      </section>
    </section>

    <section id="link-budget" class="tool-panel" aria-labelledby="link-budget-title">
      <h2 id="link-budget-title">Link Budget Calculator</h2>
      <p class="intro">Estimate free-space path loss, received signal level, and link margin from radio and antenna specifications.</p>
      <form id="link-form" novalidate>
        <div class="field-grid">
          <div>
            <label for="frequency">Frequency (MHz)</label>
            <input id="frequency" name="frequency" type="number" min="0" step="any" placeholder="5800" required>
          </div>
          <div>
            <label for="distance">Path distance (km)</label>
            <input id="distance" name="distance" type="number" min="0" step="any" placeholder="10" required>
          </div>
          <div>
            <label for="tx-power">Transmitter power (dBm)</label>
            <input id="tx-power" name="txPower" type="number" step="any" placeholder="20" required>
          </div>
          <div>
            <label for="tx-gain">Transmit antenna gain (dBi)</label>
            <input id="tx-gain" name="txGain" type="number" step="any" placeholder="23" required>
          </div>
          <div>
            <label for="tx-loss">Transmit feedline loss (dB)</label>
            <input id="tx-loss" name="txLoss" type="number" min="0" step="any" placeholder="1" required>
          </div>
          <div>
            <label for="rx-gain">Receive antenna gain (dBi)</label>
            <input id="rx-gain" name="rxGain" type="number" step="any" placeholder="23" required>
          </div>
          <div>
            <label for="rx-loss">Receive feedline loss (dB)</label>
            <input id="rx-loss" name="rxLoss" type="number" min="0" step="any" placeholder="1" required>
          </div>
          <div>
            <label for="sensitivity">Receiver sensitivity (dBm)</label>
            <input id="sensitivity" name="sensitivity" type="number" step="any" placeholder="-90" required>
          </div>
        </div>
        <button type="submit">Calculate link budget</button>
      </form>
      <p id="link-status" class="status" role="status" aria-live="polite"></p>
      <section id="link-results" class="results" aria-labelledby="link-results-title" hidden>
        <h3 id="link-results-title">Estimated results</h3>
        <dl class="result-grid">
          <div><dt>Transmit EIRP</dt><dd id="result-eirp"></dd></div>
          <div><dt>Free-space path loss</dt><dd id="result-path-loss"></dd></div>
          <div><dt>Estimated receive level</dt><dd id="result-receive-level"></dd></div>
          <div><dt>Link margin above receiver sensitivity</dt><dd id="result-link-margin"></dd></div>
        </dl>
      </section>
      <p class="model-note">This is a free-space estimate only. It does not account for terrain, obstacles, Fresnel-zone clearance, interference, polarization, or weather.</p>
    </section>
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

    function parseLocator(value) {
      const locator = value.trim().toUpperCase();
      if (locator.length < 2 || locator.length > 12 || locator.length % 2 !== 0) {
        throw new Error('Enter an even number of locator characters from 2 to 12.');
      }

      let longitude = -180;
      let latitude = -90;
      let longitudeCell = 360;
      let latitudeCell = 180;
      for (let pair = 0; pair < locator.length / 2; pair += 1) {
        const longitudeCharacter = locator[pair * 2];
        const latitudeCharacter = locator[pair * 2 + 1];
        const divisor = pair === 0 ? 18 : pair % 2 === 1 ? 10 : 24;
        longitudeCell /= divisor;
        latitudeCell /= divisor;

        let longitudeIndex;
        let latitudeIndex;
        if (pair % 2 === 0) {
          const firstLetter = 'A'.charCodeAt(0);
          const maximumLetter = pair === 0 ? 18 : 24;
          longitudeIndex = longitudeCharacter.charCodeAt(0) - firstLetter;
          latitudeIndex = latitudeCharacter.charCodeAt(0) - firstLetter;
          if (longitudeIndex < 0 || longitudeIndex >= maximumLetter ||
              latitudeIndex < 0 || latitudeIndex >= maximumLetter) {
            throw new Error('Locator letters are outside the valid range.');
          }
        } else {
          longitudeIndex = Number(longitudeCharacter);
          latitudeIndex = Number(latitudeCharacter);
          if (!/^[0-9]$/.test(longitudeCharacter) || !/^[0-9]$/.test(latitudeCharacter)) {
            throw new Error('Locator square characters must be digits.');
          }
        }
        longitude += longitudeIndex * longitudeCell;
        latitude += latitudeIndex * latitudeCell;
      }

      return {
        latitude: latitude + latitudeCell / 2,
        longitude: longitude + longitudeCell / 2
      };
    }

    function describeCenter(center) {
      return `${center.latitude.toFixed(4)}°, ${center.longitude.toFixed(4)}°`;
    }

    document.getElementById('locator-form').addEventListener('submit', (event) => {
      event.preventDefault();
      const status = document.getElementById('locator-status');
      const results = document.getElementById('locator-results');
      status.classList.remove('error');
      try {
        const form = new FormData(event.currentTarget);
        const first = parseLocator(form.get('locatorOne'));
        const second = parseLocator(form.get('locatorTwo'));
        const radians = Math.PI / 180;
        const firstLatitude = first.latitude * radians;
        const secondLatitude = second.latitude * radians;
        const latitudeDifference = (second.latitude - first.latitude) * radians;
        const longitudeDifference = (second.longitude - first.longitude) * radians;
        const haversine = Math.min(1, Math.sin(latitudeDifference / 2) ** 2 +
          Math.cos(firstLatitude) * Math.cos(secondLatitude) *
          Math.sin(longitudeDifference / 2) ** 2);
        const distanceKm = 6371.0088 * 2 *
          Math.atan2(Math.sqrt(haversine), Math.sqrt(1 - haversine));
        const bearingY = Math.sin(longitudeDifference) * Math.cos(secondLatitude);
        const bearingX = Math.cos(firstLatitude) * Math.sin(secondLatitude) -
          Math.sin(firstLatitude) * Math.cos(secondLatitude) * Math.cos(longitudeDifference);
        const bearing = (Math.atan2(bearingY, bearingX) / radians + 360) % 360;

        document.getElementById('locator-distance').textContent = `${distanceKm.toFixed(1)} km`;
        document.getElementById('locator-bearing').textContent = distanceKm < 0.001
          ? 'Same locator center'
          : `${bearing.toFixed(0)}°`;
        document.getElementById('locator-center-one').textContent = describeCenter(first);
        document.getElementById('locator-center-two').textContent = describeCenter(second);
        status.textContent = '';
        results.hidden = false;
      } catch (error) {
        results.hidden = true;
        status.textContent = error instanceof Error ? error.message : 'Could not calculate locator distance.';
        status.classList.add('error');
      }
    });

    document.getElementById('link-form').addEventListener('submit', (event) => {
      event.preventDefault();
      const formElement = event.currentTarget;
      const status = document.getElementById('link-status');
      const results = document.getElementById('link-results');
      status.classList.remove('error');
      if (!formElement.reportValidity()) return;

      try {
        const form = new FormData(formElement);
        const value = (name) => Number(form.get(name));
        const frequencyMHz = value('frequency');
        const distanceKm = value('distance');
        const txPowerDbm = value('txPower');
        const txGainDbi = value('txGain');
        const txLossDb = value('txLoss');
        const rxGainDbi = value('rxGain');
        const rxLossDb = value('rxLoss');
        const sensitivityDbm = value('sensitivity');
        const inputs = [frequencyMHz, distanceKm, txPowerDbm, txGainDbi, txLossDb,
          rxGainDbi, rxLossDb, sensitivityDbm];
        if (!inputs.every(Number.isFinite) || frequencyMHz <= 0 || distanceKm <= 0 ||
            txLossDb < 0 || rxLossDb < 0) {
          throw new Error('Enter valid values. Frequency and distance must be positive; cable losses cannot be negative.');
        }

        const pathLossDb = 32.44 + 20 * Math.log10(frequencyMHz) + 20 * Math.log10(distanceKm);
        const eirpDbm = txPowerDbm + txGainDbi - txLossDb;
        const receiveLevelDbm = eirpDbm + rxGainDbi - rxLossDb - pathLossDb;
        const linkMarginDb = receiveLevelDbm - sensitivityDbm;
        document.getElementById('result-eirp').textContent = `${eirpDbm.toFixed(1)} dBm`;
        document.getElementById('result-path-loss').textContent = `${pathLossDb.toFixed(1)} dB`;
        document.getElementById('result-receive-level').textContent = `${receiveLevelDbm.toFixed(1)} dBm`;
        document.getElementById('result-link-margin').textContent = `${linkMarginDb.toFixed(1)} dB`;
        status.textContent = '';
        results.hidden = false;
      } catch (error) {
        results.hidden = true;
        status.textContent = error instanceof Error ? error.message : 'Could not calculate link budget.';
        status.classList.add('error');
      }
    });

    synchronizeDeviceTime();
  </script>
</body>
</html>
)AREDN_TOOLS";
