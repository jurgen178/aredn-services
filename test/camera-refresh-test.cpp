#include "../CameraRefresh.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

int main()
{
  CameraRefresh refresh;
  uint32_t interval = 0;
  assert(!refresh.interval(interval));
  refresh.recordSuccess(1000);
  assert(!refresh.interval(interval));
  refresh.recordSuccess(32000);
  assert(refresh.interval(interval) && interval == 31000);
  refresh.recordSuccess(125000);
  assert(refresh.interval(interval) && interval == 93000);

  refresh.clear();
  assert(!refresh.interval(interval));
  refresh.recordSuccess(UINT32_MAX - 10000);
  assert(!refresh.interval(interval));
  refresh.recordSuccess(19999);
  assert(refresh.interval(interval) && interval == 30000);
  std::puts("camera refresh tests passed");
}
