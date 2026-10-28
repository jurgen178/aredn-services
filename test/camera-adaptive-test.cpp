#include "../CameraAdaptive.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

static void success(CameraAdaptive& camera, uint32_t now, uint32_t duration)
{
  camera.success(now, duration, duration / 4, duration * 3 / 4);
}

int main()
{
  CameraAdaptive camera(30000);
  CameraAdaptive::Resolution resolution{};
  assert(CameraAdaptive::resolutionAt(0, resolution));
  assert(resolution.width == 0 && resolution.height == 0);
  assert(!CameraAdaptive::resolutionAt(CameraAdaptive::LEVELS, resolution));
  assert(camera.level() == 0 && camera.resolution().width == 0 && camera.resolution().height == 0);
  assert(camera.connectTimeoutMs() == 5000);
  assert(camera.headerTimeoutMs() == 10000);
  assert(camera.downloadTimeoutMs() == 20000);

  for (uint32_t now = 30000; now <= 150000; now += 30000)
  {
    success(camera, now, 2000);
  }
  assert(camera.level() == 0);
  success(camera, 180000, 14000);
  assert(camera.level() == 0);
  success(camera, 210000, 14000);
  assert(camera.level() == 1 && camera.resolution().width == 1920 && camera.resolution().height == 1440);

  success(camera, 240000, 14000);
  assert(camera.level() == 1);
  success(camera, 270000, 14000);
  assert(camera.level() == 2 && camera.resolution().width == 1280 && camera.resolution().height == 960);
  success(camera, 300000, 14000);
  success(camera, 330000, 14000);
  assert(camera.level() == 3 && camera.resolution().width == 960 && camera.resolution().height == 720);
  success(camera, 360000, 14000);
  success(camera, 390000, 14000);
  assert(camera.level() == 4 && camera.resolution().width == 640 && camera.resolution().height == 480);
  for (uint32_t now = 420000; now <= 510000; now += 30000)
  {
    success(camera, now, 14000);
  }
  assert(camera.level() == 4);

  success(camera, 540000, 2000);
  success(camera, 570000, 2000);
  success(camera, 600000, 2000);
  assert(camera.level() == 4);
  success(camera, 630000, 2000);
  assert(camera.level() == 3);

  camera.reset();
  success(camera, 30000, 14000);
  camera.transferFailure(60000, 5000, false);
  success(camera, 90000, 14000);
  assert(camera.level() == 0);
  success(camera, 120000, 14000);
  assert(camera.level() == 1);

  camera.reset();
  camera.transferFailure(20000, 20000, true);
  camera.transferFailure(43000, 20000, true);
  assert(camera.level() == 0);
  camera.transferFailure(69000, 20000, true);
  assert(camera.level() == 1);

  camera.reset();
  for (uint32_t now = 30000; now <= 300000; now += 60000)
  {
    success(camera, now, 14000);
    success(camera, now + 30000, 2000);
  }
  assert(camera.level() == 0);
  for (uint32_t now = 360000; now <= 480000; now += 30000)
  {
    success(camera, now, 10000);
  }
  assert(camera.level() == 0);
  camera.transferFailure(510000, 5000, false);
  camera.transferFailure(540000, 5000, false);
  camera.transferFailure(570000, 5000, false);
  assert(camera.level() == 0);

  camera.reset();
  camera.transferFailure(30000, 30000, true);
  camera.transferFailure(300000, 30000, true);
  assert(camera.level() == 1);
  camera.transferFailure(330000, 30000, true);
  assert(camera.level() == 1);
  camera.transferFailure(360000, 30000, true);
  assert(camera.level() == 2);

  camera.reset();
  success(camera, 1000, 1000);
  assert(camera.downloadTimeoutMs() == 15000);
  camera.success(31000, 60000, 60000, 60000);
  assert(camera.connectTimeoutMs() >= 5000 && camera.connectTimeoutMs() <= 10000);
  assert(camera.headerTimeoutMs() >= 10000 && camera.headerTimeoutMs() <= 20000);
  assert(camera.downloadTimeoutMs() >= 15000 && camera.downloadTimeoutMs() <= 30000);
  for (uint32_t now = 61000; now <= 361000; now += 30000)
  {
    camera.success(now, 60000, 60000, 60000);
  }
  assert(camera.connectTimeoutMs() == 10000);
  assert(camera.headerTimeoutMs() == 20000);
  assert(camera.downloadTimeoutMs() == 30000);

  camera.reset();
  success(camera, UINT32_MAX - 10000, 14000);
  success(camera, 19999, 14000);
  success(camera, 49999, 14000);
  assert(camera.level() == 1);

  std::puts("camera adaptive tests passed");
}
