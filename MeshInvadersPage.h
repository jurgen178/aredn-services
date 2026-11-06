#pragma once
#include <Arduino.h>

static const char MESH_INVADERS_PAGE[] PROGMEM = R"MESHGAME(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <meta name="theme-color" content="#101829">
  <title>Mesh Invaders</title>
  <style>
    * { box-sizing: border-box; } [hidden] { display: none !important; }
    body { margin: 0; background: #101829; color: #d7f8e6; font: 16px "Trebuchet MS", sans-serif; }
    button, input { font: inherit; } button { padding: 10px 16px; color: #d7f8e6; background: #23513c; border: 1px solid #7df6ac; border-radius: 5px; cursor: pointer; }
    button:focus-visible, input:focus-visible, a:focus-visible { outline: 3px solid #ffd66d; outline-offset: 3px; }
    a { color: #a9eec1; } main { max-width: 680px; padding: 24px; margin: auto; }
    h1 { letter-spacing: .1em; color: #7df6ac; } h2 { margin-top: 24px; }
    .leaderboard { margin-top: 22px; padding: 18px; overflow: hidden; border: 1px solid #326d51; border-radius: 10px; background: linear-gradient(145deg, #162b3c, #0d1927); box-shadow: 0 0 24px rgb(125 246 172 / 8%), inset 0 0 22px rgb(125 246 172 / 3%); }
    .leaderboard-heading { display: flex; align-items: baseline; justify-content: space-between; gap: 12px; margin-bottom: 12px; }
    .leaderboard-heading h2 { margin: 0; color: #7df6ac; }
    .leaderboard-heading span { color: #87a997; font: 700 .7rem monospace; letter-spacing: .12em; }
    .leaderboard table { width: 100%; border-collapse: collapse; font-variant-numeric: tabular-nums; }
    .leaderboard th, .leaderboard td { padding: 10px 8px; text-align: left; border-bottom: 1px solid #294237; }
    .leaderboard th { color: #87a997; background: #0b1521; font: 700 .72rem monospace; letter-spacing: .1em; text-transform: uppercase; }
    .leaderboard th:last-child, .leaderboard .score-cell { text-align: right; }
    .leaderboard tbody tr { transition: background-color .15s ease; }
    .leaderboard tbody tr:hover { background: rgb(125 246 172 / 7%); }
    .leaderboard tbody tr:last-child td { border-bottom: 0; }
    .rank-cell { width: 64px; }
    .rank-badge { display: inline-grid; width: 30px; height: 30px; place-items: center; border: 1px solid #426354; border-radius: 6px; color: #a9eec1; font: 700 .85rem monospace; }
    .rank-1 .rank-badge { border-color: #ffd66d; color: #ffd66d; background: rgb(255 214 109 / 10%); box-shadow: 0 0 12px rgb(255 214 109 / 14%); }
    .rank-2 .rank-badge { border-color: #c7d8d1; color: #e0eee8; }
    .rank-3 .rank-badge { border-color: #c98b62; color: #e7a77b; }
    .pilot-cell { font-weight: 700; }
    .score-cell { color: #7df6ac; font: 700 1rem monospace; }
    .empty-scores { margin: 16px 0 4px; color: #a9eec1; font-style: italic; }
    #status, #save-status { min-height: 1.5em; color: #ffd66d; }
    #qrt { color: #a9eec1; font-style: italic; }
    .tagline strong { color: #7df6ac; font-size: 1.15em; }
    #game { width: 100%; height: 100vh; height: 100dvh; display: flex; flex-direction: column; padding: 8px; gap: 6px; background: #080e19; }
    #game:fullscreen { width: 100%; height: 100%; }
    .gamebar { display: flex; align-items: center; justify-content: space-between; gap: 8px; flex-wrap: wrap; }
    .gamebar button { padding: 6px 10px; }
    .playfield { flex: 1; min-height: 0; display: flex; align-items: center; justify-content: center; }
    canvas { display: block; max-width: 100%; max-height: 100%; width: auto; height: auto; aspect-ratio: 800 / 600; object-fit: contain; image-rendering: pixelated; border: 1px solid #385347; }
    .controls { display: flex; justify-content: center; gap: 14px; touch-action: none; }
    .controls button { min-width: 72px; min-height: 48px; touch-action: none; user-select: none; }
    #result { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; padding: 20px; background: rgb(0 0 0 / 80%); }
    .result-card { max-width: 420px; width: 100%; padding: 20px; background: #14283a; border: 1px solid #7df6ac; }
    .result-card form { margin-top: 18px; }
    .result-card label { display: block; margin-bottom: 22px; line-height: 1.4; }
    .result-card .score-entry { display: flex; flex-wrap: wrap; gap: 10px; }
    .result-card input { flex: 1 1 160px; min-width: 0; padding: 8px; }
    .result-card input:focus-visible { outline-offset: 1px; }
    #save-status { margin: 12px 0; }
    #game { position: relative; }
  </style>
</head>
<body>
  <main id="start-screen">
    <a href="/">← AREDN Services</a><h1>MESH INVADERS</h1>
    <p class="tagline"><strong>Defend the mesh!</strong><br><br>Move with ← / → or A / D; fire with Space. On touchscreens use the buttons below the game.</p>
    <button id="start" type="button">Start game</button>
    <section class="leaderboard" aria-labelledby="leaderboard-title">
      <div class="leaderboard-heading"><h2 id="leaderboard-title">Highscores</h2><span>TOP TEN PILOTS</span></div>
      <table><thead><tr><th scope="col">Rank</th><th scope="col">Pilot</th><th scope="col">Score</th></tr></thead><tbody id="scores"></tbody></table>
      <p id="empty" class="empty-scores" hidden>No scores yet. Be the first!</p>
    </section>
    <p id="status" role="status"></p>
  </main>
  <section id="game" aria-label="Mesh Invaders game" hidden>
    <div class="gamebar"><span id="hud"></span><span><button id="fullscreen" type="button">Fullscreen</button> <button id="quit" type="button">Quit</button></span></div>
    <div class="playfield"><canvas id="canvas" width="800" height="600" aria-label="Game field"></canvas></div>
    <div class="controls" aria-label="Game controls">
      <button type="button" data-control="left" aria-label="Move left">◀</button>
      <button type="button" data-control="fire">FIRE</button>
      <button type="button" data-control="right" aria-label="Move right">▶</button>
    </div>
    <div id="result" hidden><div class="result-card">
      <h2 id="result-title">Game over</h2><p id="final-score"></p><p id="qrt">QRT – station closed. 73!</p>
      <form id="score-form"><label for="name">Player name (1–16 letters, numbers, . _ -)</label>
        <div class="score-entry">
          <input id="name" maxlength="16" pattern="[A-Za-z0-9_.\-]{1,16}" required autocomplete="nickname">
          <button type="submit">Save score</button>
        </div>
      </form><p id="save-status" role="status"></p>
      <button id="restart" type="button" hidden>Play again</button>
      <button id="back" type="button">Back to highscores</button>
    </div></div>
  </section>
  <script>
    const $ = id => document.getElementById(id);
    const canvas = $('canvas'), ctx = canvas.getContext('2d');
    const keys = { left: false, right: false, fire: false };
    let game = null, lastFrame = 0, animation = 0, saving = false;
    function showScores(data) {
      const body = $('scores'); body.replaceChildren();
      $('empty').hidden = data.scores.length !== 0;
      data.scores.forEach((entry, index) => {
        const row = document.createElement('tr');
        row.className = index < 3 ? 'rank-' + (index + 1) : '';
        const rank = document.createElement('td'); rank.className = 'rank-cell';
        const badge = document.createElement('span'); badge.className = 'rank-badge'; badge.textContent = index + 1;
        rank.appendChild(badge); row.appendChild(rank);
        const pilot = document.createElement('td'); pilot.className = 'pilot-cell'; pilot.textContent = entry.name; row.appendChild(pilot);
        const score = document.createElement('td'); score.className = 'score-cell'; score.textContent = entry.score.toLocaleString('en-US'); row.appendChild(score);
        body.appendChild(row);
      });
    }
    async function loadScores() {
      try {
        const response = await fetch('/mesh-invaders/api/scores', { cache: 'no-store' });
        if (!response.ok) throw new Error('HTTP ' + response.status);
        showScores(await response.json()); $('status').textContent = '';
      } catch (error) { $('status').textContent = 'Highscores unavailable: ' + error.message; }
    }
    function resetControls() { keys.left = keys.right = keys.fire = false; }
    const WAVE_NAMES = [
      'CQ CQ – Station on the air', 'QRM – Interference on frequency', 'QSB – Signal fading',
      'QRN – Static crashes', 'Sunspot minimum', 'Ionospheric storm', 'Deep-space signal',
      'Rogue satellite on frequency', 'QRM in the asteroid belt', 'The final net'
    ];
    function waveBanner(level) {
      if (level === 11) return { text: 'BOSS WAVE', name: 'The Repeater', remaining: 3.5 };
      return { text: 'WAVE ' + level, name: WAVE_NAMES[level - 1] || 'Pile-up!', remaining: 2.5 };
    }
    function createWave(level) {
      const enemies = [];
      for (let row = 0; row < 4; row++) for (let col = 0; col < 9; col++)
        enemies.push({ x: 110 + col * 62, y: 90 + row * 38, w: 30, h: 22 });
      return { enemies, direction: 1, step: 0, speed: Math.min(150, 25 + level * 12) };
    }
    function createBoss() {
      return { x: 320, y: 82, w: 160, h: 66, hp: 12, maxHp: 12, direction: 1, fireTimer: 1 };
    }
    function makeShields() {
      const shields = [];
      for (let base = 125; base <= 665; base += 180)
        for (let y = 0; y < 3; y++) for (let x = 0; x < 6; x++)
          shields.push({ x: base + x * 10, y: 444 + y * 10, w: 10, h: 10, hp: 2 });
      return shields;
    }
    function startGame() {
      resetControls();
      game = { score: 0, lives: 3, level: 1, player: { x: 380, y: 555, w: 40, h: 20 },
        wave: createWave(1), shields: makeShields(), shots: [], enemyShots: [],
        fireTimer: 0, enemyTimer: 1, invulnerable: 0, bonus: null, bonusTimer: 12, bonusNotice: null,
        lastBonusKind: 'life', time: 0, banner: waveBanner(1), boss: null };
      $('start-screen').hidden = true; $('game').hidden = false; $('result').hidden = true;
      $('save-status').textContent = ''; $('score-form').hidden = false;
      lastFrame = 0; cancelAnimationFrame(animation); animation = requestAnimationFrame(frame);
    }
    function hit(a, b) { return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y; }
    function endGame(won = false) {
      game.over = true; game.won = won; resetControls();
      $('result-title').textContent = won ? 'Mesh saved!' : 'Game over';
      $('final-score').textContent = 'Score: ' + game.score;
      $('qrt').textContent = won ? '73 – You saved the mesh!' : 'QRT – station closed. 73!';
      $('restart').hidden = !won;
      $('result').hidden = false; $('name').focus();
    }
    function update(dt) {
      const g = game, p = g.player, wave = g.wave;
      p.x = Math.max(0, Math.min(800 - p.w, p.x + ((keys.right ? 1 : 0) - (keys.left ? 1 : 0)) * 300 * dt));
      g.invulnerable = Math.max(0, g.invulnerable - dt);
      g.fireTimer -= dt; g.enemyTimer -= dt; g.bonusTimer -= dt; g.time += dt;
      if (g.banner) {
        g.banner.remaining -= dt;
        if (g.banner.remaining <= 0) g.banner = null;
      }
      // One shot per key press or tap; holding the button does not auto-fire.
      if (keys.fire) { keys.fire = false; g.shots.push({ x: p.x + 18, y: p.y - 12, w: 4, h: 12 }); }
      if (g.bonusNotice) {
        g.bonusNotice.remaining -= dt;
        if (g.bonusNotice.remaining <= 0) g.bonusNotice = null;
      }
      if (!g.bonus && g.bonusTimer <= 0) {
        // From wave 4 the friendly 73 UFO alternates with the HTTPS UFO.
        const kind = g.level >= 4 && g.lastBonusKind === 'https' ? 'life' : 'https';
        g.lastBonusKind = kind;
        g.bonus = { x: 800, y: 38, w: 100, h: 32, kind };
      }
      if (g.bonus) {
        g.bonus.x -= 100 * dt;
        if (g.bonus.x + g.bonus.w < 0) { g.bonus = null; g.bonusTimer = 18 + Math.random() * 12; }
      }
      if (g.boss) {
        const boss = g.boss;
        boss.x += boss.direction * 100 * dt;
        if (boss.x < 28 || boss.x + boss.w > 772) boss.direction *= -1;
        boss.fireTimer -= dt;
        if (boss.fireTimer <= 0) {
          for (const offset of [28, 80, 132])
            g.enemyShots.push({ x: boss.x + offset, y: boss.y + boss.h, w: 7, h: 14 });
          boss.fireTimer = 1.1;
        }
      }
      const dx = wave.direction * wave.speed * dt;
      if (wave.enemies.some(e => e.x + dx < 12 || e.x + e.w + dx > 788)) {
        wave.direction *= -1; wave.enemies.forEach(e => { e.y += 15; });
      } else wave.enemies.forEach(e => { e.x += dx; });
      if (wave.enemies.some(e => e.y + e.h >= p.y)) { endGame(); return; }
      if (g.enemyTimer <= 0 && wave.enemies.length) {
        const e = wave.enemies[Math.floor(Math.random() * wave.enemies.length)];
        g.enemyShots.push({ x: e.x + e.w / 2, y: e.y + e.h, w: 5, h: 12 });
        g.enemyTimer = Math.max(.32, 1.1 - g.level * .06);
      }
      for (const shot of g.shots) {
        shot.y -= 420 * dt;
        for (const shield of g.shields) if (shield.hp && hit(shot, shield)) { shield.hp--; shot.dead = true; break; }
        if (!shot.dead && g.bonus && hit(shot, g.bonus)) {
          shot.dead = true;
          let text = '+100';
          if (g.bonus.kind === 'life') {
            if (g.lives < 5) { g.lives++; text = '+1 LIFE'; } else { g.score += 50; text = '+50'; }
          } else g.score += 100;
          g.bonusNotice = { text, x: g.bonus.x + g.bonus.w / 2, y: g.bonus.y + 16, remaining: 1.5 };
          g.bonus = null; g.bonusTimer = 18 + Math.random() * 12;
        }
        if (!shot.dead && g.boss && hit(shot, g.boss)) {
          shot.dead = true; g.boss.hp--; g.score += 25;
          if (g.boss.hp <= 0) { g.boss = null; g.score += 700; endGame(true); return; }
        }
        if (!shot.dead) {
          const index = wave.enemies.findIndex(e => hit(shot, e));
          if (index >= 0) { wave.enemies.splice(index, 1); shot.dead = true; g.score += 10; }
        }
      }
      for (const shot of g.enemyShots) {
        shot.y += 175 * dt;
        for (const shield of g.shields) if (shield.hp && hit(shot, shield)) { shield.hp--; shot.dead = true; break; }
        if (!shot.dead && hit(shot, p)) {
          shot.dead = true;
          if (g.invulnerable <= 0) { g.lives--; g.invulnerable = 1.5; if (!g.lives) { endGame(); return; } }
        }
      }
      g.shots = g.shots.filter(s => !s.dead && s.y + s.h > 0);
      g.enemyShots = g.enemyShots.filter(s => !s.dead && s.y < 600);
      if (!g.boss && !wave.enemies.length) {
        if (g.level === 10) {
          g.level = 11; g.wave = { enemies: [], direction: 1, step: 0, speed: 0 };
          g.boss = createBoss();
        } else {
          g.level++; g.wave = createWave(g.level);
        }
        g.enemyShots = []; g.shots = []; g.banner = waveBanner(g.level);
      }
    }
    function draw() {
      const g = game; ctx.fillStyle = '#080e19'; ctx.fillRect(0, 0, 800, 600);
      ctx.fillStyle = '#7df6ac';
      // QSB: from wave 5 the invaders briefly fade in and out every few seconds.
      const fading = g.level >= 5 && g.time % 4 < 0.5 && Math.floor(g.time * 16) % 2 === 0;
      for (const e of fading ? [] : g.wave.enemies) {
        ctx.fillRect(e.x + 5, e.y, 20, 5); ctx.fillRect(e.x, e.y + 5, 30, 12);
        ctx.fillRect(e.x + 3, e.y + 17, 7, 5); ctx.fillRect(e.x + 20, e.y + 17, 7, 5);
        ctx.fillStyle = '#080e19'; ctx.fillRect(e.x + 6, e.y + 8, 4, 4); ctx.fillRect(e.x + 20, e.y + 8, 4, 4); ctx.fillStyle = '#7df6ac';
      }
      if (g.boss) {
        const b = g.boss;
        ctx.fillStyle = '#67c7b1'; ctx.fillRect(b.x + 20, b.y + 18, 120, 35);
        ctx.fillRect(b.x + 38, b.y + 8, 84, 10); ctx.fillRect(b.x + 55, b.y, 50, 8);
        ctx.fillRect(b.x + 8, b.y + 30, 12, 16); ctx.fillRect(b.x + 140, b.y + 30, 12, 16);
        ctx.fillStyle = '#d7f8e6'; ctx.fillRect(b.x + 77, b.y - 16, 6, 16);
        ctx.fillRect(b.x + 68, b.y - 20, 24, 5);
        ctx.fillStyle = '#153b43'; ctx.fillRect(b.x + 32, b.y + 25, 96, 14);
        ctx.fillStyle = '#ffd66d'; ctx.font = 'bold 12px monospace'; ctx.textAlign = 'center';
        ctx.fillText('REPEATER', b.x + b.w / 2, b.y + 36); ctx.textAlign = 'left';
        ctx.fillStyle = '#385347'; ctx.fillRect(300, 24, 200, 8);
        ctx.fillStyle = '#7df6ac'; ctx.fillRect(300, 24, 200 * b.hp / b.maxHp, 8);
        ctx.strokeStyle = '#d7f8e6'; ctx.strokeRect(300, 24, 200, 8);
      }
      for (const s of g.shields) if (s.hp) { ctx.fillStyle = s.hp === 2 ? '#579a6d' : '#2c5941'; ctx.fillRect(s.x, s.y, s.w - 1, s.h - 1); }
      if (g.invulnerable <= 0 || Math.floor(g.invulnerable * 8) % 2) {
        ctx.fillStyle = '#80dfff'; ctx.fillRect(g.player.x, g.player.y + 7, 40, 13); ctx.fillRect(g.player.x + 17, g.player.y, 6, 8);
      }
      ctx.fillStyle = '#fff'; for (const s of g.shots) ctx.fillRect(s.x, s.y, s.w, s.h);
      ctx.fillStyle = '#ff8b79'; for (const s of g.enemyShots) ctx.fillRect(s.x, s.y, s.w, s.h);
      if (g.bonus) {
        const { x, y } = g.bonus;
        const friendly = g.bonus.kind === 'life';
        ctx.fillStyle = friendly ? '#80dfff' : '#ffb36b';
        ctx.fillRect(x + 32, y, 36, 4);
        ctx.fillRect(x + 24, y + 4, 52, 5);
        ctx.fillRect(x + 10, y + 9, 80, 5);
        ctx.fillRect(x, y + 14, 100, 10);
        ctx.fillRect(x + 10, y + 24, 80, 4);
        ctx.fillRect(x + 24, y + 28, 52, 4);
        ctx.fillStyle = friendly ? '#1d4a5c' : '#58322b';
        ctx.fillRect(x + 29, y + 4, 42, 5);
        ctx.fillStyle = '#fff2d6';
        ctx.font = 'bold 12px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(friendly ? '73' : 'HTTPS', x + 50, y + 23);
        ctx.textAlign = 'left';
      }
      if (g.bonusNotice) {
        ctx.fillStyle = '#ffd66d'; ctx.font = 'bold 16px monospace'; ctx.textAlign = 'center';
        ctx.fillText(g.bonusNotice.text, g.bonusNotice.x, g.bonusNotice.y - (1.5 - g.bonusNotice.remaining) * 12);
        ctx.textAlign = 'left';
      }
      if (g.banner) {
        ctx.textAlign = 'center'; ctx.fillStyle = '#ffd66d';
        ctx.font = 'bold 32px monospace'; ctx.fillText(g.banner.text, 400, 290);
        ctx.font = 'bold 20px monospace'; ctx.fillStyle = '#d7f8e6'; ctx.fillText(g.banner.name, 400, 322);
        ctx.textAlign = 'left';
      }
      $('hud').textContent = 'Score ' + g.score + '  |  Lives ' + g.lives + '  |  Wave ' + g.level;
    }
    function frame(time) {
      if (!game || game.over) return;
      const dt = lastFrame ? Math.min((time - lastFrame) / 1000, .05) : 0; lastFrame = time;
      update(dt); draw();
      if (!game.over) animation = requestAnimationFrame(frame);
    }
    function showStartScreen() {
      cancelAnimationFrame(animation); game = null; resetControls(); $('game').hidden = true; $('start-screen').hidden = false;
      if (document.fullscreenElement === $('game')) document.exitFullscreen().catch(error => { $('status').textContent = error.message; });
      loadScores(); $('start').focus();
    }
    $('start').addEventListener('click', startGame);
    $('restart').addEventListener('click', startGame);
    $('quit').addEventListener('click', () => { if (confirm('Quit this game without saving?')) showStartScreen(); });
    $('back').addEventListener('click', showStartScreen);
    $('fullscreen').addEventListener('click', async () => {
      try {
        if (document.fullscreenElement) await document.exitFullscreen();
        else if ($('game').requestFullscreen) await $('game').requestFullscreen();
        else throw new Error('Fullscreen is not supported by this browser.');
      } catch (error) { alert('Fullscreen unavailable: ' + error.message); }
    });
    document.addEventListener('fullscreenchange', () => { $('fullscreen').textContent = document.fullscreenElement ? 'Exit fullscreen' : 'Fullscreen'; });
    $('score-form').addEventListener('submit', async event => {
      event.preventDefault(); if (saving || !game || !game.over) return;
      saving = true; const button = $('score-form').querySelector('button');
      button.disabled = true; $('back').disabled = true; $('quit').disabled = true;
      try {
        const response = await fetch('/mesh-invaders/api/scores', { method: 'POST', body: new URLSearchParams({ name: $('name').value, score: String(game.score) }) });
        if (!response.ok) throw new Error('HTTP ' + response.status);
        showScores(await response.json()); $('score-form').hidden = true;
        $('save-status').textContent = 'Score submitted. Only the best result per player and the top ten are kept.';
      } catch (error) { $('save-status').textContent = 'Could not save score: ' + error.message + '. Reconnect and try again.'; }
      finally { saving = false; button.disabled = false; $('back').disabled = false; $('quit').disabled = false; }
    });
    document.addEventListener('keydown', event => {
      if (!game || game.over) return;
      const key = event.key.toLowerCase();
      if (key === 'arrowleft' || key === 'a') keys.left = true;
      else if (key === 'arrowright' || key === 'd') keys.right = true;
      else if (key === ' ' || key === 'spacebar') { if (!event.repeat) keys.fire = true; }
      else return;
      event.preventDefault();
    });
    document.addEventListener('keyup', event => {
      const key = event.key.toLowerCase();
      if (key === 'arrowleft' || key === 'a') keys.left = false;
      if (key === 'arrowright' || key === 'd') keys.right = false;
      if (key === ' ' || key === 'spacebar') keys.fire = false;
    });
    document.querySelectorAll('[data-control]').forEach(button => {
      const control = button.dataset.control;
      button.addEventListener('pointerdown', event => { event.preventDefault(); button.setPointerCapture(event.pointerId); keys[control] = true; });
      for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) button.addEventListener(type, () => { keys[control] = false; });
    });
    window.addEventListener('blur', resetControls);
    document.addEventListener('visibilitychange', () => { if (document.hidden) resetControls(); lastFrame = 0; });
    async function synchronizeDeviceTime() {
      try {
        const response = await fetch('/api/time/sync', {
          method: 'POST', body: new URLSearchParams({ epoch: String(Math.floor(Date.now() / 1000)) })
        });
        if (!response.ok) throw new Error('HTTP ' + response.status);
      } catch (error) { console.error('Device clock synchronization failed:', error); }
    }
    loadScores();
    synchronizeDeviceTime();
  </script>
</body>
</html>
)MESHGAME";
