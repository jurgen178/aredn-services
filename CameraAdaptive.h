#pragma once

#include <stdint.h>

namespace camera_adaptive_detail
{
  struct Resolution
  {
    uint16_t width;
    uint16_t height;
  };

  constexpr Resolution RESOLUTIONS[] = {
      {0, 0},
      {896, 672},
      {640, 480}};
}

class CameraAdaptive
{
public:
  using Resolution = camera_adaptive_detail::Resolution;
  static constexpr uint8_t LEVELS =
      sizeof(camera_adaptive_detail::RESOLUTIONS) /
      sizeof(camera_adaptive_detail::RESOLUTIONS[0]);

  explicit CameraAdaptive(uint32_t intervalMs) : intervalMs_(intervalMs) {}

  void reset()
  {
    level_ = 0;
    pressureMs_ = 0;
    haveObservation_ = false;
    haveChange_ = false;
    headerAverageMs_ = 0;
    downloadAverageMs_ = 0;
    bytesPerSecond_ = 0;
    for (uint8_t i = 0; i < LEVELS; ++i) imageBytes_[i] = 0;
  }

  uint8_t level() const { return level_; }

  Resolution resolution() const
  {
    return camera_adaptive_detail::RESOLUTIONS[level_];
  }

  static bool resolutionAt(uint8_t level, Resolution& result)
  {
    if (level >= LEVELS) return false;
    result = camera_adaptive_detail::RESOLUTIONS[level];
    return true;
  }

  uint32_t connectTimeoutMs() const
  {
    return headerAverageMs_ == 0 ? 5000
        : clamp(2000 + headerAverageMs_ * 2, 5000, 10000);
  }

  uint32_t headerTimeoutMs() const
  {
    return headerAverageMs_ == 0 ? 10000
        : clamp(2000 + headerAverageMs_ * 4, 10000, 20000);
  }

  uint32_t downloadTimeoutMs() const
  {
    return downloadAverageMs_ == 0 ? 20000
        : clamp(5000 + downloadAverageMs_ * 3, 15000, 30000);
  }

  void success(uint32_t now, uint32_t totalMs,
               uint32_t headerMs, uint32_t downloadMs, uint32_t imageBytes)
  {
    updateAverage(headerAverageMs_, headerMs);
    updateAverage(downloadAverageMs_, downloadMs);
    updateImageSize(imageBytes);
    uint32_t sampleRate = 0;
    if (imageBytes > 0)
    {
      sampleRate = static_cast<uint32_t>(
          static_cast<uint64_t>(imageBytes) * 1000 / (downloadMs == 0 ? 1 : downloadMs));
      bytesPerSecond_ = bytesPerSecond_ == 0 ? sampleRate
          : static_cast<uint32_t>((static_cast<uint64_t>(bytesPerSecond_) * 3 + sampleRate) / 4);
    }
    int direction = totalMs > intervalMs_ * 2 / 5 ? 1
        : totalMs < intervalMs_ / 4 ? -1 : 0;
    if (direction < 0 && level_ > 0 && !canIncrease(now, sampleRate))
    {
      direction = 0;
    }
    observe(now, totalMs, direction);
  }

  void transferFailure(uint32_t now, uint32_t durationMs, bool hasEvidence,
                       uint32_t expectedBytes = 0)
  {
    updateImageSize(expectedBytes);
    if (hasEvidence)
    {
      observe(now, durationMs, 1);
    }
    else
    {
      pressureMs_ = 0;
      lastObservationMs_ = now;
      haveObservation_ = true;
    }
  }

private:
  const uint32_t intervalMs_;
  uint8_t level_ = 0;
  int32_t pressureMs_ = 0;
  uint32_t lastObservationMs_ = 0;
  uint32_t lastChangeMs_ = 0;
  uint32_t headerAverageMs_ = 0;
  uint32_t downloadAverageMs_ = 0;
  uint32_t bytesPerSecond_ = 0;
  uint32_t imageBytes_[LEVELS]{};
  bool haveObservation_ = false;
  bool haveChange_ = false;

  static uint32_t clamp(uint32_t value, uint32_t minimum, uint32_t maximum)
  {
    return value < minimum ? minimum : value > maximum ? maximum : value;
  }

  static void updateAverage(uint32_t& average, uint32_t sample)
  {
    sample = clamp(sample, 1, 60000);
    average = average == 0 ? sample : (average * 3 + sample) / 4;
  }

  void updateImageSize(uint32_t bytes)
  {
    if (bytes == 0) return;
    uint32_t& previous = imageBytes_[level_];
    previous = previous == 0 ? bytes
        : static_cast<uint32_t>((static_cast<uint64_t>(previous) * 3 + bytes) / 4);
  }

  bool canIncrease(uint32_t now, uint32_t sampleRate) const
  {
    const uint32_t targetBytes = imageBytes_[level_ - 1];
    const uint32_t rate = sampleRate < bytesPerSecond_ ? sampleRate : bytesPerSecond_;
    if (targetBytes > 0 && rate > 0)
    {
      const uint64_t predictedMs = headerAverageMs_ +
          (static_cast<uint64_t>(targetBytes) * 1000 + rate - 1) / rate;
      if (predictedMs < intervalMs_ / 4) return true;
    }
    // Recheck occasionally: an old size estimate can become stale as the scene changes.
    return haveChange_ && now - lastChangeMs_ >= intervalMs_ * 30;
  }

  void observe(uint32_t now, uint32_t durationMs, int direction)
  {
    const uint32_t elapsed = haveObservation_
        ? clamp(now - lastObservationMs_, 0, intervalMs_)
        : clamp(durationMs, 0, intervalMs_);
    lastObservationMs_ = now;
    haveObservation_ = true;
    if ((level_ == 0 && direction < 0) ||
        (level_ + 1 == LEVELS && direction > 0))
    {
      pressureMs_ = 0;
      return;
    }
    const int32_t amount = static_cast<int32_t>(elapsed);
    if (direction > 0)
    {
      pressureMs_ += amount;
    }
    else if (direction < 0)
    {
      pressureMs_ -= amount;
    }
    else if (pressureMs_ > 0)
    {
      pressureMs_ = pressureMs_ > amount ? pressureMs_ - amount : 0;
    }
    else if (pressureMs_ < 0)
    {
      pressureMs_ = pressureMs_ < -amount ? pressureMs_ + amount : 0;
    }

    const int32_t downThreshold = static_cast<int32_t>(intervalMs_ * 2);
    const int32_t upThreshold = static_cast<int32_t>(intervalMs_ * 4);
    if (pressureMs_ > downThreshold) pressureMs_ = downThreshold;
    if (pressureMs_ < -upThreshold) pressureMs_ = -upThreshold;
    if (haveChange_ && now - lastChangeMs_ < intervalMs_ * 2) return;

    if (pressureMs_ >= downThreshold && level_ + 1 < LEVELS)
    {
      ++level_;
    }
    else if (pressureMs_ <= -upThreshold && level_ > 0)
    {
      --level_;
    }
    else
    {
      return;
    }
    pressureMs_ = 0;
    lastChangeMs_ = now;
    haveChange_ = true;
  }
};
