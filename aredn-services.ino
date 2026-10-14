#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <atomic>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <sys/time.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <new>
#include "BoardPage.h"
#include "CameraPage.h"
#include "CameraSetupPage.h"
#include "FfatMessageBoard.h"
#include "ReolinkCamera.h"
#include "SetupPage.h"
#include "ServicePage.h"
#include "ToolsPage.h"
#include "WifiSetupPage.h"

namespace
{
constexpr char WIFI_AP_SSID[] = "AREDN-Server-Setup";
constexpr char WIFI_AP_PASSWORD[] = "arednsetup";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
constexpr uint32_t WIFI_RECOVERY_DELAY_MS = 5000;
// Time bounds reject uninitialized, implausible, or far-future clock values.
constexpr int64_t MIN_VALID_EPOCH = 1577836800LL;
constexpr int64_t CLIENT_TIME_BOOTSTRAP_EPOCH = 1700000000LL;
constexpr int64_t MAX_VALID_EPOCH = 4102444800LL;
constexpr uint8_t TIME_SOURCE_UNSET = 0;
constexpr uint8_t TIME_SOURCE_NETWORK = 1;
constexpr uint8_t TIME_SOURCE_MANUAL = 2;
constexpr uint8_t TIME_SOURCE_CLIENT = 3;
constexpr uint8_t CLIENT_TIME_AGREEMENT_COUNT = 3;
constexpr int64_t CLIENT_TIME_MAX_SAMPLE_SPREAD = 10;
constexpr uint32_t CLIENT_TIME_SAMPLE_MAX_AGE_MS = 3600UL * 1000UL;
constexpr char ADMIN_PASSWORD[] = "aredn-admin";
constexpr char ADMIN_COOKIE_PREFIX[] = "aredn_admin=";
constexpr size_t ADMIN_TOKEN_LENGTH = 32;
constexpr size_t ADMIN_SESSION_CAPACITY = 4;
constexpr uint32_t ADMIN_SESSION_LIFETIME_MS = 3600UL * 1000UL;
constexpr uint8_t ADMIN_LOGIN_MAX_FAILURES = 5;
constexpr uint32_t ADMIN_LOGIN_LOCKOUT_MS = 60000;
constexpr char FFAT_FORMAT_CONFIRMATION[] = "FORMAT FFAT";
constexpr uint32_t FFAT_FORMAT_CHALLENGE_LIFETIME_MS = 60000;
constexpr size_t BOARD_BOOT_ID_LENGTH = 16;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int8_t OLED_RESET = -1;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t OLED_PAGE_INTERVAL_MS = 5000;
constexpr uint8_t OLED_PAGE_COUNT = 4;

// These objects are shared by asynchronous HTTP callbacks and the main Arduino loop.
AsyncWebServer server(80);
Preferences wifiPreferences;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
FfatMessageBoard messageBoard;
ReolinkCamera reolinkCamera;
// Atomic fields are read or updated from both the server task and the main loop.
std::atomic<uint8_t> deviceTimeSource{TIME_SOURCE_UNSET};
// Both revision counters change together, so readers take them under one spinlock.
// fullRefreshRevision is the revision of the last edit, delete, import, or reset.
portMUX_TYPE boardRevisionLock = portMUX_INITIALIZER_UNLOCKED;
uint32_t boardRevision = 1;
uint32_t fullRefreshRevision = 0;
char deviceBootId[BOARD_BOOT_ID_LENGTH + 1]{};
std::atomic<bool> importUploadActive{false};
// The import is finished by a worker task, so these are written there and read by the status route.
enum class ImportJobState : uint8_t { Idle, Running, Succeeded, Failed };
std::atomic<ImportJobState> importJobState{ImportJobState::Idle};
std::atomic<uint32_t> importJobCount{0};
const char* importJobReason = "none";
enum class MessageMutationType : uint8_t { Delete, Update };
struct MessageMutationJob
{
  MessageMutationType type = MessageMutationType::Delete;
  uint32_t id = 0;
  FfatMessageBoard::Priority priority = FfatMessageBoard::Priority::Green;
  char name[FfatMessageBoard::MAX_NAME_LENGTH + 1]{};
  char text[FfatMessageBoard::MAX_MESSAGE_LENGTH + 1]{};
};
MessageMutationJob messageMutationJob;
std::atomic<bool> messageMutationActive{false};
std::atomic<ImportJobState> messageMutationState{ImportJobState::Idle};
std::atomic<bool> wifiCredentialsSaved{false};
std::atomic<bool> wifiConfigRequestPending{false};
std::atomic<uint8_t> wifiProvisionState{0};
std::atomic<bool> wifiScanActive{false};
// Client samples are protected because HTTP handlers may run concurrently.
SemaphoreHandle_t clientTimeMutex = nullptr;
// The admin session and login throttling state below are protected by adminMutex.
SemaphoreHandle_t adminMutex = nullptr;
struct AdminSession
{
  char token[ADMIN_TOKEN_LENGTH + 1]{};
  uint32_t startedAt = 0;
  bool active = false;
};
AdminSession adminSessions[ADMIN_SESSION_CAPACITY]{};
uint8_t adminLoginFailures = 0;
bool adminLoginLocked = false;
uint32_t adminLoginLockStart = 0;
char ffatFormatChallenge[ADMIN_TOKEN_LENGTH + 1]{};
bool ffatFormatChallengeActive = false;
uint32_t ffatFormatChallengeStart = 0;
SemaphoreHandle_t wifiConfigMutex = nullptr;
SemaphoreHandle_t wifiScanMutex = nullptr;
char requestedWifiSsid[33]{};
char requestedWifiPassword[65]{};
char activeWifiSsid[33]{};
char activeWifiPassword[65]{};
String savedWifiSsid;
String savedWifiPassword;
int64_t clientTimeSamples[CLIENT_TIME_AGREEMENT_COUNT]{};
uint32_t clientTimeSampleClients[CLIENT_TIME_AGREEMENT_COUNT]{};
uint32_t clientTimeSampleTicks[CLIENT_TIME_AGREEMENT_COUNT]{};
uint8_t clientTimeSampleCount = 0;
uint32_t lastReconnectAttempt = 0;
uint32_t wifiConnectAttemptStart = 0;
uint32_t wifiRecoveryStart = 0;
bool wifiConnectAttemptActive = false;
bool wifiSavingCandidate = false;
bool wifiCandidateSawDisconnect = false;
bool wifiRecoveryPending = false;
bool wifiPreferencesAvailable = false;
bool wasConnected = false;
uint32_t lastDisplayUpdate = 0;
uint32_t displayedBoardRevision = 0;
uint8_t displayedPage = 0;
bool displayAvailable = false;
bool latestMessageAvailable = false;
FfatMessageBoard::Message latestMessage{};

enum WifiProvisionState : uint8_t
{
  WIFI_SETUP_REQUIRED,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED
};

struct StoredWifiCredentials
{
  char ssid[33];
  char password[65];
};

struct ImportUploadState
{
  // The upload callback records failure until the final request handler sends its response.
  bool failed = false;
  bool started = false;
  bool finished = false;
  const char* error = nullptr;
};

void drawDisplayLine(uint8_t row, const char* label, const char* value)
{
  display.setCursor(0, row * 8);
  display.print(label);
  display.print(value);
}

void drawLargeIpLine(const char* value, int16_t y)
{
  uint16_t lineWidth = 0;
  for (const char* current = value; *current != '\0'; ++current)
  {
    lineWidth += *current == '.' ? 6 : 18;
  }

  int16_t cursorX = (static_cast<int16_t>(OLED_WIDTH) - lineWidth) / 2;
  for (const char* current = value; *current != '\0'; ++current)
  {
    if (*current == '.')
    {
      display.setTextSize(1);
      display.setCursor(cursorX, y + 14);
      display.write('.');
      cursorX += 6;
    }
    else
    {
      display.setTextSize(3);
      display.setCursor(cursorX, y);
      display.write(static_cast<uint8_t>(*current));
      cursorX += 18;
    }
  }
}

void drawCenteredDisplayText(const char* value, int16_t y)
{
  int16_t boundsX;
  int16_t boundsY;
  uint16_t textWidth;
  uint16_t textHeight;
  display.getTextBounds(value, 0, y, &boundsX, &boundsY, &textWidth, &textHeight);
  display.setCursor((OLED_WIDTH - textWidth) / 2, y);
  display.print(value);
}

uint8_t utf8SequenceLength(const uint8_t* value)
{
  // Returns the length of the complete UTF-8 sequence at value, or 1 for an invalid or
  // truncated one. Continuation bytes are checked one by one, so a NUL is never skipped.
  uint8_t length = 0;
  if (*value >= 0xC2 && *value <= 0xDF)
  {
    length = 2;
  }
  else if (*value >= 0xE0 && *value <= 0xEF)
  {
    length = 3;
  }
  else if (*value >= 0xF0 && *value <= 0xF4)
  {
    length = 4;
  }

  for (uint8_t index = 1; index < length; ++index)
  {
    if ((value[index] & 0xC0) != 0x80)
    {
      return 1;
    }
  }
  return length == 0 ? 1 : length;
}

size_t countUtf8Characters(const char* value)
{
  const uint8_t* current = reinterpret_cast<const uint8_t*>(value);
  size_t characterCount = 0;
  while (*current != '\0')
  {
    current += utf8SequenceLength(current);
    ++characterCount;
  }
  return characterCount;
}

void printDisplayUtf8Characters(const char* value, size_t maxCharacters)
{
  // The OLED font cannot render multibyte UTF-8; replace each complete code point with one '?'.
  const uint8_t* current = reinterpret_cast<const uint8_t*>(value);
  for (size_t printed = 0; printed < maxCharacters && *current != '\0'; ++printed)
  {
    if (*current < 0x80)
    {
      display.write(*current++);
      continue;
    }
    display.write('?');
    current += utf8SequenceLength(current);
  }
}

void printDisplayUtf8(const char* value)
{
  printDisplayUtf8Characters(value, SIZE_MAX);
}

void printDisplayUtf8Ellipsis(const char* value, uint8_t maxCharacters)
{
  // Count UTF-8 code points rather than bytes so truncation never splits a character.
  if (countUtf8Characters(value) <= maxCharacters)
  {
    printDisplayUtf8(value);
    return;
  }

  printDisplayUtf8Characters(value, maxCharacters > 3 ? maxCharacters - 3 : 0);
  display.print("...");
}

void formatSerialClock(char* output, size_t outputSize);
const char* messagePriorityName(uint8_t priority);

void refreshLatestDisplayMessage()
{
  // Cache the last post so display refreshes do not reread FFat on every page update.
  latestMessageAvailable = messageBoard.readLatest(latestMessage);
}

void drawBoardDisplay()
{
  if (!displayAvailable)
  {
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (!wifiCredentialsSaved.load(std::memory_order_acquire))
  {
    display.println("WiFi setup required");
    drawDisplayLine(1, "AP: ", WIFI_AP_SSID);
    drawDisplayLine(2, "Password: ", WIFI_AP_PASSWORD);
    display.setCursor(0, 32);
    display.println("Open in browser:");
    drawDisplayLine(5, "", WiFi.softAPIP().toString().c_str());
    display.println("/setup");
    display.println("Waiting for WiFi");
    display.display();
    return;
  }

  if (displayedPage == 0)
  {
    display.println("AREDN SERVICES");
    drawDisplayLine(1, "Router: ", WiFi.status() == WL_CONNECTED
        ? "Connected"
        : "Disconnected");
    char metrics[22];
    formatSerialClock(metrics, sizeof(metrics));
    drawDisplayLine(2, "Time: ", metrics);
    snprintf(metrics, sizeof(metrics), "%lu", static_cast<unsigned long>(messageBoard.messageCount()));
    drawDisplayLine(3, "Messages: ", metrics);
    snprintf(metrics, sizeof(metrics), "%d dBm",
             WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
    drawDisplayLine(4, "WiFi: ", metrics);
    snprintf(metrics, sizeof(metrics), "%lu KB",
             static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
    drawDisplayLine(5, "Heap: ", metrics);
    const uint32_t storageTotal = messageBoard.totalBytes();
    const uint32_t storagePercent = storageTotal == 0
      ? 0
      : static_cast<uint32_t>(static_cast<uint64_t>(messageBoard.usedBytes()) * 100 / storageTotal);
    snprintf(metrics, sizeof(metrics), "%lu%%",
             static_cast<unsigned long>(storagePercent));
    drawDisplayLine(6, "Storage: ", metrics);
    const uint32_t uptimeSeconds = static_cast<uint32_t>(esp_timer_get_time() / 1000000LL);
    snprintf(metrics, sizeof(metrics), "%lud %02luh %02lum",
             static_cast<unsigned long>(uptimeSeconds / 86400),
             static_cast<unsigned long>((uptimeSeconds / 3600) % 24),
             static_cast<unsigned long>((uptimeSeconds / 60) % 60));
    drawDisplayLine(7, "Uptime: ", metrics);
  }
  else if (displayedPage == 1)
  {
    display.println("IP ADDRESS");
    if (WiFi.status() == WL_CONNECTED)
    {
      const IPAddress address = WiFi.localIP();
      char ipAddress[16];
      snprintf(ipAddress, sizeof(ipAddress), "%u.%u.%u.%u",
               address[0], address[1], address[2], address[3]);

      char* secondSeparator = strchr(ipAddress, '.');
      secondSeparator = strchr(secondSeparator + 1, '.');
      const size_t firstLineLength = static_cast<size_t>(secondSeparator - ipAddress + 1);
      char firstLine[9];
      memcpy(firstLine, ipAddress, firstLineLength);
      firstLine[firstLineLength] = '\0';
      char secondLine[8];
      snprintf(secondLine, sizeof(secondLine), "%s", secondSeparator + 1);

      drawLargeIpLine(firstLine, 12);
      drawLargeIpLine(secondLine, 39);
    }
    else
    {
      display.setTextSize(2);
      drawCenteredDisplayText("NOT", 20);
      drawCenteredDisplayText("CONNECTED", 40);
    }
    display.setTextSize(1);
  }
  else if (displayedPage == 2)
  {
    // The second page shows the newest cached message; the author is truncated to fit the OLED.
    if (!latestMessageAvailable)
    {
      display.println("NO MESSAGES");
      display.setCursor(0, 24);
      display.println("Message board");
      display.setCursor(0, 32);
      display.println("is empty.");
    }
    else
    {
      display.print("LAST MESSAGE");
      if (latestMessage.priority != static_cast<uint8_t>(FfatMessageBoard::Priority::Green))
      {
        display.print(" [");
        display.print(messagePriorityName(latestMessage.priority));
        display.print("]");
      }
      display.println();
      display.setCursor(0, 16);
      printDisplayUtf8Ellipsis(latestMessage.name, 21);
      display.println();
      display.setCursor(0, 28);
      printDisplayUtf8(latestMessage.text);
      display.println();
    }
  }
  else
  {
    display.println("AREDN CAMERA");
    drawDisplayLine(1, "State: ", reolinkCamera.displayStatus());
    if (!reolinkCamera.imageAvailable())
    {
      drawDisplayLine(2, "Snapshot: ", "None yet");
    }
    else
    {
      char age[16];
      snprintf(age, sizeof(age), "%lus old",
               static_cast<unsigned long>(reolinkCamera.imageAgeSeconds()));
      drawDisplayLine(2, "Snapshot: ", age);
    }
    drawDisplayLine(3, "Size: ", "640 x 480");
    drawDisplayLine(4, "Refresh: ", "30 seconds");
    drawDisplayLine(5, "", "Bellevue Downtown");
    drawDisplayLine(6, "Overlay: ", "Browser only");
    drawDisplayLine(7, "Setup: ", "Use setup AP");
  }

  display.display();
}

void initializeDisplay()
{
  // Display failure is non-fatal: the network service can operate without the OLED.
  Wire.begin();
  Serial.println("I2C initialized");

  Serial.println("Scanning I2C bus...");
  Wire.beginTransmission(OLED_ADDRESS);
  const uint8_t error = Wire.endTransmission();

  if (error != 0)
  {
    displayAvailable = false;
    Serial.printf("No I2C device at address 0x%02X (error: %u)\n", OLED_ADDRESS, error);
    Serial.println("Service continues without display.");
    return;
  }

  Serial.printf("I2C device found at address 0x%02X\n", OLED_ADDRESS);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    displayAvailable = false;
    Serial.println("Display found via I2C but initialization failed.");
    Serial.println("Service continues without display.");
    return;
  }

  displayAvailable = true;
  Serial.println("Display successfully initialized");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("AREDN MESSAGE BOARD");
  display.println();
  display.println("STARTING SERVICE ...");
  display.display();
}

char foldSearchCharacter(char value)
{
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool containsSearchTerm(const char* value, const String& searchTerm)
{
  // Search is deliberately ASCII case-insensitive; stored UTF-8 bytes otherwise match exactly.
  if (searchTerm.isEmpty())
  {
    return true;
  }
  for (size_t start = 0; value[start] != '\0'; ++start)
  {
    size_t offset = 0;
    while (value[start + offset] != '\0' && offset < searchTerm.length() &&
           foldSearchCharacter(value[start + offset]) == foldSearchCharacter(searchTerm[offset]))
    {
      ++offset;
    }
    if (offset == searchTerm.length())
    {
      return true;
    }
  }
  return false;
}

void markBoardChanged(bool requiresFullRefresh)
{
  // Edits and deletions cannot be fetched incrementally, so they flag every older revision.
  portENTER_CRITICAL(&boardRevisionLock);
  ++boardRevision;
  if (requiresFullRefresh)
  {
    fullRefreshRevision = boardRevision;
  }
  portEXIT_CRITICAL(&boardRevisionLock);
}

void initializeDeviceBootId()
{
  const uint32_t high = esp_random();
  const uint32_t low = esp_random();
  snprintf(deviceBootId, sizeof(deviceBootId), "%08lx%08lx",
           static_cast<unsigned long>(high),
           static_cast<unsigned long>(low));
}

void readBoardRevisions(uint32_t& revision, uint32_t& fullRefresh)
{
  portENTER_CRITICAL(&boardRevisionLock);
  revision = boardRevision;
  fullRefresh = fullRefreshRevision;
  portEXIT_CRITICAL(&boardRevisionLock);
}

uint32_t currentBoardRevision()
{
  uint32_t revision;
  uint32_t fullRefresh;
  readBoardRevisions(revision, fullRefresh);
  return revision;
}

bool isDeviceTimeValid()
{
  // The platform clock's boot-time default is earlier than this accepted epoch.
  return static_cast<int64_t>(time(nullptr)) >= MIN_VALID_EPOCH;
}

const char* deviceTimeSourceName()
{
  // A stored source is meaningful only while the clock itself passes validation.
  if (!isDeviceTimeValid())
  {
    return "unset";
  }

  switch (deviceTimeSource.load(std::memory_order_acquire))
  {
    case TIME_SOURCE_MANUAL: return "manual";
    case TIME_SOURCE_CLIENT: return "client";
    default: return "unset";
  }
}

void resetClientTimeSamples()
{
  // Discard stale votes whenever a new clock value is accepted.
  clientTimeSampleCount = 0;
}

void pruneExpiredClientTimeSamples(uint32_t nowMs)
{
  uint8_t kept = 0;
  for (uint8_t index = 0; index < clientTimeSampleCount; ++index)
  {
    if (nowMs - clientTimeSampleTicks[index] > CLIENT_TIME_SAMPLE_MAX_AGE_MS)
    {
      continue;
    }
    clientTimeSamples[kept] = clientTimeSamples[index];
    clientTimeSampleClients[kept] = clientTimeSampleClients[index];
    clientTimeSampleTicks[kept] = clientTimeSampleTicks[index];
    ++kept;
  }
  clientTimeSampleCount = kept;
}

int64_t clientTimeSampleEpochNow(uint8_t index, uint32_t nowMs)
{
  // A sample is a browser timestamp from the past; advance it by the time that has elapsed since
  // it arrived so samples collected minutes apart can be compared and applied as "now".
  const uint32_t elapsedMs = nowMs - clientTimeSampleTicks[index];
  return clientTimeSamples[index] + static_cast<int64_t>((elapsedMs + 500) / 1000);
}

void addClientTimeSample(int64_t epoch, uint32_t clientAddress, uint32_t nowMs)
{
  pruneExpiredClientTimeSamples(nowMs);

  // Keep one sample per client so a single client cannot satisfy the agreement threshold
  // repeatedly; a repeat submission replaces that client's older sample with the fresh one.
  for (uint8_t index = 0; index < clientTimeSampleCount; ++index)
  {
    if (clientTimeSampleClients[index] == clientAddress)
    {
      clientTimeSamples[index] = epoch;
      clientTimeSampleTicks[index] = nowMs;
      return;
    }
  }

  if (clientTimeSampleCount == CLIENT_TIME_AGREEMENT_COUNT)
  {
    for (uint8_t index = 1; index < CLIENT_TIME_AGREEMENT_COUNT; ++index)
    {
      clientTimeSamples[index - 1] = clientTimeSamples[index];
      clientTimeSampleClients[index - 1] = clientTimeSampleClients[index];
      clientTimeSampleTicks[index - 1] = clientTimeSampleTicks[index];
    }
    --clientTimeSampleCount;
  }
  clientTimeSamples[clientTimeSampleCount] = epoch;
  clientTimeSampleClients[clientTimeSampleCount] = clientAddress;
  clientTimeSampleTicks[clientTimeSampleCount++] = nowMs;
}

bool synchronizeFromClient(int64_t clientEpoch, uint32_t clientAddress)
{
  // Client time is accepted immediately only for an unset clock or a small correction.
  if (clientTimeMutex == nullptr || clientEpoch < CLIENT_TIME_BOOTSTRAP_EPOCH ||
      clientEpoch > MAX_VALID_EPOCH ||
      xSemaphoreTake(clientTimeMutex, portMAX_DELAY) != pdTRUE)
  {
    return false;
  }

  const bool manualTime = deviceTimeSource.load(std::memory_order_acquire) == TIME_SOURCE_MANUAL;
  const int64_t deviceEpoch = static_cast<int64_t>(time(nullptr));
  const struct timeval clientTime = {static_cast<time_t>(clientEpoch), 0};
  if (deviceEpoch < CLIENT_TIME_BOOTSTRAP_EPOCH ||
      deviceTimeSource.load(std::memory_order_acquire) == TIME_SOURCE_UNSET)
  {
    // With no trustworthy clock yet, the first plausible browser timestamp bootstraps it.
    if (settimeofday(&clientTime, nullptr) != 0)
    {
      xSemaphoreGive(clientTimeMutex);
      return false;
    }
    deviceTimeSource.store(TIME_SOURCE_CLIENT, std::memory_order_release);
    resetClientTimeSamples();
    xSemaphoreGive(clientTimeMutex);
    return true;
  }

  const int64_t difference = clientEpoch > deviceEpoch
      ? clientEpoch - deviceEpoch
      : deviceEpoch - clientEpoch;
  if (!manualTime && difference >= 1 && difference <= CLIENT_TIME_MAX_SAMPLE_SPREAD)
  {
    if (settimeofday(&clientTime, nullptr) != 0)
    {
      xSemaphoreGive(clientTimeMutex);
      return false;
    }
    deviceTimeSource.store(TIME_SOURCE_CLIENT, std::memory_order_release);
    resetClientTimeSamples();
    xSemaphoreGive(clientTimeMutex);
    return true;
  }

  if (manualTime || difference > CLIENT_TIME_MAX_SAMPLE_SPREAD)
  {
    // A large correction, or any correction after manual setup, needs distinct-client agreement.
    const uint32_t nowMs = millis();
    addClientTimeSample(clientEpoch, clientAddress, nowMs);
    if (clientTimeSampleCount == CLIENT_TIME_AGREEMENT_COUNT)
    {
      int64_t current[CLIENT_TIME_AGREEMENT_COUNT];
      for (uint8_t index = 0; index < CLIENT_TIME_AGREEMENT_COUNT; ++index)
      {
        current[index] = clientTimeSampleEpochNow(index, nowMs);
      }
      int64_t lowest = current[0];
      int64_t highest = current[0];
      for (uint8_t index = 1; index < CLIENT_TIME_AGREEMENT_COUNT; ++index)
      {
        lowest = min(lowest, current[index]);
        highest = max(highest, current[index]);
      }
      if (highest - lowest <= CLIENT_TIME_MAX_SAMPLE_SPREAD)
      {
        // The median limits the effect of a single outlying client timestamp.
        int64_t median = current[0];
        for (uint8_t index = 0; index < CLIENT_TIME_AGREEMENT_COUNT; ++index)
        {
          uint8_t valuesBelow = 0;
          for (uint8_t candidate = 0; candidate < CLIENT_TIME_AGREEMENT_COUNT; ++candidate)
          {
            valuesBelow += current[candidate] < current[index] ? 1 : 0;
          }
          if (valuesBelow == CLIENT_TIME_AGREEMENT_COUNT / 2)
          {
            median = current[index];
            break;
          }
        }
        const struct timeval consensusTime = {static_cast<time_t>(median), 0};
        if (settimeofday(&consensusTime, nullptr) != 0)
        {
          resetClientTimeSamples();
          xSemaphoreGive(clientTimeMutex);
          return false;
        }
        deviceTimeSource.store(TIME_SOURCE_CLIENT, std::memory_order_release);
        resetClientTimeSamples();
      }
    }
  }
  xSemaphoreGive(clientTimeMutex);
  return true;
}

void appendJsonString(Print& output, const char* value)
{
  // Stream escaped JSON directly to the response instead of allocating a second copy.
  output.write('"');
  for (const unsigned char* character = reinterpret_cast<const unsigned char*>(value);
       *character != '\0';
       ++character)
  {
    switch (*character)
    {
      case '"': output.print("\\\""); break;
      case '\\': output.print("\\\\"); break;
      case '\b': output.print("\\b"); break;
      case '\f': output.print("\\f"); break;
      case '\n': output.print("\\n"); break;
      case '\r': output.print("\\r"); break;
      case '\t': output.print("\\t"); break;
      default:
        if (*character < 0x20)
        {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04x", *character);
          output.print(escaped);
        }
        else
        {
          output.write(*character);
        }
        break;
    }
  }
  output.write('"');
}

void appendIsoTimestamp(Print& output, int64_t epoch)
{
  // Exports use UTC ISO-8601 timestamps, independent of the device's local timezone.
  const time_t timestamp = static_cast<time_t>(epoch);
  struct tm utcTime{};
  char formatted[21]{};
  if (gmtime_r(&timestamp, &utcTime) == nullptr ||
      strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%SZ", &utcTime) == 0)
  {
    appendJsonString(output, "");
    return;
  }

  appendJsonString(output, formatted);
}

const char* messagePriorityName(uint8_t priority)
{
  // Green is also the compatibility fallback for older records without a priority field.
  switch (static_cast<FfatMessageBoard::Priority>(priority))
  {
    case FfatMessageBoard::Priority::Orange: return "orange";
    case FfatMessageBoard::Priority::Red: return "red";
    default: return "green";
  }
}

bool parseMessagePriority(const String& value, FfatMessageBoard::Priority& priority)
{
  // Accept only the exact priority tokens shared by the browser and import/export formats.
  if (value == "green")
  {
    priority = FfatMessageBoard::Priority::Green;
    return true;
  }
  if (value == "orange")
  {
    priority = FfatMessageBoard::Priority::Orange;
    return true;
  }
  if (value == "red")
  {
    priority = FfatMessageBoard::Priority::Red;
    return true;
  }
  return false;
}

void formatSerialClock(char* output, size_t outputSize)
{
  if (!isDeviceTimeValid())
  {
    snprintf(output, outputSize, "unset");
    return;
  }

  const time_t timestamp = time(nullptr);
  struct tm utcTime{};
  if (gmtime_r(&timestamp, &utcTime) == nullptr)
  {
    snprintf(output, outputSize, "unset");
    return;
  }

  strftime(output, outputSize, "%H:%M:%S UTC", &utcTime);
}

bool parseUint32Parameter(const String& value, uint32_t& result)
{
  if (value.isEmpty())
  {
    return false;
  }

  uint32_t parsed = 0;
  for (size_t index = 0; index < value.length(); ++index)
  {
    const char character = value[index];
    if (character < '0' || character > '9')
    {
      return false;
    }
    const uint32_t digit = static_cast<uint32_t>(character - '0');
    if (parsed > (UINT32_MAX - digit) / 10)
    {
      return false;
    }
    parsed = parsed * 10 + digit;
  }

  result = parsed;
  return true;
}

bool constantTimeEquals(const char* candidate, const char* expected)
{
  // Compare every expected byte so the response time does not reveal the matching prefix length.
  const size_t candidateLength = strlen(candidate);
  const size_t expectedLength = strlen(expected);
  uint8_t difference = candidateLength != expectedLength ? 1 : 0;
  for (size_t index = 0; index < expectedLength; ++index)
  {
    const char candidateChar = index < candidateLength ? candidate[index] : '\0';
    difference |= static_cast<uint8_t>(candidateChar ^ expected[index]);
  }
  return difference == 0;
}

bool isAdminLoginLocked()
{
  if (adminMutex == nullptr || xSemaphoreTake(adminMutex, portMAX_DELAY) != pdTRUE)
  {
    return true;
  }
  if (adminLoginLocked && millis() - adminLoginLockStart >= ADMIN_LOGIN_LOCKOUT_MS)
  {
    adminLoginLocked = false;
    adminLoginFailures = 0;
  }
  const bool locked = adminLoginLocked;
  xSemaphoreGive(adminMutex);
  return locked;
}

void recordFailedAdminLogin()
{
  if (adminMutex == nullptr || xSemaphoreTake(adminMutex, portMAX_DELAY) != pdTRUE)
  {
    return;
  }
  if (++adminLoginFailures >= ADMIN_LOGIN_MAX_FAILURES)
  {
    adminLoginLocked = true;
    adminLoginLockStart = millis();
  }
  xSemaphoreGive(adminMutex);
}

bool startAdminSession(char* tokenOut, const char* existingToken)
{
  // 128 random bits, always formatted with a fixed width so cookie comparison is exact.
  char token[ADMIN_TOKEN_LENGTH + 1];
  for (size_t part = 0; part < ADMIN_TOKEN_LENGTH / 8; ++part)
  {
    snprintf(token + part * 8, 9, "%08lx", static_cast<unsigned long>(esp_random()));
  }

  if (adminMutex == nullptr || xSemaphoreTake(adminMutex, portMAX_DELAY) != pdTRUE)
  {
    return false;
  }
  const uint32_t now = millis();
  AdminSession* availableSession = nullptr;
  AdminSession* matchingSession = nullptr;
  for (AdminSession& session : adminSessions)
  {
    if (session.active && now - session.startedAt >= ADMIN_SESSION_LIFETIME_MS)
    {
      session.active = false;
      memset(session.token, 0, sizeof(session.token));
    }
    if (existingToken != nullptr && session.active &&
        constantTimeEquals(existingToken, session.token))
    {
      matchingSession = &session;
    }
    if (!session.active && availableSession == nullptr)
    {
      availableSession = &session;
    }
  }
  AdminSession* selectedSession = matchingSession != nullptr
      ? matchingSession
      : availableSession;
  if (selectedSession == nullptr)
  {
    xSemaphoreGive(adminMutex);
    return false;
  }
  memcpy(selectedSession->token, token, sizeof(selectedSession->token));
  selectedSession->startedAt = now;
  selectedSession->active = true;
  adminLoginFailures = 0;
  xSemaphoreGive(adminMutex);
  memcpy(tokenOut, token, sizeof(token));
  return true;
}

bool startFfatFormatChallenge(char* challengeOut)
{
  char challenge[ADMIN_TOKEN_LENGTH + 1];
  for (size_t part = 0; part < ADMIN_TOKEN_LENGTH / 8; ++part)
  {
    snprintf(challenge + part * 8, 9, "%08lx", static_cast<unsigned long>(esp_random()));
  }

  if (adminMutex == nullptr || xSemaphoreTake(adminMutex, portMAX_DELAY) != pdTRUE)
  {
    return false;
  }
  memcpy(ffatFormatChallenge, challenge, sizeof(ffatFormatChallenge));
  ffatFormatChallengeActive = true;
  ffatFormatChallengeStart = millis();
  xSemaphoreGive(adminMutex);
  memcpy(challengeOut, challenge, sizeof(challenge));
  return true;
}

bool consumeFfatFormatChallenge(const char* challenge, const char* confirmation)
{
  if (challenge == nullptr || confirmation == nullptr ||
      !constantTimeEquals(confirmation, FFAT_FORMAT_CONFIRMATION) ||
      adminMutex == nullptr || xSemaphoreTake(adminMutex, portMAX_DELAY) != pdTRUE)
  {
    return false;
  }

  const bool valid = ffatFormatChallengeActive &&
      millis() - ffatFormatChallengeStart < FFAT_FORMAT_CHALLENGE_LIFETIME_MS &&
      constantTimeEquals(challenge, ffatFormatChallenge);
  if (valid)
  {
    ffatFormatChallengeActive = false;
    memset(ffatFormatChallenge, 0, sizeof(ffatFormatChallenge));
  }
  xSemaphoreGive(adminMutex);
  return valid;
}

bool adminCookieToken(AsyncWebServerRequest* request, char* tokenOut)
{
  if (request == nullptr || tokenOut == nullptr || !request->hasHeader("Cookie"))
  {
    return false;
  }

  const String cookies = request->getHeader("Cookie")->value();
  const int length = static_cast<int>(cookies.length());
  int start = 0;
  while (start < length)
  {
    int end = cookies.indexOf(';', start);
    if (end < 0)
    {
      end = length;
    }
    while (start < end && cookies[start] == ' ')
    {
      ++start;
    }
    if (cookies.startsWith(ADMIN_COOKIE_PREFIX, start))
    {
      const int valueStart = start + strlen(ADMIN_COOKIE_PREFIX);
      if (end - valueStart != static_cast<int>(ADMIN_TOKEN_LENGTH))
      {
        return false;
      }
      cookies.substring(valueStart, end).toCharArray(tokenOut, ADMIN_TOKEN_LENGTH + 1);
      return true;
    }
    start = end + 1;
  }
  return false;
}

void endAdminSession(const char* candidate)
{
  if (candidate != nullptr && adminMutex != nullptr &&
      xSemaphoreTake(adminMutex, portMAX_DELAY) == pdTRUE)
  {
    for (AdminSession& session : adminSessions)
    {
      if (session.active && constantTimeEquals(candidate, session.token))
      {
        session.active = false;
        memset(session.token, 0, sizeof(session.token));
        break;
      }
    }
    xSemaphoreGive(adminMutex);
  }
}

bool adminTokenMatches(const char* candidate)
{
  if (candidate == nullptr || adminMutex == nullptr ||
      xSemaphoreTake(adminMutex, portMAX_DELAY) != pdTRUE)
  {
    return false;
  }
  const uint32_t now = millis();
  bool matches = false;
  for (AdminSession& session : adminSessions)
  {
    if (session.active && now - session.startedAt >= ADMIN_SESSION_LIFETIME_MS)
    {
      session.active = false;
      memset(session.token, 0, sizeof(session.token));
    }
    if (session.active && constantTimeEquals(candidate, session.token))
    {
      matches = true;
    }
  }
  xSemaphoreGive(adminMutex);
  return matches;
}

bool isAdminAuthenticated(AsyncWebServerRequest* request)
{
  char token[ADMIN_TOKEN_LENGTH + 1]{};
  return adminCookieToken(request, token) && adminTokenMatches(token);
}

bool isSetupApClient(AsyncWebServerRequest* request)
{
  if (request == nullptr || request->client() == nullptr)
  {
    return false;
  }

  // The connection must be addressed to the setup AP itself, so clients arriving through the
  // station network are rejected even if their subnet overlaps with the AP subnet.
  const IPAddress apAddress = WiFi.softAPIP();
  if (request->client()->localIP() != apAddress)
  {
    return false;
  }

  const IPAddress clientAddress = request->client()->remoteIP();
  return clientAddress[0] == apAddress[0] &&
         clientAddress[1] == apAddress[1] &&
         clientAddress[2] == apAddress[2];
}

void sendSetupApRequired(AsyncWebServerRequest* request)
{
  request->send(403, "application/json", "{\"error\":\"setup_ap_required\"}");
}

void sendAdminRequired(AsyncWebServerRequest* request)
{
  // Keep authorization failures consistent for every protected route.
  request->send(401, "application/json", "{\"error\":\"admin_required\"}");
}

ImportUploadState* createImportUploadState()
{
  // The request owns _tempObject and releases it with free(), so allocate it with calloc().
  void* memory = calloc(1, sizeof(ImportUploadState));
  return memory == nullptr ? nullptr : new (memory) ImportUploadState();
}

void releaseImportUploadState(AsyncWebServerRequest* request)
{
  free(request->_tempObject);
  request->_tempObject = nullptr;
}

void abortUnfinishedImport(const ImportUploadState* state)
{
  // Release the staged upload and the import slot if the request ends before its final chunk.
  if (state != nullptr && state->started && !state->finished)
  {
    messageBoard.abortImport();
    importUploadActive.store(false, std::memory_order_release);
  }
}

// Parsing a large import takes up to a minute. It must not run in the async_tcp task: that task
// has to keep draining AsyncTCP's fixed-size event queue, otherwise lwIP's tcpip thread blocks on
// the full queue and the next response write deadlocks against it until the task watchdog fires.
void importFinishTask(void*)
{
  const bool success = messageBoard.finishImport();
  importJobReason = messageBoard.importError();
  importJobCount.store(success ? messageBoard.messageCount() : 0, std::memory_order_relaxed);
  if (success)
  {
    markBoardChanged(true);
  }
  importJobState.store(success ? ImportJobState::Succeeded : ImportJobState::Failed,
                       std::memory_order_release);
  importUploadActive.store(false, std::memory_order_release);
  vTaskDelete(nullptr);
}

bool startImportFinish()
{
  importJobState.store(ImportJobState::Running, std::memory_order_release);
  // Core 1 keeps the worker away from the networking tasks and the watched idle task on core 0.
  if (xTaskCreatePinnedToCore(importFinishTask, "importFinish", 12288, nullptr, 1, nullptr, 1) == pdPASS)
  {
    return true;
  }
  importJobState.store(ImportJobState::Idle, std::memory_order_release);
  return false;
}

void messageMutationTask(void*)
{
  const MessageMutationJob job = messageMutationJob;
  const bool success = job.type == MessageMutationType::Delete
      ? messageBoard.remove(job.id)
      : messageBoard.update(job.id, job.name, job.text, job.priority);
  if (success)
  {
    markBoardChanged(true);
  }
  messageMutationState.store(success ? ImportJobState::Succeeded : ImportJobState::Failed,
                             std::memory_order_release);
  messageMutationActive.store(false, std::memory_order_release);
  vTaskDelete(nullptr);
}

bool startMessageMutation(const MessageMutationJob& job)
{
  bool expected = false;
  if (!messageMutationActive.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
  {
    return false;
  }
  messageMutationJob = job;
  messageMutationState.store(ImportJobState::Running, std::memory_order_release);
  if (xTaskCreatePinnedToCore(messageMutationTask, "messageMutation", 8192, nullptr, 1,
                              nullptr, 1) == pdPASS)
  {
    return true;
  }
  messageMutationState.store(ImportJobState::Idle, std::memory_order_release);
  messageMutationActive.store(false, std::memory_order_release);
  return false;
}

void writeMessageItems(AsyncResponseStream& response,
                       File& file,
                       uint32_t& count,
                       bool pretty,
                       bool newestFirst,
                       uint32_t limit = 0,
                       uint32_t beforeId = 0,
                       uint32_t afterId = 0,
                       bool* hasMore = nullptr,
                       const String* searchTerm = nullptr,
                       int priorityFilter = -1)
{
  // The ID-sorted index lets incremental polling skip records already seen by the client.
  if (hasMore != nullptr)
  {
    *hasMore = false;
  }
  const uint32_t storedCount = messageBoard.messageCount();
  if (storedCount == 0)
  {
    return;
  }

  const bool incrementalLatest = afterId != 0 && beforeId == 0 && searchTerm == nullptr;
  // Narrow incremental reads to the first newer ID; filtered and paged requests scan normally.
  const uint32_t firstIndexAfter = incrementalLatest
      ? messageBoard.firstIndexAfter(afterId)
      : 0;
  for (uint32_t position = 0; position < storedCount; ++position)
  {
    // Yield before filtering so a scan with few or no matches cannot starve the watchdog.
    if ((position & 0x1f) == 0)
    {
      esp_task_wdt_reset();
      delay(0);
    }
    const uint32_t index = newestFirst ? storedCount - 1 - position : position;
    if (incrementalLatest && index < firstIndexAfter)
    {
      break;
    }
    FfatMessageBoard::Message message{};
    const bool readable = messageBoard.readAtExclusive(file, index, message);
    if (!readable ||
        (beforeId != 0 && message.id >= beforeId) ||
        (afterId != 0 && message.id <= afterId) ||
        (priorityFilter >= 0 && message.priority != static_cast<uint8_t>(priorityFilter)) ||
        (searchTerm != nullptr &&
         !containsSearchTerm(message.name, *searchTerm) &&
         !containsSearchTerm(message.text, *searchTerm)))
    {
      continue;
    }
    if (limit != 0 && count >= limit)
    {
      if (hasMore != nullptr)
      {
        *hasMore = true;
      }
      break;
    }
    if (count > 0)
    {
      response.print(pretty ? ",\n" : ",");
    }
    if (pretty)
    {
      response.printf("    {\n      \"id\": %lu,\n      \"created_at\": ",
                      static_cast<unsigned long>(message.id));
      appendIsoTimestamp(response, message.createdAtEpoch);
      response.print(",\n      \"name\": ");
      appendJsonString(response, message.name);
      response.print(",\n      \"text\": ");
      appendJsonString(response, message.text);
      if (message.priority != static_cast<uint8_t>(FfatMessageBoard::Priority::Green))
      {
        response.print(",\n      \"priority\": ");
        appendJsonString(response, messagePriorityName(message.priority));
      }
      response.print("\n    }");
    }
    else
    {
      response.printf("{\"i\":%lu,\"t\":%lld,\"u\":",
                      static_cast<unsigned long>(message.id),
                      static_cast<long long>(message.createdAtEpoch));
      appendJsonString(response, message.name);
      response.print(",\"m\":");
      appendJsonString(response, message.text);
      response.print(",\"p\":");
      appendJsonString(response, messagePriorityName(message.priority));
      response.write('}');
    }
    ++count;
  }
}

void handleSubmitMessage(AsyncWebServerRequest* request)
{
  if (!request->hasParam("name", true) || !request->hasParam("text", true))
  {
    request->send(400, "application/json", "{\"error\":\"missing_fields\"}");
    return;
  }

  String author = request->getParam("name", true)->value();
  String text = request->getParam("text", true)->value();
  FfatMessageBoard::Priority priority = FfatMessageBoard::Priority::Green;
  if (request->hasParam("priority", true) &&
      !parseMessagePriority(request->getParam("priority", true)->value(), priority))
  {
    request->send(400, "application/json", "{\"error\":\"invalid_priority\"}");
    return;
  }
  author.trim();
  text.trim();

  if (author.isEmpty() || text.isEmpty())
  {
    request->send(400, "application/json", "{\"error\":\"empty_fields\"}");
    return;
  }

  if (author.length() > FfatMessageBoard::MAX_NAME_LENGTH ||
      text.length() > FfatMessageBoard::MAX_MESSAGE_LENGTH)
  {
    request->send(400, "application/json", "{\"error\":\"too_long\"}");
    return;
  }

  if (!isDeviceTimeValid())
  {
    request->send(409, "application/json", "{\"error\":\"clock_unset\"}");
    return;
  }

  uint32_t messageId = 0;
  // Persist only after validation and a usable timestamp are available.
  const FfatMessageBoard::SubmitResult result = messageBoard.enqueue(
      author.c_str(), text.c_str(), static_cast<int64_t>(time(nullptr)), priority, messageId);
  if (result == FfatMessageBoard::SubmitResult::Invalid)
  {
    request->send(400, "application/json", "{\"error\":\"invalid_message\"}");
    return;
  }
  if (result == FfatMessageBoard::SubmitResult::Full)
  {
    request->send(409, "application/json", "{\"error\":\"board_full\"}");
    return;
  }
  if (result != FfatMessageBoard::SubmitResult::Accepted)
  {
    request->send(500, "application/json", "{\"error\":\"board_unavailable\"}");
    return;
  }

  markBoardChanged(false);

  char responseBody[48];
  snprintf(responseBody, sizeof(responseBody), "{\"accepted\":true,\"id\":%lu}",
           static_cast<unsigned long>(messageId));
  request->send(202, "application/json", responseBody);
}

void registerRoutes()
{
  // Route handlers are asynchronous; long-running storage operations use the board's mutex.
  server.on("/setup/wifi", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }

    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html; charset=utf-8", WIFI_SETUP_PAGE);
    response->addHeader("Cache-Control", "no-cache, must-revalidate");
    request->send(response);
  });

  server.on("/setup/camera", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html; charset=utf-8", CAMERA_SETUP_PAGE);
    response->addHeader("Cache-Control", "no-cache, must-revalidate");
    request->send(response);
  });

  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    if (request->url() != "/setup")
    {
      request->send(404, "text/plain; charset=utf-8", "Not found");
      return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html; charset=utf-8", SETUP_PAGE);
    response->addHeader("Cache-Control", "no-cache, must-revalidate");
    request->send(response);
  });

  server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }

    const uint8_t state = wifiProvisionState.load(std::memory_order_acquire);
    const char* stateName = "setup_required";
    if (state == WIFI_CONNECTING)
    {
      stateName = "connecting";
    }
    else if (state == WIFI_CONNECTED)
    {
      stateName = "connected";
    }
    else if (state == WIFI_FAILED)
    {
      stateName = "failed";
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    const String apAddress = WiFi.softAPIP().toString();
    const String stationAddress = connected ? WiFi.localIP().toString() : "0.0.0.0";
    char response[192];
    snprintf(response, sizeof(response),
             "{\"state\":\"%s\",\"wifi_connected\":%s,"
             "\"ap_ip\":\"%s\",\"sta_ip\":\"%s\"}",
             stateName,
             connected ? "true" : "false",
             apAddress.c_str(),
             stationAddress.c_str());
    request->send(200, "application/json", response);
  });

  server.on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    if (wifiScanMutex == nullptr ||
        xSemaphoreTake(wifiScanMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "application/json", "{\"error\":\"wifi_scan_busy\"}");
      return;
    }

    if (wifiScanActive.load(std::memory_order_acquire) &&
        WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    {
      xSemaphoreGive(wifiScanMutex);
      request->send(202, "application/json", "{\"state\":\"scanning\"}");
      return;
    }

    WiFi.scanDelete();
    wifiScanActive.store(false, std::memory_order_release);
    const int16_t scanResult = WiFi.scanNetworks(true, true, false, 1000);
    if (scanResult == WIFI_SCAN_FAILED)
    {
      Serial.printf("Wi-Fi scan failed to start (result=%d, status=%d)\n",
                    scanResult, static_cast<int>(WiFi.status()));
      xSemaphoreGive(wifiScanMutex);
      request->send(500, "application/json", "{\"error\":\"wifi_scan_failed\"}");
      return;
    }

    wifiScanActive.store(true, std::memory_order_release);
    Serial.printf("Wi-Fi scan started (result=%d; -1 means still running)\n", scanResult);
    xSemaphoreGive(wifiScanMutex);
    request->send(202, "application/json", "{\"state\":\"scanning\"}");
  });

  server.on("/api/wifi/networks", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    if (wifiScanMutex == nullptr ||
        xSemaphoreTake(wifiScanMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "application/json", "{\"error\":\"wifi_scan_busy\"}");
      return;
    }
    if (!wifiScanActive.load(std::memory_order_acquire))
    {
      xSemaphoreGive(wifiScanMutex);
      request->send(200, "application/json",
                    "{\"state\":\"idle\",\"networks\":[]}");
      return;
    }

    const int16_t networkCount = WiFi.scanComplete();
    if (networkCount == WIFI_SCAN_RUNNING)
    {
      xSemaphoreGive(wifiScanMutex);
      request->send(202, "application/json", "{\"state\":\"scanning\"}");
      return;
    }
    if (networkCount == WIFI_SCAN_FAILED)
    {
      Serial.printf("Wi-Fi scan failed to complete (status=%d)\n", networkCount);
      wifiScanActive.store(false, std::memory_order_release);
      WiFi.scanDelete();
      xSemaphoreGive(wifiScanMutex);
      request->send(500, "application/json", "{\"error\":\"wifi_scan_failed\"}");
      return;
    }

    AsyncResponseStream* response =
        request->beginResponseStream("application/json; charset=utf-8");
    Serial.printf("Wi-Fi scan completed: %d networks found\n", networkCount);
    response->print("{\"state\":\"complete\",\"networks\":[");
    bool firstNetwork = true;
    for (int16_t index = 0; index < networkCount; ++index)
    {
      const String ssid = WiFi.SSID(index);
      if (ssid.isEmpty() || ssid == WIFI_AP_SSID)
      {
        continue;
      }
      if (!firstNetwork)
      {
        response->write(',');
      }
      firstNetwork = false;
      response->print("{\"ssid\":");
      appendJsonString(*response, ssid.c_str());
      response->printf(",\"rssi\":%d,\"channel\":%d,\"secure\":%s}",
                       WiFi.RSSI(index),
                       WiFi.channel(index),
                       WiFi.encryptionType(index) == WIFI_AUTH_OPEN ? "false" : "true");
    }
    response->print("]}");
    xSemaphoreGive(wifiScanMutex);
    request->send(response);
  });

  server.on("/api/wifi/config", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    if (!wifiPreferencesAvailable)
    {
      request->send(503, "application/json", "{\"error\":\"wifi_storage_unavailable\"}");
      return;
    }
    bool scanRunning = false;
    if (wifiScanMutex != nullptr)
    {
      if (xSemaphoreTake(wifiScanMutex, pdMS_TO_TICKS(100)) != pdTRUE)
      {
        request->send(503, "application/json", "{\"error\":\"wifi_scan_busy\"}");
        return;
      }
      scanRunning = wifiScanActive.load(std::memory_order_acquire) &&
                    WiFi.scanComplete() == WIFI_SCAN_RUNNING;
      if (wifiScanActive.load(std::memory_order_acquire) && !scanRunning)
      {
        WiFi.scanDelete();
        wifiScanActive.store(false, std::memory_order_release);
      }
      xSemaphoreGive(wifiScanMutex);
    }
    if (scanRunning)
    {
      request->send(409, "application/json", "{\"error\":\"wifi_scan_in_progress\"}");
      return;
    }
    if (!request->hasParam("ssid", true) || !request->hasParam("password", true))
    {
      request->send(400, "application/json", "{\"error\":\"missing_credentials\"}");
      return;
    }

    const String ssid = request->getParam("ssid", true)->value();
    const String password = request->getParam("password", true)->value();
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64)
    {
      request->send(400, "application/json", "{\"error\":\"invalid_credentials\"}");
      return;
    }
    if (wifiConfigMutex == nullptr ||
        xSemaphoreTake(wifiConfigMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      request->send(503, "application/json", "{\"error\":\"wifi_config_busy\"}");
      return;
    }

    ssid.toCharArray(requestedWifiSsid, sizeof(requestedWifiSsid));
    password.toCharArray(requestedWifiPassword, sizeof(requestedWifiPassword));
    xSemaphoreGive(wifiConfigMutex);
    wifiConfigRequestPending.store(true, std::memory_order_release);
    wifiProvisionState.store(WIFI_CONNECTING, std::memory_order_release);
    request->send(202, "application/json", "{\"accepted\":true}");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html; charset=utf-8", SERVICE_PAGE);
    response->addHeader("Cache-Control", "no-cache, must-revalidate");
    request->send(response);
  });

  server.on("/camera/image.jpg", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    reolinkCamera.handleImage(request);
  });

  server.on("/camera/api/status", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    reolinkCamera.handleStatus(request);
  });

  server.on("/api/camera/config", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    reolinkCamera.handleConfiguration(request);
  });

  server.on("/api/camera/config", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    reolinkCamera.saveConfiguration(request);
  });

  server.on("/api/camera/config", HTTP_DELETE, [](AsyncWebServerRequest* request)
  {
    if (!isSetupApClient(request))
    {
      sendSetupApRequired(request);
      return;
    }
    reolinkCamera.clearConfiguration(request);
  });

  server.on("/msg-board/api/messages/export.json", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    // The response is buffered in RAM, so the full export is limited to administrators.
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    File file;
    if (!messageBoard.openReaderExclusive(file))
    {
      request->send(500, "application/json", "{\"error\":\"board_unavailable\"}");
      return;
    }

    // Hold the exclusive reader lock while streaming so records cannot be rewritten mid-export.
    AsyncResponseStream* response = request->beginResponseStream("application/json; charset=utf-8");
    response->addHeader("Content-Disposition", "attachment; filename=aredn-messages.json");
    response->printf("{\n  \"format\": \"aredn-message-board\",\n  \"message_count\": %lu,\n  \"messages\": [\n",
                     static_cast<unsigned long>(messageBoard.messageCount()));
    uint32_t count = 0;
    writeMessageItems(*response, file, count, true, true);
    response->printf("\n  ]\n}\n");
    messageBoard.closeReaderExclusive(file);
    request->send(response);
  });

  server.on("/msg-board/api/admin/login", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    if (isAdminLoginLocked())
    {
      request->send(429, "application/json", "{\"error\":\"too_many_attempts\"}");
      return;
    }
    if (!request->hasParam("password", true) ||
        !constantTimeEquals(request->getParam("password", true)->value().c_str(), ADMIN_PASSWORD))
    {
      recordFailedAdminLogin();
      request->send(403, "application/json", "{\"error\":\"invalid_password\"}");
      return;
    }

    char token[ADMIN_TOKEN_LENGTH + 1];
    char existingToken[ADMIN_TOKEN_LENGTH + 1]{};
    const bool hasExistingToken = adminCookieToken(request, existingToken);
    if (!startAdminSession(token, hasExistingToken ? existingToken : nullptr))
    {
      request->send(503, "application/json", "{\"error\":\"admin_unavailable\"}");
      return;
    }
    AsyncWebServerResponse* response = request->beginResponse(
        200, "application/json", "{\"authenticated\":true}");
    response->addHeader("Set-Cookie", String(ADMIN_COOKIE_PREFIX) + token +
      "; Path=/; Max-Age=3600; HttpOnly; SameSite=Strict");
    request->send(response);
  });

  server.on("/msg-board/api/admin/logout", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    // Only the signed-in administrator may end the session; other clients cannot force a logout.
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    char token[ADMIN_TOKEN_LENGTH + 1]{};
    adminCookieToken(request, token);
    endAdminSession(token);
    AsyncWebServerResponse* response = request->beginResponse(
        200, "application/json", "{\"authenticated\":false}");
    response->addHeader("Set-Cookie", String(ADMIN_COOKIE_PREFIX) +
      "; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
    request->send(response);
  });

  server.on("/msg-board/api/admin/status", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    request->send(200, "application/json",
                  isAdminAuthenticated(request)
                      ? "{\"authenticated\":true}"
                      : "{\"authenticated\":false}");
  });

  server.on("/msg-board/api/admin/import", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    ImportUploadState* state = static_cast<ImportUploadState*>(request->_tempObject);
    if (!isAdminAuthenticated(request))
    {
      abortUnfinishedImport(state);
      releaseImportUploadState(request);
      sendAdminRequired(request);
      return;
    }
    const bool failed = state == nullptr || state->failed;
    const char* importError = state != nullptr && state->error != nullptr
        ? state->error : messageBoard.importError();
    releaseImportUploadState(request);
    if (failed)
    {
      String response = "{\"error\":\"import_failed\",\"reason\":\"";
      response += importError;
      response += "\"}";
      request->send(400, "application/json", response);
    }
    else
    {
      // The upload is complete; the worker is still installing it. The page polls the status route.
      request->send(202, "application/json", "{\"processing\":true}");
    }
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data,
                 size_t length, size_t index, size_t total)
  {
    // AsyncWebServer delivers uploads in chunks; keep state on the request until the final chunk.
    if (index == 0)
    {
      ImportUploadState* state = createImportUploadState();
      request->_tempObject = state;
      // A dropped connection never reaches the final chunk, so release the import slot here.
      request->onDisconnect([request]()
      {
        abortUnfinishedImport(static_cast<ImportUploadState*>(request->_tempObject));
      });
      const bool claimed = !importUploadActive.exchange(true, std::memory_order_acq_rel);
      const bool authenticated = isAdminAuthenticated(request);
      if (state == nullptr || !claimed || !authenticated ||
          !messageBoard.beginImport(total))
      {
        if (claimed)
        {
          importUploadActive.store(false, std::memory_order_release);
        }
        if (state != nullptr)
        {
          state->failed = true;
          if (!claimed)
          {
            state->error = "import_in_progress";
          }
          else if (!authenticated)
          {
            state->error = "admin_required";
          }
        }
      }
      else
      {
        state->started = true;
      }
    }
    ImportUploadState* state = static_cast<ImportUploadState*>(request->_tempObject);
    if (state != nullptr && !state->failed &&
        !messageBoard.appendImportData(data, length))
    {
      state->failed = true;
      state->error = "upload_write_failed";
    }
    if (index + length == total && state != nullptr && state->started)
    {
      if (!state->failed && !startImportFinish())
      {
        state->failed = true;
        state->error = "worker_unavailable";
      }
      if (state->failed)
      {
        messageBoard.abortImport();
        importUploadActive.store(false, std::memory_order_release);
      }
      // From here the worker owns the staged import and the import slot.
      state->finished = true;
    }
  });

  server.on("/msg-board/api/admin/import/status", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    switch (importJobState.load(std::memory_order_acquire))
    {
      case ImportJobState::Running:
        request->send(200, "application/json", "{\"state\":\"processing\"}");
        break;
      case ImportJobState::Succeeded:
      {
        char response[48];
        snprintf(response, sizeof(response), "{\"state\":\"done\",\"count\":%lu}",
                 static_cast<unsigned long>(importJobCount.load(std::memory_order_relaxed)));
        request->send(200, "application/json", response);
        break;
      }
      case ImportJobState::Failed:
      {
        String response = "{\"state\":\"failed\",\"reason\":\"";
        response += importJobReason;
        response += "\"}";
        request->send(200, "application/json", response);
        break;
      }
      default:
        request->send(200, "application/json", "{\"state\":\"idle\"}");
        break;
    }
  });

  server.on("/msg-board/api/admin/message/mutation/status", HTTP_GET,
            [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    switch (messageMutationState.load(std::memory_order_acquire))
    {
      case ImportJobState::Running:
        request->send(200, "application/json", "{\"state\":\"processing\"}");
        break;
      case ImportJobState::Succeeded:
        request->send(200, "application/json", "{\"state\":\"done\"}");
        break;
      case ImportJobState::Failed:
        request->send(200, "application/json", "{\"state\":\"failed\"}");
        break;
      default:
        request->send(200, "application/json", "{\"state\":\"idle\"}");
        break;
    }
  });

  server.on("/msg-board/api/admin/messages", HTTP_DELETE, [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    const bool reset = messageBoard.reset();
    if (reset)
    {
      markBoardChanged(true);
    }
    request->send(reset ? 200 : 500,
                  "application/json",
            reset ? "{\"reset\":true}" : "{\"error\":\"reset_failed\"}");
  });

  server.on("/msg-board/api/admin/message", HTTP_DELETE, [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    if (!request->hasParam("id"))
    {
      request->send(400, "application/json", "{\"error\":\"missing_id\"}");
      return;
    }
    const uint32_t id = request->getParam("id")->value().toInt();
    if (importUploadActive.load(std::memory_order_acquire) ||
        importJobState.load(std::memory_order_acquire) == ImportJobState::Running)
    {
      request->send(409, "application/json", "{\"error\":\"import_in_progress\"}");
      return;
    }
    MessageMutationJob job;
    job.type = MessageMutationType::Delete;
    job.id = id;
    if (!startMessageMutation(job))
    {
      request->send(503, "application/json", "{\"error\":\"message_worker_unavailable\"}");
      return;
    }
    request->send(202, "application/json", "{\"processing\":true}");
  });

  server.on("/msg-board/api/admin/message", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    if (!request->hasParam("id"))
    {
      request->send(400, "application/json", "{\"error\":\"missing_id\"}");
      return;
    }
    if (!request->hasParam("name", true) || !request->hasParam("text", true))
    {
      request->send(400, "application/json", "{\"error\":\"missing_fields\"}");
      return;
    }
    String name = request->getParam("name", true)->value();
    String text = request->getParam("text", true)->value();
    FfatMessageBoard::Priority priority = FfatMessageBoard::Priority::Green;
    if (!request->hasParam("priority", true))
    {
      // A missing priority must not silently reset the stored one to green.
      request->send(400, "application/json", "{\"error\":\"missing_priority\"}");
      return;
    }
    if (!parseMessagePriority(request->getParam("priority", true)->value(), priority))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_priority\"}");
      return;
    }
    name.trim();
    text.trim();
    const uint32_t id = request->getParam("id")->value().toInt();
    if (name.isEmpty() || text.isEmpty() ||
        name.length() > FfatMessageBoard::MAX_NAME_LENGTH ||
        text.length() > FfatMessageBoard::MAX_MESSAGE_LENGTH)
    {
      request->send(400, "application/json", "{\"error\":\"invalid_message\"}");
      return;
    }
    if (importUploadActive.load(std::memory_order_acquire) ||
        importJobState.load(std::memory_order_acquire) == ImportJobState::Running)
    {
      request->send(409, "application/json", "{\"error\":\"import_in_progress\"}");
      return;
    }
    MessageMutationJob job;
    job.type = MessageMutationType::Update;
    job.id = id;
    job.priority = priority;
    memcpy(job.name, name.c_str(), name.length() + 1);
    memcpy(job.text, text.c_str(), text.length() + 1);
    if (!startMessageMutation(job))
    {
      request->send(503, "application/json", "{\"error\":\"message_worker_unavailable\"}");
      return;
    }
    request->send(202, "application/json", "{\"processing\":true}");
  });

  server.on("/msg-board/api/admin/format/prepare", HTTP_POST,
            [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }

    char challenge[ADMIN_TOKEN_LENGTH + 1];
    if (!startFfatFormatChallenge(challenge))
    {
      request->send(503, "application/json", "{\"error\":\"format_unavailable\"}");
      return;
    }

    char response[96];
    snprintf(response, sizeof(response), "{\"challenge\":\"%s\"}", challenge);
    request->send(200, "application/json", response);
  });

  server.on("/msg-board/api/admin/format", HTTP_POST,
            [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    if (!request->hasParam("challenge", true) ||
        !request->hasParam("confirmation", true))
    {
      request->send(400, "application/json", "{\"error\":\"missing_confirmation\"}");
      return;
    }

    const String challenge = request->getParam("challenge", true)->value();
    const String confirmation = request->getParam("confirmation", true)->value();
    if (!consumeFfatFormatChallenge(challenge.c_str(), confirmation.c_str()))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_confirmation\"}");
      return;
    }

    if (!messageBoard.format())
    {
      request->send(500, "application/json", "{\"error\":\"format_failed\"}");
      return;
    }
    markBoardChanged(true);
    request->send(200, "application/json", "{\"formatted\":true,\"messages\":0}");
  });

  server.on("/msg-board/api/messages", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    // Validate every parameter first so no error path has to release the board lock.
    // Pages are always bounded: the full archive is only available through the admin export.
    uint32_t limit = 20;
    if (request->hasParam("limit"))
    {
      if (!parseUint32Parameter(request->getParam("limit")->value(), limit) || limit == 0)
      {
        request->send(400, "application/json", "{\"error\":\"invalid_limit\"}");
        return;
      }
      if (limit > 100)
      {
        limit = 100;
      }
    }
    uint32_t beforeId = 0;
    if (request->hasParam("before") &&
        !parseUint32Parameter(request->getParam("before")->value(), beforeId))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_before\"}");
      return;
    }
    uint32_t afterId = 0;
    if (request->hasParam("after") &&
        !parseUint32Parameter(request->getParam("after")->value(), afterId))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_after\"}");
      return;
    }
    String searchTerm;
    if (request->hasParam("search"))
    {
      searchTerm = request->getParam("search")->value();
      searchTerm.trim();
      if (searchTerm.length() > 64)
      {
        request->send(400, "application/json", "{\"error\":\"search_too_long\"}");
        return;
      }
    }
    int priorityFilter = -1;
    if (request->hasParam("priority"))
    {
      const String priorityValue = request->getParam("priority")->value();
      FfatMessageBoard::Priority parsedPriority;
      if (priorityValue != "all")
      {
        if (!parseMessagePriority(priorityValue, parsedPriority))
        {
          request->send(400, "application/json", "{\"error\":\"invalid_priority\"}");
          return;
        }
        priorityFilter = static_cast<int>(parsedPriority);
      }
    }

    uint32_t clientRevision = 0;
    if (request->hasParam("rev") &&
        !parseUint32Parameter(request->getParam("rev")->value(), clientRevision))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_revision\"}");
      return;
    }
    const bool hasClientRevision = request->hasParam("rev") && clientRevision != 0;
    const String clientBootId = request->hasParam("boot")
        ? request->getParam("boot")->value()
        : String();

    File file;
    if (!messageBoard.openReaderExclusive(file))
    {
      request->send(500, "application/json", "{\"error\":\"board_unavailable\"}");
      return;
    }

    const bool clockValid = isDeviceTimeValid();
    const time_t currentEpoch = clockValid ? time(nullptr) : 0;
    bool hasMore = false;
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    uint32_t currentRevision;
    uint32_t fullRefreshAt;
    readBoardRevisions(currentRevision, fullRefreshAt);
    // Clients use these compact fields to decide whether polling can stay incremental. A client
    // reporting its last seen revision needs a full refresh when an edit or delete happened
    // after it, or when the revision went backwards because the device restarted.
    const bool requiresFullRefresh = hasClientRevision
        ? clientBootId != deviceBootId ||
          clientRevision < fullRefreshAt ||
          clientRevision > currentRevision
        : fullRefreshAt == currentRevision;
    response->printf("{\"r\":%lu,\"b\":\"%s\",\"f\":%s,\"v\":%s,\"s\":\"%s\",\"e\":%lld,\"p\":[",
             static_cast<unsigned long>(currentRevision),
             deviceBootId,
             requiresFullRefresh ? "true" : "false",
                     clockValid ? "true" : "false",
                     deviceTimeSourceName(),
                     static_cast<long long>(currentEpoch));
    uint32_t count = 0;
    writeMessageItems(*response, file, count, false, true, limit, beforeId, afterId, &hasMore,
              searchTerm.isEmpty() ? nullptr : &searchTerm, priorityFilter);
    response->printf("],\"h\":%s,\"c\":%lu,\"u\":%lu,\"z\":%lu}",
             hasMore ? "true" : "false",
                     static_cast<unsigned long>(messageBoard.messageCount()),
                     static_cast<unsigned long>(messageBoard.usedBytes()),
                     static_cast<unsigned long>(messageBoard.totalBytes()));
    messageBoard.closeReaderExclusive(file);
    request->send(response);
  });

  server.on("/msg-board/api/time", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    char response[128];
    const bool valid = isDeviceTimeValid();
    snprintf(response, sizeof(response),
             "{\"valid\":%s,\"source\":\"%s\",\"epoch\":%lld}",
             valid ? "true" : "false",
             deviceTimeSourceName(),
             static_cast<long long>(valid ? time(nullptr) : 0));
    request->send(200, "application/json", response);
  });

  server.on("/msg-board/api/time", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    if (!isAdminAuthenticated(request))
    {
      sendAdminRequired(request);
      return;
    }
    if (!request->hasParam("epoch", true))
    {
      request->send(400, "application/json", "{\"error\":\"missing_epoch\"}");
      return;
    }

    const String epochValue = request->getParam("epoch", true)->value();
    char* parseEnd = nullptr;
    const long long parsedEpoch = strtoll(epochValue.c_str(), &parseEnd, 10);
    if (parseEnd == epochValue.c_str() || *parseEnd != '\0' ||
        parsedEpoch < MIN_VALID_EPOCH || parsedEpoch > MAX_VALID_EPOCH)
    {
      request->send(400, "application/json", "{\"error\":\"invalid_epoch\"}");
      return;
    }

    if (clientTimeMutex == nullptr ||
        xSemaphoreTake(clientTimeMutex, portMAX_DELAY) != pdTRUE)
    {
      request->send(503, "application/json", "{\"error\":\"clock_busy\"}");
      return;
    }

    const struct timeval manualTime = {static_cast<time_t>(parsedEpoch), 0};
    if (settimeofday(&manualTime, nullptr) != 0)
    {
      xSemaphoreGive(clientTimeMutex);
      request->send(500, "application/json", "{\"error\":\"clock_update_failed\"}");
      return;
    }

    resetClientTimeSamples();
    deviceTimeSource.store(TIME_SOURCE_MANUAL, std::memory_order_release);
    xSemaphoreGive(clientTimeMutex);
    request->send(200, "application/json", "{\"valid\":true,\"source\":\"manual\"}");
  });

  server.on("/msg-board/api/messages", HTTP_POST, handleSubmitMessage);

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    const bool connected = WiFi.status() == WL_CONNECTED;
    const String ipAddress = connected ? WiFi.localIP().toString() : "0.0.0.0";
    const int rssi = connected ? WiFi.RSSI() : 0;
    char response[480];

    snprintf(
        response,
        sizeof(response),
      "{\"service\":\"aredn-service\",\"status\":\"ok\","
        "\"board_ready\":%s,"
        "\"message_count\":%lu,"
        "\"storage_used_bytes\":%lu,\"storage_total_bytes\":%lu,"
        "\"storage_free_bytes\":%lu,\"record_size_bytes\":%lu,"
        "\"index_capacity\":%lu,\"index_bytes\":%lu,"
        "\"uptime_ms\":%lu,\"free_heap\":%lu,\"wifi_connected\":%s,"
        "\"ip\":\"%s\",\"rssi_dbm\":%d}",
      messageBoard.ready() ? "true" : "false",
      static_cast<unsigned long>(messageBoard.messageCount()),
      static_cast<unsigned long>(messageBoard.usedBytes()),
      static_cast<unsigned long>(messageBoard.totalBytes()),
      static_cast<unsigned long>(messageBoard.freeBytes()),
      static_cast<unsigned long>(messageBoard.recordSize()),
      static_cast<unsigned long>(messageBoard.indexCapacity()),
      static_cast<unsigned long>(messageBoard.indexBytes()),
        static_cast<unsigned long>(millis()),
        static_cast<unsigned long>(ESP.getFreeHeap()),
        connected ? "true" : "false",
        ipAddress.c_str(),
        rssi);
    request->send(200, "application/json", response);
  });

  server.on("/api/time/sync", HTTP_POST, [](AsyncWebServerRequest* request)
  {
    if (!request->hasParam("epoch", true))
    {
      request->send(400, "application/json", "{\"error\":\"missing_epoch\"}");
      return;
    }

    const String epochValue = request->getParam("epoch", true)->value();
    char* parseEnd = nullptr;
    const long long clientEpoch = strtoll(epochValue.c_str(), &parseEnd, 10);
    if (parseEnd == epochValue.c_str() || *parseEnd != '\0' ||
        clientEpoch < CLIENT_TIME_BOOTSTRAP_EPOCH || clientEpoch > MAX_VALID_EPOCH)
    {
      request->send(400, "application/json", "{\"error\":\"invalid_epoch\"}");
      return;
    }

    if (clientTimeMutex == nullptr)
    {
      request->send(503, "application/json", "{\"error\":\"clock_sync_unavailable\"}");
      return;
    }

    const uint32_t clientAddress = request->client() == nullptr
        ? 0
        : static_cast<uint32_t>(request->client()->remoteIP());
    if (!synchronizeFromClient(static_cast<int64_t>(clientEpoch), clientAddress))
    {
      request->send(500, "application/json", "{\"error\":\"clock_sync_failed\"}");
      return;
    }
    request->send(204);
  });

  server.on("/msg-board", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (request->url() != "/msg-board")
    {
      request->send(404, "text/plain; charset=utf-8", "Not found");
      return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html; charset=utf-8", BOARD_PAGE);
    response->addHeader("Cache-Control", "no-cache, must-revalidate");
    request->send(response);
  });

  server.on("/camera", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (request->url() != "/camera")
    {
      request->send(404, "text/plain; charset=utf-8", "Not found");
      return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html; charset=utf-8", CAMERA_PAGE);
    response->addHeader("Cache-Control", "no-cache, must-revalidate");
    request->send(response);
  });

  server.on("/tools", HTTP_GET, [](AsyncWebServerRequest* request)
  {
    if (request->url() != "/tools")
    {
      request->send(404, "text/plain; charset=utf-8", "Not found");
      return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html; charset=utf-8", TOOLS_PAGE);
    response->addHeader("Cache-Control", "no-cache, must-revalidate");
    request->send(response);
  });

  server.onNotFound([](AsyncWebServerRequest* request)
  {
    request->send(404, "text/plain; charset=utf-8", "Not found");
  });
}
}

const char* resetReasonName(esp_reset_reason_t reason)
{
  switch (reason)
  {
    case ESP_RST_POWERON:   return "Power-On";
    case ESP_RST_EXT:       return "EXT_PIN (Reset by external pin)";
    case ESP_RST_SW:        return "Software Reset";
    case ESP_RST_PANIC:     return "ERROR: PANIC";
    case ESP_RST_INT_WDT:   return "ERROR: Interrupt WDT";
    case ESP_RST_TASK_WDT:  return "ERROR: Task WDT";
    case ESP_RST_WDT:       return "ERROR: Other WDT";
    case ESP_RST_DEEPSLEEP: return "Deep Sleep";
    case ESP_RST_BROWNOUT:  return "ERROR: Brownout";
    case ESP_RST_SDIO:      return "SDIO (Reset over SDIO)";
    default:                return "UNKNOWN (Reset reason can not be determined)";
  }
}

void setup()
{
  // Bring up diagnostics and the optional display before initializing storage and networking.
  Serial.begin(115200);

  const unsigned long serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < 1500))
  {
    delay(10);
  }
  const unsigned long serialWaitMs = millis() - serialWaitStart;

  delay(200);
  Serial.print("[Serial connection ready. Connection time: ");
  Serial.print(serialWaitMs);
  Serial.println(" ms.]");

  Serial.println("\nAREDN service starting");

  const esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("ESP reset reason: %s (Code %d)\n",
                resetReasonName(resetReason), static_cast<int>(resetReason));
  Serial.flush();
  initializeDisplay();
  Serial.printf("Hardware: %s | cores: %d | PSRAM: %lu KB | flash: %lu MB | free heap: %lu KB\n",
                ESP.getChipModel(),
                ESP.getChipCores(),
                static_cast<unsigned long>(ESP.getPsramSize() / 1024),
                static_cast<unsigned long>(ESP.getFlashChipSize() / (1024 * 1024)),
                static_cast<unsigned long>(ESP.getFreeHeap() / 1024));

  if (!messageBoard.begin())
  {
    Serial.println("ERROR: Could not initialize the FFat message board");
  }
  clientTimeMutex = xSemaphoreCreateMutex();
  adminMutex = xSemaphoreCreateMutex();
  if (adminMutex == nullptr)
  {
    Serial.println("ERROR: Could not create the admin session mutex; administrator login is disabled");
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  const IPAddress setupIp(192, 168, 4, 1);
  const IPAddress setupGateway(192, 168, 4, 1);
  const IPAddress setupSubnet(255, 255, 255, 0);
  if (!WiFi.softAPConfig(setupIp, setupGateway, setupSubnet))
  {
    Serial.println("ERROR: Could not configure the Wi-Fi setup AP address");
  }
  if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD))
  {
    Serial.println("ERROR: Could not start the Wi-Fi setup AP");
  }
  else
  {
    Serial.printf("Wi-Fi setup AP started: SSID=\"%s\" | IP=%s\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str());
  }
  wifiConfigMutex = xSemaphoreCreateMutex();
  if (wifiConfigMutex == nullptr)
  {
    Serial.println("ERROR: Could not create the Wi-Fi configuration mutex");
  }
  wifiScanMutex = xSemaphoreCreateMutex();
  if (wifiScanMutex == nullptr)
  {
    Serial.println("ERROR: Could not create the Wi-Fi scan mutex");
  }

  wifiPreferencesAvailable = wifiPreferences.begin("aredn-wifi", false);
  if (!wifiPreferencesAvailable)
  {
    Serial.println("ERROR: Could not open Wi-Fi credential storage");
    wifiProvisionState.store(WIFI_SETUP_REQUIRED, std::memory_order_release);
  }
  else
  {
    StoredWifiCredentials storedCredentials{};
    const size_t storedSize = wifiPreferences.getBytesLength("credentials");
    if (storedSize == sizeof(storedCredentials) &&
        wifiPreferences.getBytes("credentials", &storedCredentials,
                                 sizeof(storedCredentials)) == sizeof(storedCredentials) &&
        storedCredentials.ssid[sizeof(storedCredentials.ssid) - 1] == '\0' &&
        storedCredentials.password[sizeof(storedCredentials.password) - 1] == '\0' &&
        storedCredentials.ssid[0] != '\0')
    {
      savedWifiSsid = storedCredentials.ssid;
      savedWifiPassword = storedCredentials.password;
      wifiCredentialsSaved.store(true, std::memory_order_release);
      wifiProvisionState.store(WIFI_CONNECTING, std::memory_order_release);
      WiFi.begin(savedWifiSsid.c_str(), savedWifiPassword.c_str());
      wifiConnectAttemptStart = millis();
      wifiConnectAttemptActive = true;
      lastReconnectAttempt = wifiConnectAttemptStart;
      Serial.printf("Connecting to saved Wi-Fi network \"%s\"\n", savedWifiSsid.c_str());
    }
    else
    {
      wifiProvisionState.store(WIFI_SETUP_REQUIRED, std::memory_order_release);
      Serial.println("No saved Wi-Fi credentials; waiting for setup through the access point");
    }
  }
  Serial.printf("Setup page: http://%s/setup\n", WiFi.softAPIP().toString().c_str());
  Serial.println("Client time synchronization enabled");
  Serial.printf("Wi-Fi connection started (status %d)\n", static_cast<int>(WiFi.status()));

  initializeDeviceBootId();
  reolinkCamera.begin();
  registerRoutes();
  DefaultHeaders::Instance().addHeader("Connection", "close");
  server.begin();
  Serial.println("Async HTTP server started on port 80");
}

void loop()
{
  const uint32_t now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;

  if (wifiConfigRequestPending.exchange(false, std::memory_order_acq_rel))
  {
    char candidateSsid[sizeof(requestedWifiSsid)]{};
    char candidatePassword[sizeof(requestedWifiPassword)]{};
    if (wifiConfigMutex != nullptr &&
        xSemaphoreTake(wifiConfigMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      memcpy(candidateSsid, requestedWifiSsid, sizeof(candidateSsid));
      memcpy(candidatePassword, requestedWifiPassword, sizeof(candidatePassword));
      xSemaphoreGive(wifiConfigMutex);

      memcpy(activeWifiSsid, candidateSsid, sizeof(activeWifiSsid));
      memcpy(activeWifiPassword, candidatePassword, sizeof(activeWifiPassword));
      WiFi.disconnect(false, false);
      // The status can lag behind the disconnect; without a confirmed drop, reconnecting to the
      // same network would be indistinguishable from the old connection still being up.
      const uint32_t disconnectWaitStart = millis();
      while (WiFi.status() == WL_CONNECTED && millis() - disconnectWaitStart < 1000)
      {
        delay(20);
      }
      wifiCandidateSawDisconnect = WiFi.status() != WL_CONNECTED;
      WiFi.begin(activeWifiSsid, activeWifiPassword);
      wifiConnectAttemptStart = now;
      wifiConnectAttemptActive = true;
      wifiSavingCandidate = true;
      wifiRecoveryPending = false;
      lastReconnectAttempt = now;
      wifiProvisionState.store(WIFI_CONNECTING, std::memory_order_release);
      Serial.printf("Testing Wi-Fi credentials for \"%s\"\n", candidateSsid);
    }
    else
    {
      wifiProvisionState.store(WIFI_FAILED, std::memory_order_release);
      Serial.println("ERROR: Could not read the pending Wi-Fi configuration");
    }
  }
  connected = WiFi.status() == WL_CONNECTED;
  if (wifiSavingCandidate && !connected)
  {
    wifiCandidateSawDisconnect = true;
  }
  const bool connectedForAttempt =
      connected && (!wifiSavingCandidate || wifiCandidateSawDisconnect);

  if (connectedForAttempt && wifiConnectAttemptActive)
  {
    if (wifiSavingCandidate)
    {
      StoredWifiCredentials candidateCredentials{};
      strncpy(candidateCredentials.ssid, activeWifiSsid,
              sizeof(candidateCredentials.ssid) - 1);
      strncpy(candidateCredentials.password, activeWifiPassword,
              sizeof(candidateCredentials.password) - 1);
      if (wifiPreferences.putBytes("credentials", &candidateCredentials,
                                  sizeof(candidateCredentials)) != sizeof(candidateCredentials))
      {
        wifiProvisionState.store(WIFI_FAILED, std::memory_order_release);
        Serial.println("ERROR: Could not save the Wi-Fi credentials");
        WiFi.disconnect(false, false);
        wifiSavingCandidate = false;
        wifiCandidateSawDisconnect = false;
        wifiConnectAttemptActive = false;
        wifiRecoveryPending = !savedWifiSsid.isEmpty();
        wifiRecoveryStart = now;
      }
      else
      {
        savedWifiSsid = candidateCredentials.ssid;
        savedWifiPassword = candidateCredentials.password;
        wifiCredentialsSaved.store(true, std::memory_order_release);
        wifiProvisionState.store(WIFI_CONNECTED, std::memory_order_release);
        wifiSavingCandidate = false;
        wifiCandidateSawDisconnect = false;
        wifiConnectAttemptActive = false;
        Serial.printf("Wi-Fi credentials saved; connected to \"%s\"\n",
                      savedWifiSsid.c_str());
      }
    }
    else
    {
      wifiProvisionState.store(WIFI_CONNECTED, std::memory_order_release);
      wifiConnectAttemptActive = false;
    }
  }
  else if (wifiConnectAttemptActive &&
           now - wifiConnectAttemptStart >= WIFI_CONNECT_TIMEOUT_MS)
  {
    wifiConnectAttemptActive = false;
    wifiProvisionState.store(WIFI_FAILED, std::memory_order_release);
    Serial.println("ERROR: Wi-Fi connection attempt timed out; setup AP remains available");
    if (wifiSavingCandidate)
    {
      wifiSavingCandidate = false;
      wifiCandidateSawDisconnect = false;
      wifiRecoveryPending = !savedWifiSsid.isEmpty();
      wifiRecoveryStart = now;
    }
    lastReconnectAttempt = now;
  }

  if (wifiRecoveryPending && now - wifiRecoveryStart >= WIFI_RECOVERY_DELAY_MS)
  {
    wifiRecoveryPending = false;
    WiFi.begin(savedWifiSsid.c_str(), savedWifiPassword.c_str());
    wifiConnectAttemptStart = now;
    wifiConnectAttemptActive = true;
    lastReconnectAttempt = now;
    wifiProvisionState.store(WIFI_CONNECTING, std::memory_order_release);
    Serial.printf("Retrying previously saved Wi-Fi network \"%s\"\n", savedWifiSsid.c_str());
  }

  if (displayAvailable)
  {
    // Refresh the cached latest post only when a write changes the board revision.
    const uint32_t currentRevision = currentBoardRevision();
    const bool boardChanged = currentRevision != displayedBoardRevision;
    if (boardChanged)
    {
      displayedBoardRevision = currentRevision;
      refreshLatestDisplayMessage();
    }
    if (boardChanged || now - lastDisplayUpdate >= OLED_PAGE_INTERVAL_MS)
    {
      // Show changed content immediately; otherwise rotate through the OLED service pages.
      if (!boardChanged && lastDisplayUpdate != 0)
      {
        displayedPage = static_cast<uint8_t>((displayedPage + 1) % OLED_PAGE_COUNT);
      }
      lastDisplayUpdate = now;
      drawBoardDisplay();
    }
  }

  if (connected && !wasConnected)
  {
    const String ipAddress = WiFi.localIP().toString();
    Serial.printf("Connected to AREDN WLAN; IP: %s\n", ipAddress.c_str());
    wasConnected = true;
  }
  else if (!connected && wasConnected)
  {
    Serial.println("AREDN WLAN disconnected");
    wasConnected = false;
  }

  // Retry saved credentials without interrupting the setup access point.
  if (!connected && !wifiConnectAttemptActive && !wifiRecoveryPending &&
      wifiCredentialsSaved.load(std::memory_order_acquire) &&
      now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS)
  {
    lastReconnectAttempt = now;
    wifiConnectAttemptStart = now;
    wifiConnectAttemptActive = true;
    wifiProvisionState.store(WIFI_CONNECTING, std::memory_order_release);
    Serial.printf("Retrying saved Wi-Fi connection (status %d)\n",
                  static_cast<int>(WiFi.status()));
    WiFi.begin(savedWifiSsid.c_str(), savedWifiPassword.c_str());
  }

  delay(10);
}