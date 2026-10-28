#include "../CameraAdaptive.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

static void success(CameraAdaptive& camera, uint32_t now, uint32_t duration,
                    uint32_t bytes, uint32_t header = 1000)
{
  assert(duration >= header);
  camera.success(now, duration, header, duration - header, bytes);
}

static void reduceFromFull(CameraAdaptive& camera)
{
  success(camera, 0, 2000, 800 * 1024, 500);
  success(camera, 30000, 14000, 800 * 1024);
  assert(camera.level() == 0);
  success(camera, 60000, 14000, 800 * 1024);
  assert(camera.level() == 1);
}

int main()
{
  CameraAdaptive camera(30000);
  CameraAdaptive::Resolution resolution{};
  assert(CameraAdaptive::LEVELS == 3);
  assert(CameraAdaptive::resolutionAt(0, resolution));
  assert(resolution.width == 0 && resolution.height == 0);
  assert(CameraAdaptive::resolutionAt(1, resolution));
  assert(resolution.width == 896 && resolution.height == 672);
  assert(CameraAdaptive::resolutionAt(2, resolution));
  assert(resolution.width == 640 && resolution.height == 480);
  assert(!CameraAdaptive::resolutionAt(3, resolution));
  assert(camera.connectTimeoutMs() == 5000);
  assert(camera.headerTimeoutMs() == 10000);
  assert(camera.downloadTimeoutMs() == 20000);

  reduceFromFull(camera);
  for (uint32_t now = 90000; now <= 300000; now += 30000)
  {
    success(camera, now, 2000, 100 * 1024, 500);
    assert(camera.level() == 1);
  }
  for (uint32_t now = 330000; now <= 750000 && camera.level() == 1; now += 30000)
  {
    success(camera, now, 400, 100 * 1024, 100);
  }
  assert(camera.level() == 0);

  camera.reset();
  reduceFromFull(camera);
  for (uint32_t now = 90000; now <= 930000; now += 30000)
  {
    success(camera, now, 2000, 100 * 1024, 500);
    assert(camera.level() == 1);
  }
  for (uint32_t now = 960000; now <= 1050000; now += 30000)
  {
    success(camera, now, 2000, 100 * 1024, 500);
  }
  assert(camera.level() == 0);

  camera.reset();
  reduceFromFull(camera);
  success(camera, 90000, 14000, 100 * 1024);
  assert(camera.level() == 1);
  success(camera, 120000, 14000, 100 * 1024);
  assert(camera.level() == 2);
  assert(camera.resolution().width == 640 && camera.resolution().height == 480);
  for (uint32_t now = 150000; now <= 210000; now += 30000)
  {
    success(camera, now, 1000, 50 * 1024, 500);
    assert(camera.level() == 2);
  }
  success(camera, 240000, 1000, 50 * 1024, 500);
  assert(camera.level() == 1);

  camera.reset();
  camera.transferFailure(30000, 30000, true, 800 * 1024);
  camera.transferFailure(60000, 30000, true, 800 * 1024);
  assert(camera.level() == 1);
  for (uint32_t now = 90000; now <= 300000; now += 30000)
  {
    success(camera, now, 2000, 100 * 1024, 500);
    assert(camera.level() == 1);
  }

  camera.reset();
  camera.transferFailure(30000, 30000, true);
  camera.transferFailure(60000, 30000, true);
  assert(camera.level() == 1);
  for (uint32_t now = 90000; now <= 930000; now += 30000)
  {
    success(camera, now, 2000, 100 * 1024, 500);
    assert(camera.level() == 1);
  }
  for (uint32_t now = 960000; now <= 1050000; now += 30000)
  {
    success(camera, now, 2000, 100 * 1024, 500);
  }
  assert(camera.level() == 0);

  camera.reset();
  success(camera, 30000, 14000, 800 * 1024);
  camera.transferFailure(60000, 5000, false);
  success(camera, 90000, 14000, 800 * 1024);
  assert(camera.level() == 0);
  success(camera, 120000, 14000, 800 * 1024);
  assert(camera.level() == 1);

  camera.reset();
  for (uint32_t now = 30000; now <= 300000; now += 60000)
  {
    success(camera, now, 14000, 800 * 1024);
    success(camera, now + 30000, 2000, 800 * 1024, 500);
  }
  assert(camera.level() == 0);
  camera.transferFailure(330000, 5000, false);
  camera.transferFailure(360000, 5000, false);
  assert(camera.level() == 0);

  camera.reset();
  success(camera, 1000, 1000, 800 * 1024, 250);
  assert(camera.downloadTimeoutMs() == 15000);
  for (uint32_t now = 31000; now <= 361000; now += 30000)
  {
    camera.success(now, 60000, 60000, 60000, 800 * 1024);
  }
  assert(camera.connectTimeoutMs() == 10000);
  assert(camera.headerTimeoutMs() == 20000);
  assert(camera.downloadTimeoutMs() == 30000);

  camera.reset();
  success(camera, UINT32_MAX - 10000, 14000, 800 * 1024);
  success(camera, 19999, 14000, 800 * 1024);
  success(camera, 49999, 14000, 800 * 1024);
  assert(camera.level() == 1);

  std::puts("camera adaptive tests passed");
}
