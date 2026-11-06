#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

class MeshScores
{
public:
  static constexpr size_t CAPACITY = 10;
  static constexpr size_t NAME_LENGTH = 16;
  static constexpr uint32_t MAX_SCORE = 1000000000;
  struct Entry { char name[NAME_LENGTH + 1]; uint32_t score; };
  struct List { uint32_t version; uint32_t count; Entry entries[CAPACITY]; };
  enum class Result { Ok, Invalid, Unavailable };

  bool begin()
  {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_ || !preferences_.begin("mesh-scores", false)) return false;
    const size_t size = preferences_.getBytesLength("scores");
    if (size == 0) { available_ = true; return true; }
    List saved{};
    if (size != sizeof(saved) || preferences_.getBytes("scores", &saved, sizeof(saved)) != sizeof(saved) ||
        !valid(saved)) return false;
    list_ = saved;
    available_ = true;
    return true;
  }

  static bool validName(const char* name)
  {
    size_t i = 0;
    for (; i <= NAME_LENGTH && name[i]; ++i)
    {
      const char c = name[i];
      if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return false;
    }
    return i > 0 && i <= NAME_LENGTH;
  }

  static bool sameName(const char* a, const char* b)
  {
    while (*a && *b)
    {
      char x = *a++, y = *b++;
      if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
      if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
      if (x != y) return false;
    }
    return *a == *b;
  }

  static bool valid(const List& list)
  {
    if (list.version != 1 || list.count > CAPACITY) return false;
    for (size_t i = 0; i < list.count; ++i)
    {
      if (!memchr(list.entries[i].name, '\0', sizeof(list.entries[i].name)) ||
          !validName(list.entries[i].name) || list.entries[i].score > MAX_SCORE ||
          (i && list.entries[i - 1].score < list.entries[i].score)) return false;
      for (size_t j = 0; j < i; ++j)
        if (sameName(list.entries[i].name, list.entries[j].name)) return false;
    }
    return true;
  }

  Result read(List& out)
  {
    if (!available_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return Result::Unavailable;
    out = list_;
    xSemaphoreGive(mutex_);
    return Result::Ok;
  }

  Result replace(const List& candidate)
  {
    if (!valid(candidate)) return Result::Invalid;
    if (!available_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return Result::Unavailable;
    const bool saved = save(candidate);
    xSemaphoreGive(mutex_);
    return saved ? Result::Ok : Result::Unavailable;
  }

  Result submit(const char* name, uint32_t score)
  {
    if (!validName(name) || score > MAX_SCORE) return Result::Invalid;
    if (!available_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return Result::Unavailable;
    List next = list_;
    size_t index = next.count;
    for (size_t i = 0; i < next.count; ++i)
      if (sameName(name, next.entries[i].name)) { index = i; break; }
    if ((index < next.count && score <= next.entries[index].score) ||
        (index == CAPACITY && score <= next.entries[CAPACITY - 1].score))
    {
      xSemaphoreGive(mutex_);
      return Result::Ok;
    }
    if (index == next.count && next.count < CAPACITY) ++next.count;
    else if (index == CAPACITY) index = CAPACITY - 1;
    Entry entry{};
    memcpy(entry.name, name, strlen(name) + 1);
    entry.score = score;
    while (index > 0 && next.entries[index - 1].score < score)
    {
      next.entries[index] = next.entries[index - 1];
      --index;
    }
    next.entries[index] = entry;
    const bool saved = save(next);
    xSemaphoreGive(mutex_);
    return saved ? Result::Ok : Result::Unavailable;
  }

private:
  bool save(const List& candidate)
  {
    if (preferences_.putBytes("scores", &candidate, sizeof(candidate)) != sizeof(candidate)) return false;
    list_ = candidate;
    return true;
  }
  Preferences preferences_;
  SemaphoreHandle_t mutex_ = nullptr;
  bool available_ = false;
  List list_{1, 0, {}};
};
