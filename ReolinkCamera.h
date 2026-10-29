#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <memory>
#include <new>
#include <cstring>
#include <cstdlib>
#include <time.h>
#include "CameraAdaptive.h"
#include "CameraRefresh.h"

class ReolinkCamera
{
public:
  static constexpr uint32_t SNAPSHOT_INTERVAL_MS = 30000;
  static constexpr uint32_t SNAPSHOT_BUSY_RETRY_MS = 5000;
  // A failed attempt already takes 5-10 s, so a short cap keeps recovery quick.
  static constexpr uint32_t MAX_FAILURE_RETRY_MS = 10000;
  static constexpr uint32_t IMAGE_ACK_TIMEOUT_MS = 15000;
  static constexpr uint8_t MAX_BUSY_RETRIES = 12;
  static constexpr size_t MAX_JPEG_SIZE = 2 * 1024 * 1024;
  static constexpr uint32_t MIN_VALID_CAPTURE_EPOCH = 1577836800UL;

  struct StatusSnapshot
  {
    const char* status;
    const char* error;
    int16_t httpStatus;
    bool imageAvailable;
    uint32_t version;
    uint32_t ageSeconds;
    uint32_t refreshSeconds;
    uint32_t captureEpoch;
    uint32_t accessCount;
    uint8_t resolutionLevel;
    uint32_t lastFetchMs;
    uint32_t lastFetchBytes;
    uint32_t downloadTimeoutMs;
  };

  bool begin()
  {
    configMutex_ = xSemaphoreCreateMutex();
    snapshotMutex_ = xSemaphoreCreateMutex();
    if (configMutex_ == nullptr || snapshotMutex_ == nullptr)
    {
      state_.store(State::StorageError, std::memory_order_release);
      Serial.println("ERROR: Could not create camera service mutexes");
      return false;
    }

    if (!preferences_.begin("aredn-camera", false))
    {
      state_.store(State::StorageError, std::memory_order_release);
      Serial.println("ERROR: Could not open camera configuration storage");
      return false;
    }
    preferencesReady_ = true;

    const size_t storedSize = preferences_.getBytesLength("settings");
    if (storedSize != 0)
    {
      CameraSettings stored{};
      if (storedSize != sizeof(stored) ||
          preferences_.getBytes("settings", &stored, sizeof(stored)) != sizeof(stored) ||
          !isValidSettings(stored))
      {
        Serial.println("ERROR: Stored camera configuration is invalid");
        if (!preferences_.remove("settings"))
        {
          Serial.println("ERROR: Could not remove invalid camera configuration");
        }
      }
      else
      {
        settings_ = stored;
        configured_ = true;
      }
    }

    const BaseType_t taskResult = xTaskCreate(
        taskEntry, "camera-snapshot", 10240, this, 1, &taskHandle_);
    if (taskResult != pdPASS)
    {
      state_.store(State::StorageError, std::memory_order_release);
      Serial.println("ERROR: Could not start the camera snapshot task");
      return false;
    }
    if (configured_)
    {
      xTaskNotifyGive(taskHandle_);
    }
    else
    {
      state_.store(State::NotConfigured, std::memory_order_release);
    }
    return true;
  }

  void handleImage(AsyncWebServerRequest* request)
  {
    uint8_t index = 0;
    size_t imageSize = 0;
    if (snapshotMutex_ == nullptr ||
        xSemaphoreTake(snapshotMutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "text/plain; charset=utf-8", "Camera snapshot is busy");
      return;
    }
    if (activeBuffer_ < 0 || buffers_[activeBuffer_].size == 0)
    {
      xSemaphoreGive(snapshotMutex_);
      request->send(503, "text/plain; charset=utf-8", "Camera snapshot is not ready");
      return;
    }

    index = static_cast<uint8_t>(activeBuffer_);
    imageSize = buffers_[index].size;
    ++buffers_[index].readers;
    const uint8_t* imageData = buffers_[index].data;
    xSemaphoreGive(snapshotMutex_);

    std::shared_ptr<ImageLease> lease(
        new (std::nothrow) ImageLease(this, index, imageSize));
    if (!lease)
    {
      releaseReader(index);
      request->send(503, "text/plain; charset=utf-8", "Camera snapshot is busy");
      return;
    }

    AsyncWebServerResponse* response = request->beginResponse(
        "image/jpeg",
        imageSize,
        [lease, imageData, imageSize](uint8_t* output,
                                     size_t maxLength,
                                     size_t offset) -> size_t
        {
          if (offset >= imageSize)
          {
            return 0;
          }
          const size_t count = std::min(maxLength, imageSize - offset);
          memcpy(output, imageData + offset, count);
          lease->provided = offset + count;
          return count;
        });
    if (response == nullptr)
    {
      request->send(500, "text/plain; charset=utf-8", "Could not serve camera snapshot");
      return;
    }
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Camera-Refresh-Seconds",
                        String(SNAPSHOT_INTERVAL_MS / 1000));
    const uint32_t ackTimeout = request->client()->getAckTimeout();
    if (ackTimeout != 0 && ackTimeout < IMAGE_ACK_TIMEOUT_MS)
    {
      request->client()->setAckTimeout(IMAGE_ACK_TIMEOUT_MS);
    }
    request->send(response);
  }

  StatusSnapshot statusSnapshot(uint32_t pageAccessCount) const
  {
    const bool available = imageAvailable_.load(std::memory_order_acquire);
    const State currentState = state_.load(std::memory_order_acquire);
    const SnapshotError currentError = lastError_.load(std::memory_order_acquire);
    const uint32_t lastSuccess = lastSuccessMs_.load(std::memory_order_acquire);
    const int16_t httpStatus = lastHttpStatus_.load(std::memory_order_acquire);
    const uint32_t ageSeconds = available
        ? (millis() - lastSuccess) / 1000
        : 0;
    const time_t estimatedCaptureEpoch =
        time(nullptr) - static_cast<time_t>(ageSeconds);
    const uint32_t captureEpoch =
        available &&
                estimatedCaptureEpoch >= static_cast<time_t>(MIN_VALID_CAPTURE_EPOCH)
            ? static_cast<uint32_t>(estimatedCaptureEpoch)
            : 0;

    return {
        stateName(currentState, available),
        errorName(currentError),
        httpStatus,
        available,
        version_.load(std::memory_order_acquire),
        ageSeconds,
        SNAPSHOT_INTERVAL_MS / 1000,
        captureEpoch,
        pageAccessCount,
        resolutionLevel_.load(std::memory_order_acquire),
        lastFetchMs_.load(std::memory_order_acquire),
        lastFetchBytes_.load(std::memory_order_acquire),
        downloadTimeoutMs_.load(std::memory_order_acquire)};
  }

  void handleConfiguration(AsyncWebServerRequest* request) const
  {
    if (!preferencesReady_ || configMutex_ == nullptr ||
        xSemaphoreTake(configMutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "application/json",
                    "{\"error\":\"camera_storage_unavailable\"}");
      return;
    }

    char response[192];
    snprintf(response, sizeof(response),
             "{\"configured\":%s,\"host\":\"%s\",\"port\":%u,"
             "\"username\":\"%s\",\"password_set\":%s}",
             configured_ ? "true" : "false",
             settings_.host,
             settings_.port,
             settings_.username,
             configured_ ? "true" : "false");
    xSemaphoreGive(configMutex_);
    request->send(200, "application/json; charset=utf-8", response);
  }

  void saveConfiguration(AsyncWebServerRequest* request)
  {
    if (!preferencesReady_)
    {
      request->send(503, "application/json",
                    "{\"error\":\"camera_storage_unavailable\"}");
      return;
    }
    if (taskHandle_ == nullptr)
    {
      request->send(503, "application/json",
                    "{\"error\":\"camera_service_unavailable\"}");
      return;
    }
    if (!request->hasParam("host", true) ||
        !request->hasParam("port", true) ||
        !request->hasParam("username", true) ||
        !request->hasParam("password", true))
    {
      request->send(400, "application/json", "{\"error\":\"missing_camera_fields\"}");
      return;
    }

    const String hostValue = request->getParam("host", true)->value();
    const String portValue = request->getParam("port", true)->value();
    const String usernameValue = request->getParam("username", true)->value();
    const String passwordValue = request->getParam("password", true)->value();
    IPAddress hostAddress;
    if (!hostAddress.fromString(hostValue) ||
        hostAddress == IPAddress(0, 0, 0, 0) ||
        hostValue.length() >= sizeof(CameraSettings::host))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_camera_host\"}");
      return;
    }
    if (!isValidPort(portValue) ||
        usernameValue.isEmpty() ||
        usernameValue.length() >= sizeof(CameraSettings::username) ||
        !isValidUsername(usernameValue))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_camera_settings\"}");
      return;
    }
    if (passwordValue.length() >= sizeof(CameraSettings::password))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_camera_password\"}");
      return;
    }
    if (xSemaphoreTake(configMutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "application/json", "{\"error\":\"camera_config_busy\"}");
      return;
    }

    CameraSettings updated{};
    strlcpy(updated.host, hostAddress.toString().c_str(), sizeof(updated.host));
    updated.port = static_cast<uint16_t>(strtoul(portValue.c_str(), nullptr, 10));
    strlcpy(updated.username, usernameValue.c_str(), sizeof(updated.username));
    if (passwordValue.isEmpty() && configured_)
    {
      memcpy(updated.password, settings_.password, sizeof(updated.password));
    }
    else if (!passwordValue.isEmpty())
    {
      strlcpy(updated.password, passwordValue.c_str(), sizeof(updated.password));
    }
    if (updated.password[0] == '\0')
    {
      xSemaphoreGive(configMutex_);
      request->send(400, "application/json", "{\"error\":\"camera_password_required\"}");
      return;
    }

    if (preferences_.putBytes("settings", &updated, sizeof(updated)) != sizeof(updated))
    {
      xSemaphoreGive(configMutex_);
      request->send(500, "application/json", "{\"error\":\"camera_save_failed\"}");
      Serial.println("ERROR: Could not save camera configuration");
      return;
    }
    settings_ = updated;
    configured_ = true;
    configRevision_.fetch_add(1, std::memory_order_release);
    xSemaphoreGive(configMutex_);

    clearSnapshot();
    lastError_.store(SnapshotError::None, std::memory_order_release);
    lastHttpStatus_.store(0, std::memory_order_release);
    state_.store(State::Connecting, std::memory_order_release);
    notifyTask();
    request->send(202, "application/json", "{\"saved\":true}");
  }

  void clearConfiguration(AsyncWebServerRequest* request)
  {
    if (!preferencesReady_ || configMutex_ == nullptr ||
        xSemaphoreTake(configMutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "application/json",
                    "{\"error\":\"camera_storage_unavailable\"}");
      return;
    }
    if (configured_ && !preferences_.remove("settings"))
    {
      xSemaphoreGive(configMutex_);
      request->send(500, "application/json", "{\"error\":\"camera_clear_failed\"}");
      Serial.println("ERROR: Could not clear camera configuration");
      return;
    }
    memset(&settings_, 0, sizeof(settings_));
    configured_ = false;
    configRevision_.fetch_add(1, std::memory_order_release);
    xSemaphoreGive(configMutex_);

    clearSnapshot();
    lastError_.store(SnapshotError::None, std::memory_order_release);
    lastHttpStatus_.store(0, std::memory_order_release);
    state_.store(State::NotConfigured, std::memory_order_release);
    notifyTask();
    request->send(200, "application/json", "{\"cleared\":true}");
  }

  const char* displayStatus() const
  {
    const bool available = imageAvailable_.load(std::memory_order_acquire);
    // With an image, its age is shown separately, so short failures stay invisible.
    switch (state_.load(std::memory_order_acquire))
    {
      case State::StorageError: return "Storage error";
      case State::NotConfigured: return "Not set";
      case State::Waiting: return "Network off";
      case State::Connecting: return available ? "Available" : "Connecting";
      case State::Available: return "Available";
      case State::Error: return available ? "Available" : "Unavailable";
    }
    return "Unavailable";
  }

  uint32_t imageAgeSeconds() const
  {
    if (!imageAvailable_.load(std::memory_order_acquire))
    {
      return 0;
    }
    return (millis() - lastSuccessMs_.load(std::memory_order_acquire)) / 1000;
  }

  bool imageAvailable() const
  {
    return imageAvailable_.load(std::memory_order_acquire);
  }

  bool imageDimensions(uint16_t& width, uint16_t& height) const
  {
    const uint32_t dimensions = imageDimensions_.load(std::memory_order_acquire);
    width = static_cast<uint16_t>(dimensions >> 16);
    height = static_cast<uint16_t>(dimensions & 0xFFFF);
    return width != 0 && height != 0;
  }

  bool lastRefreshInterval(uint32_t& intervalMs) const
  {
    return refresh_.interval(intervalMs);
  }

  bool nextRequestedResolution(CameraAdaptive::Resolution& resolution) const
  {
    return CameraAdaptive::resolutionAt(
        resolutionLevel_.load(std::memory_order_acquire), resolution);
  }

  bool lastFetch(uint32_t& durationMs, uint32_t& bytes) const
  {
    if (!lastFetchAvailable_.load(std::memory_order_acquire))
    {
      return false;
    }
    durationMs = lastFetchMs_.load(std::memory_order_acquire);
    bytes = lastFetchBytes_.load(std::memory_order_acquire);
    return true;
  }

private:
  struct CameraSettings
  {
    char host[16]{};
    uint16_t port = 80;
    char username[33]{};
    char password[65]{};
  };

  struct SnapshotBuffer
  {
    uint8_t* data = nullptr;
    size_t size = 0;
    uint32_t readers = 0;
  };

  enum class State : uint8_t
  {
    StorageError,
    NotConfigured,
    Waiting,
    Connecting,
    Available,
    Error
  };

  enum class SnapshotError : uint8_t
  {
    None,
    NoPsram,
    MemoryAllocation,
    BufferBusy,
    UrlTooLong,
    ConnectionFailed,
    HttpFailed,
    SnapshotTooLarge,
    SnapshotTimeout,
    InvalidJpeg
  };

  enum class PollResult : uint8_t
  {
    Normal,
    Busy,
    TransientFailure
  };

  struct FetchMetrics
  {
    uint32_t totalMs = 0;
    uint32_t headerMs = 0;
    uint32_t downloadMs = 0;
    size_t received = 0;
    uint32_t expectedBytes = 0;
  };

  struct ImageLease
  {
    ImageLease(ReolinkCamera* owner, uint8_t bufferIndex, size_t imageSize)
        : camera(owner), index(bufferIndex), size(imageSize), startedAt(millis()) {}
    ~ImageLease()
    {
      Serial.printf("Camera image transfer ended: %lu/%lu bytes supplied to HTTP response in %lu ms\n",
                    static_cast<unsigned long>(provided),
                    static_cast<unsigned long>(size),
                    static_cast<unsigned long>(millis() - startedAt));
      camera->releaseReader(index);
    }
    ReolinkCamera* camera;
    uint8_t index;
    size_t size;
    uint32_t startedAt;
    size_t provided = 0;
  };

  Preferences preferences_;
  CameraSettings settings_{};
  SnapshotBuffer buffers_[2]{};
  SemaphoreHandle_t configMutex_ = nullptr;
  SemaphoreHandle_t snapshotMutex_ = nullptr;
  TaskHandle_t taskHandle_ = nullptr;
  int8_t activeBuffer_ = -1;
  uint8_t busyRetries_ = 0;
  bool preferencesReady_ = false;
  bool configured_ = false;
  std::atomic<State> state_{State::NotConfigured};
  std::atomic<SnapshotError> lastError_{SnapshotError::None};
  std::atomic<int16_t> lastHttpStatus_{0};
  std::atomic<bool> imageAvailable_{false};
  std::atomic<uint32_t> version_{0};
  std::atomic<uint32_t> lastSuccessMs_{0};
  std::atomic<uint32_t> imageDimensions_{0};
  std::atomic<uint32_t> configRevision_{0};
  std::atomic<uint8_t> resolutionLevel_{0};
  std::atomic<uint32_t> lastFetchMs_{0};
  std::atomic<uint32_t> lastFetchBytes_{0};
  std::atomic<bool> lastFetchAvailable_{false};
  std::atomic<uint32_t> downloadTimeoutMs_{20000};
  CameraAdaptive adaptive_{SNAPSHOT_INTERVAL_MS};
  CameraRefresh refresh_{};

  static void taskEntry(void* context)
  {
    static_cast<ReolinkCamera*>(context)->cameraTask();
  }

  void cameraTask()
  {
    uint32_t revision = configRevision_.load(std::memory_order_acquire);
    uint32_t retryDelayMs = 3000;
    for (;;)
    {
      const uint32_t currentRevision = configRevision_.load(std::memory_order_acquire);
      if (currentRevision != revision)
      {
        revision = currentRevision;
        adaptive_.reset();
        resolutionLevel_.store(0, std::memory_order_release);
        lastFetchMs_.store(0, std::memory_order_release);
        lastFetchBytes_.store(0, std::memory_order_release);
        lastFetchAvailable_.store(false, std::memory_order_release);
        downloadTimeoutMs_.store(adaptive_.downloadTimeoutMs(), std::memory_order_release);
        retryDelayMs = 3000;
        busyRetries_ = 0;
      }
      CameraSettings current{};
      if (!copySettings(current))
      {
        if (state_.load(std::memory_order_acquire) != State::StorageError)
        {
          state_.store(State::NotConfigured, std::memory_order_release);
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        continue;
      }
      if (WiFi.status() != WL_CONNECTED)
      {
        state_.store(State::Waiting, std::memory_order_release);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        continue;
      }

      const TickType_t pollStartedAt = xTaskGetTickCount();
      const PollResult result = pollSnapshot(current, revision);
      const TickType_t elapsed = xTaskGetTickCount() - pollStartedAt;
      if (result == PollResult::TransientFailure)
      {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(retryDelayMs));
        retryDelayMs = std::min(retryDelayMs * 2, MAX_FAILURE_RETRY_MS);
      }
      else if (result == PollResult::Busy)
      {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SNAPSHOT_BUSY_RETRY_MS));
      }
      else
      {
        retryDelayMs = 3000;
        const TickType_t interval = pdMS_TO_TICKS(SNAPSHOT_INTERVAL_MS);
        if (elapsed < interval)
        {
          ulTaskNotifyTake(pdTRUE, interval - elapsed);
        }
      }
    }
  }

  bool copySettings(CameraSettings& output)
  {
    if (configMutex_ == nullptr ||
        xSemaphoreTake(configMutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      return false;
    }
    const bool available = configured_;
    if (available)
    {
      output = settings_;
    }
    xSemaphoreGive(configMutex_);
    return available;
  }

  bool isCurrentSettings(const CameraSettings& expected)
  {
    if (xSemaphoreTake(configMutex_, portMAX_DELAY) != pdTRUE)
    {
      return false;
    }
    const bool matches = configured_ &&
        settings_.port == expected.port &&
        strcmp(settings_.host, expected.host) == 0 &&
        strcmp(settings_.username, expected.username) == 0 &&
        strcmp(settings_.password, expected.password) == 0;
    xSemaphoreGive(configMutex_);
    return matches;
  }

  PollResult pollSnapshot(const CameraSettings& settings, uint32_t revision)
  {
    const State previousState = state_.load(std::memory_order_acquire);
    state_.store(State::Connecting, std::memory_order_release);
    uint8_t targetIndex = 0;
    uint8_t* target = reserveTarget(targetIndex);
    if (target == nullptr)
    {
      const SnapshotError reserveError =
          lastError_.load(std::memory_order_acquire);
      // A slow viewer may still be reading the spare buffer. Keep the current
      // image and state and retry shortly; report an error only if it persists.
      if (reserveError == SnapshotError::BufferBusy &&
          ++busyRetries_ < MAX_BUSY_RETRIES)
      {
        state_.store(previousState, std::memory_order_release);
        return PollResult::Busy;
      }
      fail(reserveError == SnapshotError::None
               ? SnapshotError::BufferBusy
               : reserveError);
      return PollResult::Normal;
    }
    busyRetries_ = 0;

    size_t imageSize = 0;
    SnapshotError error = SnapshotError::None;
    FetchMetrics metrics{};
    const uint8_t requestLevel = adaptive_.level();
    const bool fetched = fetchSnapshot(settings, target, imageSize, error, metrics);
    if (revision != configRevision_.load(std::memory_order_acquire))
    {
      return PollResult::Busy;
    }
    lastFetchMs_.store(metrics.totalMs, std::memory_order_release);
    lastFetchBytes_.store(static_cast<uint32_t>(metrics.received), std::memory_order_release);
    lastFetchAvailable_.store(true, std::memory_order_release);
    if (!fetched)
    {
      const bool transferEvidence =
          (error == SnapshotError::SnapshotTimeout && metrics.received > 0) ||
          error == SnapshotError::SnapshotTooLarge;
      const bool expectedImageSize =
          error == SnapshotError::SnapshotTooLarge ||
          (error == SnapshotError::SnapshotTimeout && metrics.received >= 2 &&
           target[0] == 0xFF && target[1] == 0xD8);
      adaptive_.transferFailure(millis(), metrics.totalMs, transferEvidence,
                                expectedImageSize ? metrics.expectedBytes : 0);
      reportAdaptive(requestLevel, metrics);
      fail(error);
      const int16_t httpStatus = lastHttpStatus_.load(std::memory_order_acquire);
      return error == SnapshotError::ConnectionFailed ||
              error == SnapshotError::SnapshotTimeout ||
              error == SnapshotError::InvalidJpeg ||
              error == SnapshotError::SnapshotTooLarge ||
              (error == SnapshotError::HttpFailed &&
               (httpStatus == 408 || httpStatus == 429 || httpStatus >= 500))
          ? PollResult::TransientFailure : PollResult::Normal;
    }
    uint16_t imageWidth = 0;
    uint16_t imageHeight = 0;
    const bool dimensionsAvailable =
        readJpegDimensions(target, imageSize, imageWidth, imageHeight);

    if (xSemaphoreTake(snapshotMutex_, portMAX_DELAY) != pdTRUE)
    {
      fail(SnapshotError::BufferBusy);
      return PollResult::Busy;
    }
    if (!isCurrentSettings(settings))
    {
      xSemaphoreGive(snapshotMutex_);
      return PollResult::Busy;
    }
    buffers_[targetIndex].size = imageSize;
    activeBuffer_ = static_cast<int8_t>(targetIndex);
    const uint32_t dimensions = dimensionsAvailable
        ? (static_cast<uint32_t>(imageWidth) << 16) | imageHeight
        : 0;
    imageDimensions_.store(dimensions, std::memory_order_release);
    const uint32_t successMs = millis();
    refresh_.recordSuccess(successMs);
    lastSuccessMs_.store(successMs, std::memory_order_release);
    version_.fetch_add(1, std::memory_order_acq_rel);
    imageAvailable_.store(true, std::memory_order_release);
    xSemaphoreGive(snapshotMutex_);
    adaptive_.success(millis(), metrics.totalMs, metrics.headerMs, metrics.downloadMs,
                      static_cast<uint32_t>(imageSize));
    reportAdaptive(requestLevel, metrics);
    lastError_.store(SnapshotError::None, std::memory_order_release);
    state_.store(State::Available, std::memory_order_release);
    return PollResult::Normal;
  }

  void reportAdaptive(uint8_t requestLevel, const FetchMetrics& metrics)
  {
    resolutionLevel_.store(adaptive_.level(), std::memory_order_release);
    downloadTimeoutMs_.store(adaptive_.downloadTimeoutMs(), std::memory_order_release);
    Serial.printf("Camera fetch: level %u, %lu bytes, total %lu ms, headers %lu ms, "
                  "download %lu ms; next level %u, download timeout %lu ms\n",
                  static_cast<unsigned>(requestLevel),
                  static_cast<unsigned long>(metrics.received),
                  static_cast<unsigned long>(metrics.totalMs),
                  static_cast<unsigned long>(metrics.headerMs),
                  static_cast<unsigned long>(metrics.downloadMs),
                  static_cast<unsigned>(adaptive_.level()),
                  static_cast<unsigned long>(adaptive_.downloadTimeoutMs()));
  }

  uint8_t* reserveTarget(uint8_t& index)
  {
    if (!ensureBuffers())
    {
      return nullptr;
    }
    if (xSemaphoreTake(snapshotMutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      lastError_.store(SnapshotError::BufferBusy, std::memory_order_release);
      return nullptr;
    }

    index = activeBuffer_ == 0 ? 1 : 0;
    if (buffers_[index].readers != 0)
    {
      xSemaphoreGive(snapshotMutex_);
      lastError_.store(SnapshotError::BufferBusy, std::memory_order_release);
      return nullptr;
    }
    buffers_[index].size = 0;
    uint8_t* result = buffers_[index].data;
    xSemaphoreGive(snapshotMutex_);
    return result;
  }

  bool ensureBuffers()
  {
    if (buffers_[0].data != nullptr && buffers_[1].data != nullptr)
    {
      return true;
    }
    if (!psramFound())
    {
      lastError_.store(SnapshotError::NoPsram, std::memory_order_release);
      return false;
    }

    uint8_t* first = static_cast<uint8_t*>(
        heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    uint8_t* second = static_cast<uint8_t*>(
        heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (first == nullptr || second == nullptr)
    {
      if (first != nullptr)
      {
        heap_caps_free(first);
      }
      if (second != nullptr)
      {
        heap_caps_free(second);
      }
      lastError_.store(SnapshotError::MemoryAllocation, std::memory_order_release);
      return false;
    }
    buffers_[0].data = first;
    buffers_[1].data = second;
    return true;
  }

  bool fetchSnapshot(const CameraSettings& settings,
                     uint8_t* target,
                     size_t& imageSize,
                     SnapshotError& error,
                     FetchMetrics& metrics)
  {
    const uint32_t startedAt = millis();
    lastHttpStatus_.store(0, std::memory_order_release);
    char encodedUsername[sizeof(settings.username) * 3]{};
    char encodedPassword[sizeof(settings.password) * 3]{};
    if (!urlEncode(settings.username, encodedUsername, sizeof(encodedUsername)) ||
        !urlEncode(settings.password, encodedPassword, sizeof(encodedPassword)))
    {
      error = SnapshotError::UrlTooLong;
      return false;
    }

    char url[512];
    const uint32_t requestNonce = esp_random();
    const int urlLength = snprintf(
        url, sizeof(url),
        "http://%s:%u/cgi-bin/api.cgi?cmd=Snap&channel=0&rs=%08lx"
        "&user=%s&password=%s",
        settings.host,
        settings.port,
        static_cast<unsigned long>(requestNonce),
        encodedUsername,
        encodedPassword);
    if (urlLength <= 0 || static_cast<size_t>(urlLength) >= sizeof(url))
    {
      error = SnapshotError::UrlTooLong;
      return false;
    }
    if (adaptive_.level() != 0)
    {
      const CameraAdaptive::Resolution resolution = adaptive_.resolution();
      const int added = snprintf(url + urlLength, sizeof(url) - urlLength,
                                 "&width=%u&height=%u",
                                 static_cast<unsigned>(resolution.width),
                                 static_cast<unsigned>(resolution.height));
      if (added < 0 || static_cast<size_t>(added) >= sizeof(url) - urlLength)
      {
        error = SnapshotError::UrlTooLong;
        return false;
      }
    }

    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(adaptive_.connectTimeoutMs());
    http.setTimeout(static_cast<uint16_t>(adaptive_.headerTimeoutMs()));
    http.setReuse(false);
    http.useHTTP10(true);
    if (!http.begin(client, url))
    {
      error = SnapshotError::ConnectionFailed;
      return false;
    }
    http.addHeader("Accept", "image/jpeg");
    const int responseCode = http.GET();
    metrics.headerMs = millis() - startedAt;
    metrics.totalMs = metrics.headerMs;
    lastHttpStatus_.store(
        responseCode > 0 ? static_cast<int16_t>(responseCode) : 0,
        std::memory_order_release);
    if (responseCode != HTTP_CODE_OK)
    {
      http.end();
      error = responseCode < 0
          ? SnapshotError::ConnectionFailed
          : SnapshotError::HttpFailed;
      return false;
    }

    const int contentLength = http.getSize();
    if (contentLength > 0)
    {
      metrics.expectedBytes = static_cast<uint32_t>(contentLength);
    }
    if (contentLength > static_cast<int>(MAX_JPEG_SIZE))
    {
      http.end();
      error = SnapshotError::SnapshotTooLarge;
      return false;
    }
    // Check the deadline even while data is arriving; read only available bytes.
    WiFiClient* stream = http.getStreamPtr();
    const uint32_t downloadStartedAt = millis();
    const uint32_t downloadTimeout = adaptive_.downloadTimeoutMs();
    size_t received = 0;
    bool overflow = false;
    bool timedOut = false;
    while (contentLength < 0 || received < static_cast<size_t>(contentLength))
    {
      if (millis() - downloadStartedAt >= downloadTimeout)
      {
        timedOut = true;
        break;
      }
      const int available = stream->available();
      if (available > 0)
      {
        const size_t room = MAX_JPEG_SIZE - received;
        if (room == 0)
        {
          overflow = true;
          break;
        }
        size_t chunk = std::min(static_cast<size_t>(available), room);
        if (contentLength >= 0)
        {
          chunk = std::min(chunk, static_cast<size_t>(contentLength) - received);
        }
        const int count = stream->read(target + received, chunk);
        if (count > 0)
        {
          received += static_cast<size_t>(count);
        }
        else
        {
          delay(5);
        }
        continue;
      }
      if (!stream->connected())
      {
        break;
      }
      delay(5);
    }
    http.end();

    metrics.received = received;
    metrics.downloadMs = millis() - downloadStartedAt;
    metrics.totalMs = millis() - startedAt;
    imageSize = received;
    if (overflow)
    {
      error = SnapshotError::SnapshotTooLarge;
      return false;
    }
    if (timedOut || imageSize == 0 ||
        (contentLength >= 0 && imageSize != static_cast<size_t>(contentLength)))
    {
      error = SnapshotError::SnapshotTimeout;
      return false;
    }
    if (imageSize < 4 ||
        target[0] != 0xFF || target[1] != 0xD8 ||
        target[imageSize - 2] != 0xFF || target[imageSize - 1] != 0xD9)
    {
      error = SnapshotError::InvalidJpeg;
      return false;
    }
    error = SnapshotError::None;
    return true;
  }

  void clearSnapshot()
  {
    if (snapshotMutex_ == nullptr ||
        xSemaphoreTake(snapshotMutex_, portMAX_DELAY) != pdTRUE)
    {
      return;
    }
    activeBuffer_ = -1;
    buffers_[0].size = 0;
    buffers_[1].size = 0;
    imageDimensions_.store(0, std::memory_order_release);
    imageAvailable_.store(false, std::memory_order_release);
    refresh_.clear();
    lastFetchAvailable_.store(false, std::memory_order_release);
    version_.fetch_add(1, std::memory_order_acq_rel);
    lastSuccessMs_.store(0, std::memory_order_release);
    xSemaphoreGive(snapshotMutex_);
  }

  void releaseReader(uint8_t index)
  {
    if (snapshotMutex_ == nullptr ||
        xSemaphoreTake(snapshotMutex_, portMAX_DELAY) != pdTRUE)
    {
      return;
    }
    if (index < 2 && buffers_[index].readers > 0)
    {
      --buffers_[index].readers;
    }
    xSemaphoreGive(snapshotMutex_);
  }

  void fail(SnapshotError error)
  {
    lastError_.store(error, std::memory_order_release);
    state_.store(State::Error, std::memory_order_release);
    Serial.printf("Camera snapshot failed: %s (HTTP %d, uptime %lu s)\n",
                  errorName(error),
                  static_cast<int>(lastHttpStatus_.load(std::memory_order_acquire)),
                  static_cast<unsigned long>(millis() / 1000));
  }

  void notifyTask()
  {
    if (taskHandle_ != nullptr)
    {
      xTaskNotifyGive(taskHandle_);
    }
  }

  static bool isValidSettings(const CameraSettings& settings)
  {
    IPAddress address;
    return settings.host[sizeof(settings.host) - 1] == '\0' &&
           settings.username[sizeof(settings.username) - 1] == '\0' &&
           settings.password[sizeof(settings.password) - 1] == '\0' &&
           address.fromString(settings.host) &&
           address != IPAddress(0, 0, 0, 0) &&
           settings.port != 0 &&
           settings.username[0] != '\0' &&
           settings.password[0] != '\0';
  }

  static bool isValidPort(const String& value)
  {
    if (value.isEmpty() || value.length() > 5)
    {
      return false;
    }
    for (size_t index = 0; index < value.length(); ++index)
    {
      if (value[index] < '0' || value[index] > '9')
      {
        return false;
      }
    }
    const unsigned long port = strtoul(value.c_str(), nullptr, 10);
    return port > 0 && port <= 65535;
  }

  static bool isValidUsername(const String& username)
  {
    for (size_t index = 0; index < username.length(); ++index)
    {
      const char ch = username[index];
      if (!((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '.' || ch == '_' || ch == '-' || ch == '@'))
      {
        return false;
      }
    }
    return true;
  }

  static bool urlEncode(const char* input, char* output, size_t outputSize)
  {
    static const char hexDigits[] = "0123456789ABCDEF";
    size_t outputLength = 0;
    for (const uint8_t* current =
             reinterpret_cast<const uint8_t*>(input);
         *current != '\0';
         ++current)
    {
      const uint8_t ch = *current;
      const bool unreserved =
          (ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z') ||
          (ch >= '0' && ch <= '9') ||
          ch == '-' || ch == '_' || ch == '.' || ch == '~';
      const size_t needed = unreserved ? 1 : 3;
      if (outputLength + needed >= outputSize)
      {
        return false;
      }
      if (unreserved)
      {
        output[outputLength++] = static_cast<char>(ch);
      }
      else
      {
        output[outputLength++] = '%';
        output[outputLength++] = hexDigits[ch >> 4];
        output[outputLength++] = hexDigits[ch & 0x0F];
      }
    }
    output[outputLength] = '\0';
    return true;
  }

  static const char* stateName(State state, bool hasImage)
  {
    if (state == State::Error && hasImage)
    {
      return "stale";
    }
    switch (state)
    {
      case State::StorageError: return "storage_error";
      case State::NotConfigured: return "not_configured";
      case State::Waiting: return "waiting_for_network";
      case State::Connecting: return "updating";
      case State::Available: return "available";
      case State::Error: return "error";
    }
    return "error";
  }

  static const char* errorName(SnapshotError error)
  {
    switch (error)
    {
      case SnapshotError::None: return "none";
      case SnapshotError::NoPsram: return "psram_unavailable";
      case SnapshotError::MemoryAllocation: return "snapshot_memory_unavailable";
      case SnapshotError::BufferBusy: return "snapshot_busy";
      case SnapshotError::UrlTooLong: return "invalid_credentials";
      case SnapshotError::ConnectionFailed: return "camera_unreachable";
      case SnapshotError::HttpFailed: return "camera_request_failed";
      case SnapshotError::SnapshotTooLarge: return "snapshot_too_large";
      case SnapshotError::SnapshotTimeout: return "snapshot_timeout";
      case SnapshotError::InvalidJpeg: return "invalid_snapshot";
    }
    return "camera_request_failed";
  }

  static bool isJpegStartOfFrameMarker(uint8_t marker)
  {
    return (marker >= 0xC0 && marker <= 0xC3) ||
           (marker >= 0xC5 && marker <= 0xC7) ||
           (marker >= 0xC9 && marker <= 0xCB) ||
           (marker >= 0xCD && marker <= 0xCF);
  }

  static bool readJpegDimensions(const uint8_t* jpeg,
                                 size_t imageSize,
                                 uint16_t& width,
                                 uint16_t& height)
  {
    width = 0;
    height = 0;
    if (jpeg == nullptr || imageSize < 4 ||
        jpeg[0] != 0xFF || jpeg[1] != 0xD8)
    {
      return false;
    }

    size_t offset = 2;
    while (offset < imageSize)
    {
      if (jpeg[offset] != 0xFF)
      {
        return false;
      }
      while (offset < imageSize && jpeg[offset] == 0xFF)
      {
        ++offset;
      }
      if (offset >= imageSize)
      {
        return false;
      }

      const uint8_t marker = jpeg[offset++];
      if (marker == 0xD9 || marker == 0xDA)
      {
        return false;
      }
      if (marker == 0xD8 || marker == 0x01 ||
          (marker >= 0xD0 && marker <= 0xD7))
      {
        continue;
      }
      if (imageSize - offset < 2)
      {
        return false;
      }

      const size_t segmentLength =
          (static_cast<size_t>(jpeg[offset]) << 8) | jpeg[offset + 1];
      if (segmentLength < 2 || segmentLength > imageSize - offset)
      {
        return false;
      }
      if (isJpegStartOfFrameMarker(marker))
      {
        if (segmentLength < 7)
        {
          return false;
        }
        height = static_cast<uint16_t>(
            (static_cast<uint16_t>(jpeg[offset + 3]) << 8) |
            jpeg[offset + 4]);
        width = static_cast<uint16_t>(
            (static_cast<uint16_t>(jpeg[offset + 5]) << 8) |
            jpeg[offset + 6]);
        return width != 0 && height != 0;
      }
      offset += segmentLength;
    }
    return false;
  }
};
