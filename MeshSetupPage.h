#pragma once
#include <Arduino.h>

static const char MESH_SETUP_PAGE[] PROGMEM = R"MESHSETUP(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Mesh Invaders highscores · Setup</title>
  <style>
    body { margin: 0; background: #f3f5ef; color: #172722; font: 16px "Segoe UI", sans-serif; }
    header { padding: 28px 20px; color: white; background: #173b33; border-bottom: 5px solid #c55b36; }
    header a { color: white; } h1 { margin: 12px 0 0; }
    main { max-width: 720px; margin: auto; padding: 24px 20px; }
    section { padding: 20px; background: white; border: 1px solid #d7ded7; border-radius: 12px; }
    table { width: 100%; border-collapse: collapse; } th, td { padding: 8px; border-bottom: 1px solid #ddd; text-align: left; }
    button, input { font: inherit; margin: 8px 8px 0 0; } button { padding: 10px 16px; cursor: pointer; }
    .status { min-height: 1.5em; } .error { color: #a2382d; }
  </style>
</head>
<body>
  <header><a href="/setup">← Setup</a><h1>Mesh Invaders highscores</h1></header>
  <main><section>
    <p>Back up or restore the top ten players. Import replaces the current list.</p>
    <table><thead><tr><th>Rank</th><th>Player</th><th>Score</th></tr></thead><tbody id="scores"></tbody></table>
    <p id="empty" hidden>No scores yet.</p>
    <button id="export" type="button">Export JSON</button>
    <label for="file">Backup file:</label><input id="file" type="file" accept=".json,application/json">
    <button id="import" type="button">Import JSON</button>
    <button id="clear" type="button">Clear scores</button>
    <p id="status" class="status" role="status"></p>
  </section></main>
  <script>
    const scores = document.getElementById('scores');
    const status = document.getElementById('status');
    function showStatus(message, error = false) { status.textContent = message; status.classList.toggle('error', error); }
    function validate(data) {
      if (!data || data.version !== 1 || !Array.isArray(data.scores) || data.scores.length > 10) throw new Error('Invalid backup format.');
      const names = new Set();
      let previous = 1000000000;
      for (const entry of data.scores) {
        if (!entry || typeof entry.name !== 'string' || !/^[A-Za-z0-9_.-]{1,16}$/.test(entry.name) ||
            !Number.isInteger(entry.score) || entry.score < 0 || entry.score > previous ||
            names.has(entry.name.toLowerCase())) throw new Error('Invalid, duplicate, or unsorted score entry.');
        names.add(entry.name.toLowerCase());
        previous = entry.score;
      }
      return data;
    }
    function render(data) {
      scores.replaceChildren();
      document.getElementById('empty').hidden = data.scores.length !== 0;
      data.scores.forEach((entry, index) => {
        const row = document.createElement('tr');
        [index + 1, entry.name, entry.score].forEach(value => {
          const cell = document.createElement('td'); cell.textContent = value; row.appendChild(cell);
        });
        scores.appendChild(row);
      });
    }
    async function readScores() {
      const response = await fetch('/mesh-invaders/api/scores', { cache: 'no-store' });
      if (!response.ok) throw new Error('Unable to load highscores (HTTP ' + response.status + ').');
      const data = validate(await response.json()); render(data); return data;
    }
    async function replace(data) {
      const body = new URLSearchParams({ count: String(data.scores.length) });
      data.scores.forEach((entry, i) => { body.set('name' + i, entry.name); body.set('score' + i, String(entry.score)); });
      const response = await fetch('/setup/api/mesh-invaders/scores', { method: 'POST', body });
      if (!response.ok) throw new Error('Unable to save highscores (HTTP ' + response.status + ').');
      render(validate(await response.json()));
    }
    document.getElementById('export').addEventListener('click', async () => {
      try {
        const data = await readScores();
        const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2) + '\n'], { type: 'application/json' }));
        const link = document.createElement('a'); link.href = url; link.download = 'mesh-invaders-scores.json'; link.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
        showStatus('Backup downloaded.');
      } catch (error) { showStatus(error.message, true); }
    });
    document.getElementById('import').addEventListener('click', async () => {
      try {
        const file = document.getElementById('file').files[0];
        if (!file || file.size > 8192) throw new Error('Choose a JSON backup no larger than 8 KB.');
        const data = validate(JSON.parse(await file.text()));
        if (!confirm('Replace all current highscores with this backup?')) return;
        await replace(data); showStatus('Highscores restored.');
      } catch (error) { showStatus('Import failed: ' + error.message, true); }
    });
    document.getElementById('clear').addEventListener('click', async () => {
      if (!confirm('Delete all Mesh Invaders highscores?')) return;
      try { await replace({ version: 1, scores: [] }); showStatus('Highscores cleared.'); }
      catch (error) { showStatus(error.message, true); }
    });
    readScores().catch(error => showStatus(error.message, true));
  </script>
</body>
</html>
)MESHSETUP";
