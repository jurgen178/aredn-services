#pragma once

#include <stdint.h>

class CameraLed
{
public:
  enum class Color : uint8_t { Off, Green, Blue, Red };

  Color update(bool imageAvailable, uint32_t lastSuccessMs, uint32_t nowMs)
  {
    if (!imageAvailable)
    {
      haveImage_ = false;
      color_ = Color::Off;
      return color_;
    }

    if (!haveImage_ || lastSuccessMs != lastSuccessMs_)
    {
      haveImage_ = true;
      lastSuccessMs_ = lastSuccessMs;
      color_ = Color::Green;
    }
    // Keep an overdue image red even after millis() wraps around.
    if (color_ == Color::Red)
      return color_;

    const uint32_t ageMs = nowMs - lastSuccessMs;
    color_ = ageMs <= 35000 ? Color::Green
        : ageMs <= 45000 ? Color::Blue : Color::Red;
    
    return color_;
  }

private:
  bool haveImage_ = false;
  uint32_t lastSuccessMs_ = 0;
  Color color_ = Color::Off;
};
