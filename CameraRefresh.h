#pragma once

#include <atomic>
#include <stdint.h>

class CameraRefresh
{
public:
  // The caller serializes recordSuccess() and clear(); readers need no lock.
  void recordSuccess(uint32_t nowMs)
  {
    if (havePrevious_)
    {
      intervalMs_.store(nowMs - previousMs_, std::memory_order_release);
      available_.store(true, std::memory_order_release);
    }
    previousMs_ = nowMs;
    havePrevious_ = true;
  }

  void clear()
  {
    available_.store(false, std::memory_order_release);
    havePrevious_ = false;
  }

  bool interval(uint32_t& intervalMs) const
  {
    if (!available_.load(std::memory_order_acquire)) return false;
    intervalMs = intervalMs_.load(std::memory_order_acquire);
    return true;
  }

private:
  uint32_t previousMs_ = 0;
  bool havePrevious_ = false;
  std::atomic<uint32_t> intervalMs_{0};
  std::atomic<bool> available_{false};
};
