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
  let statusFails = false;
  let imageResponseFactory = null;
  let nextTimerId = 0;
  let fakeNow = Date.now();
  const timers = new Map();
  const logs = [];
  let imageWidth = 2560;
  let imageHeight = 1920;
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
    window: {
      setTimeout(callback, delay) {
        const id = ++nextTimerId;
        timers.set(id, { callback, delay });
        return id;
      },
      clearTimeout(id) { timers.delete(id); },
      setInterval() {},
      addEventListener() {}
    },
    navigator: { onLine: true },
    console: { log(message) { logs.push(message); }, error() {} },
    Date: class extends Date { static now() { return fakeNow; } },
    AbortController, URLSearchParams, Blob,
    URL: { createObjectURL() { return 'blob:test'; }, revokeObjectURL() {} },
    Image: class {
      constructor() { this.naturalWidth = imageWidth; this.naturalHeight = imageHeight; }
      set src(value) { queueMicrotask(() => this.onload()); }
    },
    fetch: async url => {
      if (url === '/api/time/sync') return { ok: true };
      if (url === '/camera/api/status') {
        statusRequests++;
        if (statusFails) throw new TypeError('Status unavailable');
        return { ok: true, json: async () => ({ ...status }) };
      }
      if (url.startsWith('/camera/image.jpg')) {
        imageRequests++;
        if (imageResponseFactory) return imageResponseFactory();
        return { ok: true, blob: () => image };
      }
      throw new Error(`Unexpected request: ${url}`);
    }
  });
  vm.runInContext(script, context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(statusRequests, 1);
  assert.equal(imageRequests, 1);
  assert.doesNotMatch(elements.get('cameraStatus').textContent, /\(\d+x\d+\)/);
  assert.match(elements.get('cameraStatus').textContent, /Loading new image\./,
    'show image loading while the previous image is still fresh');

  status.version = 2;
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(statusRequests, 1, 'status polling must be throttled while the image is downloading');
  fakeNow += 31000;
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(statusRequests, 2, 'status must still be polled during a long download');
  assert.equal(imageRequests, 1, 'only one image request may run at a time');

  finishImage({ type: 'image/jpeg', size: 100 });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(vm.runInContext('currentVersion', context), 1);
  assert.equal(elements.get('cameraImage').hidden, false);
  assert.match(elements.get('cameraStatus').  textContent, /\(2560x1920\)/);
  imageWidth = 896;
  imageHeight = 672;
  image = new Promise(resolve => { finishImage = resolve; });
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(statusRequests, 3);
  assert.equal(imageRequests, 2, 'a newer version must be requested after the first download');
  assert.match(elements.get('cameraStatus').textContent, /\(2560x1920\)/,
    'the displayed resolution must remain unchanged while the new image loads');
  assert.match(elements.get('cameraStatus').textContent, /Loading new image\./,
    'show the loading message during a normal refresh');
  finishImage({ type: 'image/jpeg', size: 100 });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(vm.runInContext('currentVersion', context), 2);
  assert.match(elements.get('cameraStatus').textContent, /\(896x672\)/);
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(imageRequests, 2, 'an unchanged version must not be downloaded again');

  image = new Promise(resolve => { finishImage = resolve; });
  status.version = 3;
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(imageRequests, 3);
  fakeNow += 31000;
  status.image_available = false;
  status.status = 'not_configured';
  status.version = 4;
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(vm.runInContext('cameraActivity', context), '');
  assert.equal(vm.runInContext('currentVersion', context), 4);
  assert.doesNotMatch(elements.get('cameraStatus').textContent, /\(\d+x\d+\)/);
  finishImage({ type: 'image/jpeg', size: 100 });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(vm.runInContext('currentVersion', context), 4,
    'a superseded image must not replace the unavailable status');

  const partialResponse = () => {
    let reads = 0;
    return {
      ok: true,
      headers: { get(name) {
        return name === 'Content-Length' ? '500' : name === 'Content-Type' ? 'image/jpeg' : null;
      } },
      body: { getReader() { return {
        async read() {
          if (reads++ === 0) return { done: false, value: new Uint8Array(100) };
          throw new TypeError('Connection closed before the full response');
        }
      }; } }
    };
  };
  const runTimer = delay => {
    const entry = [...timers.entries()].find(([, timer]) => timer.delay === delay);
    assert.ok(entry, `expected a ${delay} ms retry timer`);
    timers.delete(entry[0]);
    entry[1].callback();
  };
  imageResponseFactory = partialResponse;
  status.image_available = true;
  status.status = 'available';
  status.version = 5;
  await vm.runInContext('refreshCameraStatus()', context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(imageRequests, 4);
  assert.ok(logs.some(message => message.includes('received 100/500 bytes')));
  assert.equal(vm.runInContext('currentVersion', context), 4,
    'a truncated image must not replace the displayed version');

  statusFails = true;
  await vm.runInContext('refreshCameraStatus()', context);
  runTimer(2000);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(imageRequests, 5, 'image retry must not depend on a successful status request');
  imageResponseFactory = () => {
    let reads = 0;
    return {
      ok: true,
      headers: { get(name) {
        return name === 'Content-Length' ? '500' : name === 'Content-Type' ? 'image/jpeg' : null;
      } },
      body: { getReader() { return {
        async read() {
          return reads++ === 0
            ? { done: false, value: new Uint8Array(100) }
            : { done: true };
        }
      }; } }
    };
  };
  runTimer(4000);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(imageRequests, 6);
  assert.equal(vm.runInContext('currentVersion', context), 4,
    'a response shorter than Content-Length must not replace the displayed image');
  assert.ok(logs.some(message => message.includes('Incomplete image: received 100 of 500 bytes')));
  imageResponseFactory = () => {
    let reads = 0;
    return {
      ok: true,
      headers: { get(name) {
        return name === 'Content-Length' ? '100' : name === 'Content-Type' ? 'image/jpeg' : null;
      } },
      body: { getReader() { return {
        async read() {
          return reads++ === 0
            ? { done: false, value: new Uint8Array(100) }
            : { done: true };
        }
      }; } }
    };
  };
  imageWidth = 640;
  imageHeight = 480;
  runTimer(8000);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(imageRequests, 7);
  assert.equal(vm.runInContext('currentVersion', context), 5);
  assert.match(elements.get('cameraStatus').textContent, /\(640x480\)/);
  assert.equal(vm.runInContext('imageRetryDelayMs', context), 2000);

  statusFails = false;
  imageResponseFactory = partialResponse;
  status.version = 6;
  await vm.runInContext('refreshCameraStatus()', context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(imageRequests, 8);
  await vm.runInContext('refreshCameraStatus()', context);
  assert.equal(imageRequests, 8, 'the same failed version must wait for the retry timer');
  assert.match(vm.runInContext('cameraActivity', context), /could not be loaded; retrying/,
    'the retry notice must survive status polls while a retry is pending');
  imageResponseFactory = () => {
    let reads = 0;
    return {
      ok: true,
      headers: { get(name) {
        return name === 'Content-Length' ? '100' : name === 'Content-Type' ? 'image/jpeg' : null;
      } },
      body: { getReader() { return {
        async read() {
          return reads++ === 0
            ? { done: false, value: new Uint8Array(100) }
            : { done: true };
        }
      }; } }
    };
  };
  status.version = 7;
  await vm.runInContext('refreshCameraStatus()', context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(imageRequests, 9, 'a newer version must not wait for the retry backoff');
  assert.equal(vm.runInContext('currentVersion', context), 7);
  assert.equal(vm.runInContext('imageRetryDelayMs', context), 2000);
  console.log('camera page tests passed');
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
