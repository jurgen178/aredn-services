const assert = require('node:assert/strict');

const UINT32_MAX = 0xFFFFFFFF;
const RESOLUTIONS = [
  { width: 0, height: 0 },
  { width: 896, height: 672 },
  { width: 640, height: 480 }
];
const LEVELS = RESOLUTIONS.length;

// Port of CameraAdaptive.h. Keep both in sync: unsigned 32-bit wraparound is
// emulated with >>> 0 and C++ integer division with Math.floor.
class CameraAdaptive {
  static LEVELS = LEVELS;

  static resolutionAt(level) {
    return level < LEVELS ? RESOLUTIONS[level] : null;
  }

  static clamp(value, minimum, maximum) {
    return value < minimum ? minimum : value > maximum ? maximum : value;
  }

  constructor(intervalMs) {
    this.intervalMs = intervalMs;
    this.reset();
  }

  reset() {
    this.levelValue = 0;
    this.pressureMs = 0;
    this.lastObservationMs = 0;
    this.lastChangeMs = 0;
    this.haveObservation = false;
    this.haveChange = false;
    this.headerAverageMs = 0;
    this.downloadAverageMs = 0;
    this.bytesPerSecond = 0;
    this.imageBytes = new Array(LEVELS).fill(0);
  }

  level() { return this.levelValue; }
  resolution() { return RESOLUTIONS[this.levelValue]; }

  connectTimeoutMs() {
    return this.headerAverageMs === 0 ? 5000
      : CameraAdaptive.clamp(2000 + this.headerAverageMs * 2, 5000, 10000);
  }

  headerTimeoutMs() {
    return this.headerAverageMs === 0 ? 10000
      : CameraAdaptive.clamp(2000 + this.headerAverageMs * 4, 10000, 20000);
  }

  downloadTimeoutMs() {
    return this.downloadAverageMs === 0 ? 20000
      : CameraAdaptive.clamp(5000 + this.downloadAverageMs * 3, 15000, 30000);
  }

  success(now, totalMs, headerMs, downloadMs, imageBytes) {
    this.headerAverageMs = this.updateAverage(this.headerAverageMs, headerMs);
    this.downloadAverageMs = this.updateAverage(this.downloadAverageMs, downloadMs);
    this.updateImageSize(imageBytes);
    let sampleRate = 0;
    if (imageBytes > 0) {
      sampleRate = Math.floor(imageBytes * 1000 / (downloadMs === 0 ? 1 : downloadMs));
      this.bytesPerSecond = this.bytesPerSecond === 0 ? sampleRate
        : Math.floor((this.bytesPerSecond * 3 + sampleRate) / 4);
    }
    let direction = totalMs > Math.floor(this.intervalMs * 2 / 5) ? 1
      : totalMs < Math.floor(this.intervalMs / 4) ? -1 : 0;
    if (direction < 0 && this.levelValue > 0 && !this.canIncrease(now, sampleRate)) {
      direction = 0;
    }
    this.observe(now, totalMs, direction);
  }

  transferFailure(now, durationMs, hasEvidence, expectedBytes = 0) {
    this.updateImageSize(expectedBytes);
    if (hasEvidence) {
      this.observe(now, durationMs, 1);
    } else {
      this.pressureMs = 0;
      this.lastObservationMs = now;
      this.haveObservation = true;
    }
  }

  updateAverage(average, sample) {
    sample = CameraAdaptive.clamp(sample, 1, 60000);
    return average === 0 ? sample : Math.floor((average * 3 + sample) / 4);
  }

  updateImageSize(bytes) {
    if (bytes === 0) return;
    const previous = this.imageBytes[this.levelValue];
    this.imageBytes[this.levelValue] = previous === 0 ? bytes
      : Math.floor((previous * 3 + bytes) / 4);
  }

  canIncrease(now, sampleRate) {
    const targetBytes = this.imageBytes[this.levelValue - 1];
    const rate = Math.min(sampleRate, this.bytesPerSecond);
    if (targetBytes > 0 && rate > 0) {
      const predictedMs = this.headerAverageMs +
        Math.floor((targetBytes * 1000 + rate - 1) / rate);
      if (predictedMs < Math.floor(this.intervalMs / 4)) return true;
    }
    // Recheck occasionally: an old size estimate can become stale as the scene changes.
    return this.haveChange && ((now - this.lastChangeMs) >>> 0) >= this.intervalMs * 30;
  }

  observe(now, durationMs, direction) {
    const elapsed = this.haveObservation
      ? CameraAdaptive.clamp((now - this.lastObservationMs) >>> 0, 0, this.intervalMs)
      : CameraAdaptive.clamp(durationMs, 0, this.intervalMs);
    this.lastObservationMs = now;
    this.haveObservation = true;
    if ((this.levelValue === 0 && direction < 0) ||
        (this.levelValue + 1 === LEVELS && direction > 0)) {
      this.pressureMs = 0;
      return;
    }
    if (direction > 0) {
      this.pressureMs += elapsed;
    } else if (direction < 0) {
      this.pressureMs -= elapsed;
    } else if (this.pressureMs > 0) {
      this.pressureMs = this.pressureMs > elapsed ? this.pressureMs - elapsed : 0;
    } else if (this.pressureMs < 0) {
      this.pressureMs = this.pressureMs < -elapsed ? this.pressureMs + elapsed : 0;
    }

    const downThreshold = this.intervalMs * 2;
    const upThreshold = this.intervalMs * 4;
    if (this.pressureMs > downThreshold) this.pressureMs = downThreshold;
    if (this.pressureMs < -upThreshold) this.pressureMs = -upThreshold;
    if (this.haveChange && ((now - this.lastChangeMs) >>> 0) < this.intervalMs * 2) return;

    if (this.pressureMs >= downThreshold && this.levelValue + 1 < LEVELS) {
      ++this.levelValue;
    } else if (this.pressureMs <= -upThreshold && this.levelValue > 0) {
      --this.levelValue;
    } else {
      return;
    }
    this.pressureMs = 0;
    this.lastChangeMs = now;
    this.haveChange = true;
  }
}

function success(camera, now, duration, bytes, header = 1000) {
  assert.ok(duration >= header);
  camera.success(now, duration, header, duration - header, bytes);
}

function reduceFromFull(camera) {
  success(camera, 0, 2000, 800 * 1024, 500);
  success(camera, 30000, 14000, 800 * 1024);
  assert.equal(camera.level(), 0);
  success(camera, 60000, 14000, 800 * 1024);
  assert.equal(camera.level(), 1);
}

const camera = new CameraAdaptive(30000);
assert.equal(CameraAdaptive.LEVELS, 3);
assert.deepEqual(CameraAdaptive.resolutionAt(0), { width: 0, height: 0 });
assert.deepEqual(CameraAdaptive.resolutionAt(1), { width: 896, height: 672 });
assert.deepEqual(CameraAdaptive.resolutionAt(2), { width: 640, height: 480 });
assert.equal(CameraAdaptive.resolutionAt(3), null);
assert.equal(camera.connectTimeoutMs(), 5000);
assert.equal(camera.headerTimeoutMs(), 10000);
assert.equal(camera.downloadTimeoutMs(), 20000);

reduceFromFull(camera);
for (let now = 90000; now <= 300000; now += 30000) {
  success(camera, now, 2000, 100 * 1024, 500);
  assert.equal(camera.level(), 1);
}
for (let now = 330000; now <= 750000 && camera.level() === 1; now += 30000) {
  success(camera, now, 400, 100 * 1024, 100);
}
assert.equal(camera.level(), 0);

camera.reset();
reduceFromFull(camera);
for (let now = 90000; now <= 930000; now += 30000) {
  success(camera, now, 2000, 100 * 1024, 500);
  assert.equal(camera.level(), 1);
}
for (let now = 960000; now <= 1050000; now += 30000) {
  success(camera, now, 2000, 100 * 1024, 500);
}
assert.equal(camera.level(), 0);

camera.reset();
reduceFromFull(camera);
success(camera, 90000, 14000, 100 * 1024);
assert.equal(camera.level(), 1);
success(camera, 120000, 14000, 100 * 1024);
assert.equal(camera.level(), 2);
assert.deepEqual(camera.resolution(), { width: 640, height: 480 });
for (let now = 150000; now <= 210000; now += 30000) {
  success(camera, now, 1000, 50 * 1024, 500);
  assert.equal(camera.level(), 2);
}
success(camera, 240000, 1000, 50 * 1024, 500);
assert.equal(camera.level(), 1);

camera.reset();
camera.transferFailure(30000, 30000, true, 800 * 1024);
camera.transferFailure(60000, 30000, true, 800 * 1024);
assert.equal(camera.level(), 1);
for (let now = 90000; now <= 300000; now += 30000) {
  success(camera, now, 2000, 100 * 1024, 500);
  assert.equal(camera.level(), 1);
}

camera.reset();
camera.transferFailure(30000, 30000, true);
camera.transferFailure(60000, 30000, true);
assert.equal(camera.level(), 1);
for (let now = 90000; now <= 930000; now += 30000) {
  success(camera, now, 2000, 100 * 1024, 500);
  assert.equal(camera.level(), 1);
}
for (let now = 960000; now <= 1050000; now += 30000) {
  success(camera, now, 2000, 100 * 1024, 500);
}
assert.equal(camera.level(), 0);

camera.reset();
success(camera, 30000, 14000, 800 * 1024);
camera.transferFailure(60000, 5000, false);
success(camera, 90000, 14000, 800 * 1024);
assert.equal(camera.level(), 0);
success(camera, 120000, 14000, 800 * 1024);
assert.equal(camera.level(), 1);

camera.reset();
for (let now = 30000; now <= 300000; now += 60000) {
  success(camera, now, 14000, 800 * 1024);
  success(camera, now + 30000, 2000, 800 * 1024, 500);
}
assert.equal(camera.level(), 0);
camera.transferFailure(330000, 5000, false);
camera.transferFailure(360000, 5000, false);
assert.equal(camera.level(), 0);

camera.reset();
success(camera, 1000, 1000, 800 * 1024, 250);
assert.equal(camera.downloadTimeoutMs(), 15000);
for (let now = 31000; now <= 361000; now += 30000) {
  camera.success(now, 60000, 60000, 60000, 800 * 1024);
}
assert.equal(camera.connectTimeoutMs(), 10000);
assert.equal(camera.headerTimeoutMs(), 20000);
assert.equal(camera.downloadTimeoutMs(), 30000);

// The millisecond counter wraps at 2^32; time differences must stay correct.
camera.reset();
success(camera, UINT32_MAX - 10000, 14000, 800 * 1024);
success(camera, 19999, 14000, 800 * 1024);
success(camera, 49999, 14000, 800 * 1024);
assert.equal(camera.level(), 1);

console.log('camera adaptive tests passed');
