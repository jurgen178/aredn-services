const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

function element() {
  return {
    hidden: false,
    textContent: '',
    style: { setProperty() {} },
    classList: { toggle() {} },
    addEventListener() {},
    setAttribute() {},
    querySelector() { return element(); }
  };
}

async function main() {
  const source = fs.readFileSync(path.join(__dirname, '..', 'CameraPage.h'), 'utf8');
  const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
  const elements = new Map();
  const document = {
    hidden: false,
    visibilityState: 'visible',
    fullscreenEnabled: false,
    addEventListener() {},
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, element());
      return elements.get(id);
    }
  };
  let statusRequests = 0;
  let imageRequests = 0;
  let finishImage;
  let image = new Promise(resolve => { finishImage = resolve; });
  const status = {
    status: 'available', error: 'none', http_status: 200,
    image_available: true, version: 1, age_seconds: 1,
    refresh_seconds: 30, capture_epoch: 0,
    resolution_level: 0, last_fetch_ms: 1000, last_fetch_bytes: 100,
    download_timeout_ms: 20000
  };
  const context = vm.createContext({
    document,
    window: { setTimeout() { return 1; }, clearTimeout() {}, setInterval() {}, addEventListener() {} },
    navigator: { onLine: true },
    console: { log() {}, error(...args) { throw new Error(args.join(' ')); } },
    AbortController, URLSearchParams,
    URL: { createObjectURL() { return 'blob:test'; }, revokeObjectURL() {} },
    Image: class {
      constructor() { this.naturalWidth = 640; this.naturalHeight = 480; }
      set src(value) { queueMicrotask(() => this.onload()); }
    },
    fetch: async url => {
      if (url === '/api/time/sync') return { ok: true };
      if (url === '/camera/api/status') {
        statusRequests++;
        return { ok: true, json: async () => ({ ...status }) };
      }
      if (url.startsWith('/camera/image.jpg')) {
        imageRequests++;
        return { ok: true, blob: () => image };
      }
      throw new Error(`Unexpected request: ${url}`);
    }
  });
  vm.runInContext(script, context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(statusRequests, 1);
  assert.equal(imageRequests, 1);

  status.version = 2;
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(statusRequests, 2, 'status must continue while the image is downloading');
  assert.equal(imageRequests, 1, 'only one image request may run at a time');

  finishImage({ type: 'image/jpeg', size: 100 });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(vm.runInContext('currentVersion', context), 1);
  assert.equal(elements.get('cameraImage').hidden, false);
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(statusRequests, 3);
  assert.equal(imageRequests, 2, 'a newer version must be requested after the first download');
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(vm.runInContext('currentVersion', context), 2);
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(imageRequests, 2, 'an unchanged version must not be downloaded again');

  image = new Promise(resolve => { finishImage = resolve; });
  status.version = 3;
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(imageRequests, 3);
  status.image_available = false;
  status.status = 'not_configured';
  status.version = 4;
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(vm.runInContext('cameraActivity', context), '');
  assert.equal(vm.runInContext('currentVersion', context), 4);
  finishImage({ type: 'image/jpeg', size: 100 });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(vm.runInContext('currentVersion', context), 4,
    'a superseded image must not replace the unavailable status');
  console.log('camera page tests passed');
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
