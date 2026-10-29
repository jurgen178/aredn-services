const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const vm = require('node:vm');

async function main() {
  const source = fs.readFileSync(path.join(__dirname, '..', 'CameraPage.h'), 'utf8');
  const start = source.indexOf('    async function fetchSnapshotBlob(');
  const end = source.indexOf('    async function synchronizeDeviceTime()', start);
  assert.ok(start >= 0 && end > start);
  const context = vm.createContext({
    window: { setTimeout, clearTimeout },
    fetch, Blob, lastImageDownloadMs: 0
  });
  vm.runInContext(source.slice(start, end), context);

  const server = http.createServer((request, response) => {
    response.writeHead(200, {
      'Content-Type': 'image/jpeg',
      'Content-Length': request.url === '/truncated' ? '500' : '100'
    });
    response.write(Buffer.alloc(100));
    if (request.url === '/truncated') {
      setTimeout(() => response.destroy(), 20);
    } else {
      response.end();
    }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  try {
    const incomplete = { received: 0, expected: null };
    await assert.rejects(context.fetchSnapshotBlob(
      `${base}/truncated`, new AbortController(), incomplete
    ));
    assert.equal(incomplete.expected, 500);
    assert.equal(incomplete.received, 100);

    const complete = { received: 0, expected: null };
    const blob = await context.fetchSnapshotBlob(
      `${base}/complete`, new AbortController(), complete
    );
    assert.equal(complete.expected, 100);
    assert.equal(complete.received, 100);
    assert.equal(blob.size, 100);

    // Without a stream reader, a slow body must not trip the idle timer.
    const delays = new Map();
    const cleared = new Set();
    let idleCleared = null;
    const fallbackContext = vm.createContext({
      window: {
        setTimeout(callback, delay) {
          const id = setTimeout(callback, delay);
          delays.set(id, delay);
          return id;
        },
        clearTimeout(id) { cleared.add(id); clearTimeout(id); }
      },
      fetch: async () => ({
        ok: true,
        headers: { get(name) { return name === 'Content-Type' ? 'image/jpeg' : null; } },
        blob: async () => {
          idleCleared = [...cleared].some(id => delays.get(id) === 30000);
          return new Blob([Buffer.alloc(10)], { type: 'image/jpeg' });
        }
      }),
      Blob, lastImageDownloadMs: 0
    });
    vm.runInContext(source.slice(start, end), fallbackContext);
    const fallback = { received: 0, expected: null };
    await fallbackContext.fetchSnapshotBlob('/x', new AbortController(), fallback);
    assert.equal(fallback.received, 10);
    assert.equal(idleCleared, true, 'the idle timer must be cancelled before reading the blob');
    console.log('camera HTTP transfer tests passed');
  } finally {
    server.closeAllConnections();
    await new Promise((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
  }
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
