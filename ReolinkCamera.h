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

class ReolinkCamera
{
public:
  static constexpr uint32_t SNAPSHOT_INTERVAL_MS = 30000;
  static constexpr uint16_t SNAPSHOT_WIDTH = 640;
  static constexpr uint16_t SNAPSHOT_HEIGHT = 480;
  static constexpr size_t MAX_JPEG_SIZE = 2 * 1024 * 1024;

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
        new (std::nothrow) ImageLease(this, index));
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
    request->send(response);
  }

  void handleStatus(AsyncWebServerRequest* request) const
  {
    const bool available = imageAvailable_.load(std::memory_order_acquire);
    const State currentState = state_.load(std::memory_order_acquire);
    const SnapshotError currentError = lastError_.load(std::memory_order_acquire);
    const uint32_t lastSuccess = lastSuccessMs_.load(std::memory_order_acquire);
    const int16_t httpStatus = lastHttpStatus_.load(std::memory_order_acquire);
    const uint32_t ageSeconds = available
        ? (millis() - lastSuccess) / 1000
        : 0;

    char response[224];
    snprintf(response, sizeof(response),
             "{\"state\":\"%s\",\"error\":\"%s\",\"http_status\":%d,"
             "\"image_available\":%s,\"version\":%lu,"
             "\"age_seconds\":%lu,\"refresh_seconds\":%lu}",
             stateName(currentState, available),
             errorName(currentError),
             static_cast<int>(httpStatus),
             available ? "true" : "false",
             static_cast<unsigned long>(version_.load(std::memory_order_acquire)),
             static_cast<unsigned long>(ageSeconds),
             static_cast<unsigned long>(SNAPSHOT_INTERVAL_MS / 1000));
    request->send(200, "application/json; charset=utf-8", response);
  }

  void requestSnapshot(AsyncWebServerRequest* request)
  {
    if (!preferencesReady_ || taskHandle_ == nullptr || configMutex_ == nullptr)
    {
      request->send(503, "application/json",
                    "{\"error\":\"camera_service_unavailable\"}");
      return;
    }
    if (xSemaphoreTake(configMutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "application/json",
                    "{\"error\":\"camera_config_busy\"}");
      return;
    }
    const bool configured = configured_;
    xSemaphoreGive(configMutex_);
    if (!configured)
    {
      request->send(409, "application/json",
                    "{\"error\":\"camera_not_configured\"}");
      return;
    }

    if (state_.load(std::memory_order_acquire) != State::Connecting)
    {
      state_.store(State::Connecting, std::memory_order_release);
      notifyTask();
    }
    request->send(202, "application/json", "{\"accepted\":true}");
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
    switch (state_.load(std::memory_order_acquire))
    {
      case State::StorageError: return "Storage error";
      case State::NotConfigured: return "Not set";
      case State::Waiting: return "Network off";
      case State::Connecting: return available ? "Updating" : "Connecting";
      case State::Available: return "Available";
      case State::Error: return available ? "Stale image" : "Unavailable";
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

  class BoundedBufferWriter : public Stream
  {
  public:
    BoundedBufferWriter(uint8_t* destination, size_t capacity)
        : destination_(destination), capacity_(capacity) {}

    size_t write(uint8_t value) override
    {
      return write(&value, 1);
    }

    size_t write(const uint8_t* source, size_t length) override
    {
      if (length > capacity_ - size_)
      {
        overflow_ = true;
        return 0;
      }
      memcpy(destination_ + size_, source, length);
      size_ += length;
      return length;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    size_t size() const { return size_; }
    bool overflowed() const { return overflow_; }

  private:
    uint8_t* destination_;
    size_t capacity_;
    size_t size_ = 0;
    bool overflow_ = false;
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

  struct ImageLease
  {
    ImageLease(ReolinkCamera* owner, uint8_t bufferIndex)
        : camera(owner), index(bufferIndex) {}
    ~ImageLease()
    {
      camera->releaseReader(index);
    }
    ReolinkCamera* camera;
    uint8_t index;
  };

  Preferences preferences_;
  CameraSettings settings_{};
  SnapshotBuffer buffers_[2]{};
  SemaphoreHandle_t configMutex_ = nullptr;
  SemaphoreHandle_t snapshotMutex_ = nullptr;
  TaskHandle_t taskHandle_ = nullptr;
  int8_t activeBuffer_ = -1;
  bool preferencesReady_ = false;
  bool configured_ = false;
  std::atomic<State> state_{State::NotConfigured};
  std::atomic<SnapshotError> lastError_{SnapshotError::None};
  std::atomic<int16_t> lastHttpStatus_{0};
  std::atomic<bool> imageAvailable_{false};
  std::atomic<uint32_t> version_{0};
  std::atomic<uint32_t> lastSuccessMs_{0};

  static void taskEntry(void* context)
  {
    static_cast<ReolinkCamera*>(context)->cameraTask();
  }

  void cameraTask()
  {
    for (;;)
    {
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

      pollSnapshot(current);
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SNAPSHOT_INTERVAL_MS));
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

  void pollSnapshot(const CameraSettings& settings)
  {
    state_.store(State::Connecting, std::memory_order_release);
    uint8_t targetIndex = 0;
    uint8_t* target = reserveTarget(targetIndex);
    if (target == nullptr)
    {
      const SnapshotError reserveError =
          lastError_.load(std::memory_order_acquire);
      fail(reserveError == SnapshotError::None
               ? SnapshotError::BufferBusy
               : reserveError);
      return;
    }

    size_t imageSize = 0;
    SnapshotError error = SnapshotError::None;
    if (!fetchSnapshot(settings, target, imageSize, error))
    {
      fail(error);
      return;
    }

    if (xSemaphoreTake(snapshotMutex_, portMAX_DELAY) != pdTRUE)
    {
      fail(SnapshotError::BufferBusy);
      return;
    }
    if (!isCurrentSettings(settings))
    {
      xSemaphoreGive(snapshotMutex_);
      return;
    }
    buffers_[targetIndex].size = imageSize;
    activeBuffer_ = static_cast<int8_t>(targetIndex);
    lastSuccessMs_.store(millis(), std::memory_order_release);
    version_.fetch_add(1, std::memory_order_acq_rel);
    imageAvailable_.store(true, std::memory_order_release);
    xSemaphoreGive(snapshotMutex_);
    lastError_.store(SnapshotError::None, std::memory_order_release);
    state_.store(State::Available, std::memory_order_release);
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
                     SnapshotError& error)
  {
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
        "&user=%s&password=%s&width=%u&height=%u",
        settings.host,
        settings.port,
        static_cast<unsigned long>(requestNonce),
        encodedUsername,
        encodedPassword,
        SNAPSHOT_WIDTH,
        SNAPSHOT_HEIGHT);
    if (urlLength <= 0 || static_cast<size_t>(urlLength) >= sizeof(url))
    {
      error = SnapshotError::UrlTooLong;
      return false;
    }

    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.setReuse(false);
    http.useHTTP10(true);
    if (!http.begin(client, url))
    {
      error = SnapshotError::ConnectionFailed;
      return false;
    }
    http.addHeader("Accept", "image/jpeg");
    const int responseCode = http.GET();
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
    if (contentLength > static_cast<int>(MAX_JPEG_SIZE))
    {
      http.end();
      error = SnapshotError::SnapshotTooLarge;
      return false;
    }
    BoundedBufferWriter writer(target, MAX_JPEG_SIZE);
    const int copiedBytes = http.writeToStream(&writer);
    http.end();

    imageSize = writer.size();
    if (writer.overflowed())
    {
      error = SnapshotError::SnapshotTooLarge;
      return false;
    }
    if (copiedBytes < 0 || imageSize == 0 ||
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
    imageAvailable_.store(false, std::memory_order_release);
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
    Serial.printf("Camera snapshot failed: %s\n", errorName(error));
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
};
