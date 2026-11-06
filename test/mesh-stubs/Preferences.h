#pragma once
#include <cstddef>
#include <cstring>
#include <vector>

class Preferences
{
public:
  static inline std::vector<unsigned char> data{};
  static inline bool failWrites = false;
  bool begin(const char*, bool) { return true; }
  size_t getBytesLength(const char*) { return data.size(); }
  size_t getBytes(const char*, void* target, size_t length)
  {
    if (length != data.size()) return 0;
    std::memcpy(target, data.data(), length);
    return length;
  }
  size_t putBytes(const char*, const void* source, size_t length)
  {
    if (failWrites) return 0;
    const auto* bytes = static_cast<const unsigned char*>(source);
    data.assign(bytes, bytes + length);
    return length;
  }
};
