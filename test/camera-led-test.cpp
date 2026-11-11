#include "../CameraLed.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

int main()
{
  CameraLed led;
  using Color = CameraLed::Color;
  assert(led.update(false, 0, 100000) == Color::Off);
  assert(led.update(true, 1000, 1000) == Color::Green);
  assert(led.update(true, 1000, 36000) == Color::Green);
  assert(led.update(true, 1000, 36001) == Color::Blue);
  assert(led.update(true, 1000, 46000) == Color::Blue);
  assert(led.update(true, 1000, 46001) == Color::Red);
  assert(led.update(true, 1000, 999) == Color::Red);
  assert(led.update(true, 50000, 50000) == Color::Green);
  assert(led.update(false, 0, 50000) == Color::Off);
  assert(led.update(true, UINT32_MAX - 10000, 19999) == Color::Green);
  assert(led.update(true, UINT32_MAX - 10000, 35000) == Color::Red);
  std::puts("camera LED tests passed");
}
