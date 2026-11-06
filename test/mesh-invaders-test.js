const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

function pageScript(file) {
  const source = fs.readFileSync(path.join(__dirname, '..', file), 'utf8');
  return source.match(/<script>([\s\S]*?)<\/script>/)[1];
}

function environment(script) {
  const elements = new Map();
  const listeners = new Map();
  const drawing = { calls: [], fillRect(...args) { this.calls.push(['rect', ...args]); }, fillText(...args) { this.calls.push(['text', ...args]); } };
  function element(id) {
    if (!elements.has(id)) elements.set(id, {
      id, hidden: false, disabled: false, textContent: '', value: '', dataset: {},
      classList: { toggle() {} }, children: [], handlers: {},
      addEventListener(type, fn) { this.handlers[type] = fn; },
      appendChild(child) { this.children.push(child); },
      replaceChildren() { this.children = []; },
      querySelector() { return element('submit'); }, focus() {},
      getContext() { return drawing; }, setPointerCapture() {}
    });
    return elements.get(id);
  }
  const document = {
    getElementById: element, createElement: () => ({ textContent: '', children: [], appendChild(child) { this.children.push(child); } }),
    querySelectorAll: () => [],
    addEventListener(type, fn) { listeners.set(type, fn); }
  };
  const responses = [];
  const context = vm.createContext({
    document, window: { addEventListener() {} }, console, URLSearchParams, Math,
    fetch: async (...args) => {
      if (args[0] === '/api/time/sync') return { ok: true };
      if (responses.length) return responses.shift()(...args);
      return { ok: true, json: async () => ({ version: 1, scores: [] }) };
    },
    requestAnimationFrame: () => 1, cancelAnimationFrame() {},
    confirm: () => true, alert() {}, setTimeout() {}, Blob, URL
  });
  vm.runInContext(script, context);
  return { context, element, responses, drawing };
}

async function main() {
  const firmware = fs.readFileSync(path.join(__dirname, '..', 'aredn-services.ino'), 'utf8');
  const routes = [...firmware.matchAll(/server\.on\("([^"]+)",\s*HTTP_(GET|POST)/g)]
    .map(([, uri, method]) => ({ uri, method }));
  function resolveRoute(url, method) {
    return routes.find(route => route.method === method &&
      (url === route.uri || url.startsWith(route.uri + '/')))?.uri;
  }
  assert.equal(resolveRoute('/mesh-invaders', 'GET'), '/mesh-invaders');
  assert.equal(resolveRoute('/mesh-invaders/api/scores', 'GET'), '/mesh-invaders/api/scores');
  assert.equal(resolveRoute('/mesh-invaders/api/status', 'GET'), '/mesh-invaders/api/status');
  assert.equal(resolveRoute('/mesh-invaders/api/scores', 'POST'), '/mesh-invaders/api/scores');
  assert.equal(resolveRoute('/setup/mesh-invaders', 'GET'), '/setup/mesh-invaders');
  assert.equal(resolveRoute('/setup/api/mesh-invaders/scores', 'POST'), '/setup/api/mesh-invaders/scores');
  const game = environment(pageScript('MeshInvadersPage.h'));
  const run = text => vm.runInContext(text, game.context);
  run('startGame()');
  assert.equal(run('game.lives'), 3);
  assert.equal(run('game.wave.enemies.length'), 36);
  assert.equal(run('game.shields.length'), 72);
  run('keys.right = true; update(0.05); keys.right = false');
  assert.equal(run('game.player.x'), 395);
  run('keys.fire = true; update(0.05); keys.fire = false');
  assert.equal(run('game.shots.length'), 1);
  run('game.wave.enemies = [{x: 758, y: 100, w: 30, h: 22}]; game.wave.direction = 1; update(0.05)');
  assert.equal(run('game.wave.direction'), -1);
  assert.equal(run('game.wave.enemies[0].y'), 115);
  run('game.bonusTimer = 0; update(0.05)');
  assert.equal(run('game.bonus.x < 800'), true);
  run('draw()');
  assert.ok(game.drawing.calls.some(call => call[0] === 'text' && call[1] === 'HTTPS'));
  assert.ok(game.drawing.calls.some(call => call[0] === 'rect' && call[3] === 100 && call[4] === 10));
  run('game.shots = [{x: game.bonus.x + 5, y: game.bonus.y + 20, w: 4, h: 12}]; update(0.01)');
  assert.equal(run('game.bonus'), null);
  assert.equal(run('game.score'), 100);
  run('draw()');
  assert.ok(game.drawing.calls.some(call => call[0] === 'text' && call[1] === '+100'));
  run('update(1.5)');
  assert.equal(run('game.bonusNotice'), null);
  run('game.shots = [{x: 125, y: 450, w: 4, h: 12}]; update(0.01)');
  assert.equal(run('game.shields[0].hp'), 1);
  run('game.wave.enemies = [{x: 100, y: 100, w: 30, h: 22}]; game.shots = [{x: 105, y: 115, w: 4, h: 12}]; update(0.01)');
  assert.equal(run('game.level'), 2);
  assert.equal(run('game.score'), 110);
  run('game.lives = 1; game.enemyShots = [{x: game.player.x, y: game.player.y - 2, w: 5, h: 12}]; update(0.01)');
  assert.equal(run('game.over'), true);
  game.element('name').value = 'TEST';
  game.responses.push(async () => ({ ok: false, status: 503 }));
  await game.element('score-form').handlers.submit({ preventDefault() {} });
  assert.match(game.element('save-status').textContent, /try again/);
  assert.equal(game.element('score-form').hidden, false);
  game.responses.push(async (url, options) => {
    assert.equal(url, '/mesh-invaders/api/scores');
    assert.equal(options.body.get('name'), 'TEST');
    assert.equal(options.body.get('score'), '110');
    return { ok: true, json: async () => ({ version: 1, scores: [{ name: 'TEST', score: 110 }] }) };
  });
  await game.element('score-form').handlers.submit({ preventDefault() {} });
  assert.equal(game.element('score-form').hidden, true);

  const extra = environment(pageScript('MeshInvadersPage.h'));
  const x = text => vm.runInContext(text, extra.context);
  const rects = (w, h) => extra.drawing.calls.filter(c => c[0] === 'rect' && c[3] === w && c[4] === h).length;
  assert.equal(x('createWave(1).speed'), 37);
  assert.equal(x('createWave(5).speed'), 85);
  assert.equal(x('createWave(20).speed'), 150);
  x('startGame()');
  assert.equal(x('game.banner.text'), 'WAVE 1');
  assert.equal(x('game.banner.name'), 'CQ CQ – Station on the air');
  x('update(2.6)');
  assert.equal(x('game.banner'), null);
  x('game.level = 3; game.bonusTimer = 0; update(0.01)');
  assert.equal(x('game.bonus.kind'), 'https');
  x('game.bonus = null; game.level = 4; game.bonusTimer = 0; update(0.01)');
  assert.equal(x('game.bonus.kind'), 'life');
  x('game.lives = 2; game.shots = [{x: game.bonus.x + 5, y: game.bonus.y + 20, w: 4, h: 12}]; update(0.01)');
  assert.equal(x('game.lives'), 3);
  assert.equal(x('game.bonusNotice.text'), '+1 LIFE');
  x('game.bonusTimer = 0; update(0.01)');
  assert.equal(x('game.bonus.kind'), 'https');
  x('game.bonus = {x: 300, y: 38, w: 100, h: 32, kind: "life"}; game.lives = 5; game.score = 0; game.shots = [{x: 305, y: 58, w: 4, h: 12}]; update(0.01)');
  assert.equal(x('game.lives'), 5);
  assert.equal(x('game.score'), 50);
  x('game.bonus = {x: 300, y: 38, w: 100, h: 32, kind: "life"}; draw()');
  assert.ok(extra.drawing.calls.some(c => c[0] === 'text' && c[1] === '73'));
  x('game.bonus = null; game.wave.enemies = [{x: 100, y: 100, w: 30, h: 22}]; game.shots = [{x: 105, y: 115, w: 4, h: 12}]; game.level = 4; update(0.01)');
  assert.equal(x('game.banner.text'), 'WAVE 5');
  assert.equal(x('game.banner.name'), 'Sunspot minimum');
  x('draw()');
  assert.ok(extra.drawing.calls.some(c => c[0] === 'text' && c[1] === 'WAVE 5'));
  assert.equal(x('waveBanner(12).name'), 'Pile-up!');
  x('game.wave.enemies = [{x: 100, y: 100, w: 30, h: 22}]; game.time = 0.01');
  extra.drawing.calls.length = 0; x('draw()');
  assert.equal(rects(30, 12), 0, 'invaders fade during the QSB window');
  x('game.time = 2');
  extra.drawing.calls.length = 0; x('draw()');
  assert.equal(rects(30, 12), 1);

  const setup = environment(pageScript('MeshSetupPage.h'));
  const validate = value => vm.runInContext(`validate(${JSON.stringify(value)})`, setup.context);
  validate({ version: 1, scores: [{ name: 'TEST', score: 10 }] });
  validate({ version: 1, scores: [] });
  assert.throws(() => validate({ version: 1, scores: [{ name: 'TEST', score: 1 }, { name: 'test', score: 1 }] }));
  assert.throws(() => validate({ version: 1, scores: [{ name: 'A', score: 1 }, { name: 'B', score: 2 }] }));
  assert.throws(() => validate({ version: 1, scores: [{ name: '<script>', score: 1 }] }));
  assert.throws(() => validate({ version: 1, scores: [{ name: 'A', score: 1000000001 }] }));
  assert.throws(() => validate({ version: 2, scores: [] }));
  const backup = { version: 1, scores: [{ name: 'ALICE', score: 500 }, { name: 'BOB', score: 100 }] };
  setup.element('file').files = [{ size: 100, text: async () => JSON.stringify(backup) }];
  setup.responses.push(async (url, options) => {
    assert.equal(url, '/setup/api/mesh-invaders/scores');
    assert.equal(options.body.get('count'), '2');
    assert.equal(options.body.get('name0'), 'ALICE');
    assert.equal(options.body.get('score1'), '100');
    return { ok: true, json: async () => backup };
  });
  await setup.element('import').handlers.click();
  assert.equal(setup.element('status').textContent, 'Highscores restored.');
  setup.element('file').files = [{ size: 100, text: async () => '{"version":2}' }];
  await setup.element('import').handlers.click();
  assert.match(setup.element('status').textContent, /Import failed/);
  console.log('Mesh Invaders game and backup validation tests passed.');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
