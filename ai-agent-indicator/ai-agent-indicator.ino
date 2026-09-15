#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* DEVICE_HOSTNAME = "AI-Agent-Indicator";

constexpr uint8_t RED_LED_PIN = D1;
constexpr uint8_t YELLOW_LED_PIN = D2;
constexpr uint8_t GREEN_LED_PIN = D5;
constexpr uint32_t STATUS_TIMEOUT_MS = 120000UL;
constexpr uint32_t WIFI_BLINK_INTERVAL_MS = 500UL;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000UL;
constexpr size_t MAX_REQUEST_BYTES = 256;
constexpr size_t MAX_AGENT_BYTES = 32;

enum class AgentState : uint8_t { READY, WORKING, BLOCKED, PERMISSION };

ESP8266WebServer server(80);
AgentState currentState = AgentState::READY;
char currentAgent[MAX_AGENT_BYTES + 1] = {};
size_t currentAgentLength = 0;
uint32_t lastValidUpdateMs = 0;
uint32_t lastWifiAttemptMs = 0;
bool hasFreshStatus = false;
bool wasWifiConnected = false;

bool parseState(JsonString value, AgentState& parsed) {
  if (value == JsonString("ready")) parsed = AgentState::READY;
  else if (value == JsonString("working")) parsed = AgentState::WORKING;
  else if (value == JsonString("blocked")) parsed = AgentState::BLOCKED;
  else if (value == JsonString("permission")) parsed = AgentState::PERMISSION;
  else return false;
  return true;
}

const char* stateName(AgentState state) {
  switch (state) {
    case AgentState::WORKING: return "working";
    case AgentState::BLOCKED: return "blocked";
    case AgentState::PERMISSION: return "permission";
    default: return "ready";
  }
}

void setCurrentState(AgentState state, JsonString agent, uint32_t now) {
  currentState = state;
  currentAgentLength = agent.size();
  memcpy(currentAgent, agent.c_str(), currentAgentLength);
  currentAgent[currentAgentLength] = '\0';
  lastValidUpdateMs = now;
  hasFreshStatus = true;
}

void expireStatusIfNeeded(uint32_t now) {
  if (hasFreshStatus && static_cast<uint32_t>(now - lastValidUpdateMs) >= STATUS_TIMEOUT_MS) {
    currentState = AgentState::READY;
    currentAgent[0] = '\0';
    currentAgentLength = 0;
    hasFreshStatus = false;
  }
}

uint32_t expiresInSeconds(uint32_t now) {
  if (!hasFreshStatus) return 0;
  uint32_t elapsed = static_cast<uint32_t>(now - lastValidUpdateMs);
  if (elapsed >= STATUS_TIMEOUT_MS) return 0;
  return (STATUS_TIMEOUT_MS - elapsed + 999UL) / 1000UL;
}

void writeLeds(bool red, bool yellow, bool green) {
  digitalWrite(RED_LED_PIN, red ? HIGH : LOW);
  digitalWrite(YELLOW_LED_PIN, yellow ? HIGH : LOW);
  digitalWrite(GREEN_LED_PIN, green ? HIGH : LOW);
}

void renderLeds(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    bool on = ((now / WIFI_BLINK_INTERVAL_MS) % 2U) == 0U;
    writeLeds(on, on, on);
    return;
  }

  switch (currentState) {
    case AgentState::WORKING: writeLeds(true, false, false); break;
    case AgentState::BLOCKED:
    case AgentState::PERMISSION: writeLeds(false, true, false); break;
    default: writeLeds(false, false, true); break;
  }
}

void maintainWifi(uint32_t now) {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasWifiConnected) {
    Serial.print("Connected. Device URL: http://");
    Serial.println(WiFi.localIP());
  } else if (!connected &&
             static_cast<uint32_t>(now - lastWifiAttemptMs) >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiAttemptMs = now;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  wasWifiConnected = connected;
}

void sendJsonError(int code, const char* message) {
  StaticJsonDocument<96> doc;
  doc["error"] = message;
  String output;
  serializeJson(doc, output);
  server.send(code, "application/json", output);
}

void sendStatusResponse() {
  uint32_t now = millis();
  expireStatusIfNeeded(now);
  StaticJsonDocument<192> doc;
  doc["state"] = stateName(currentState);
  doc["agent"] = JsonString(currentAgent, currentAgentLength);
  doc["expires_in_seconds"] = expiresInSeconds(now);
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleStatusGet() {
  sendStatusResponse();
}

// ArduinoJson also accepts relaxed syntax and stops at the first object.
// Check the complete JSON grammar first, without allocating or decoding it.
class StrictJson {
 public:
  explicit StrictJson(const String& body)
      : cursor(body.c_str()), end(cursor + body.length()) {}

  bool isObject() {
    whitespace();
    if (peek() != '{' || !value(0)) return false;
    whitespace();
    return cursor == end;
  }

 private:
  const char* cursor;
  const char* end;

  char peek() const { return cursor < end ? *cursor : '\0'; }
  bool take(char c) {
    if (cursor == end || *cursor != c) return false;
    ++cursor;
    return true;
  }
  void whitespace() {
    while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n') ++cursor;
  }
  bool digits() {
    const char* start = cursor;
    while (peek() >= '0' && peek() <= '9') ++cursor;
    return cursor != start;
  }
  bool literal(const char* text) {
    while (*text) if (!take(*text++)) return false;
    return true;
  }
  bool string() {
    if (!take('"')) return false;
    while (cursor < end) {
      unsigned char c = *cursor++;
      if (c == '"') return true;
      if (c < 0x20) return false;
      if (c == '\\') {
        if (take('u')) {
          for (uint8_t i = 0; i < 4; ++i) {
            char h = peek();
            if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
                  (h >= 'A' && h <= 'F'))) return false;
            ++cursor;
          }
        } else {
          char escaped = peek();
          if (!escaped || !strchr("\"\\/bfnrt", escaped)) return false;
          ++cursor;
        }
      } else if (c >= 0x80) {
        uint8_t count;
        uint32_t codepoint;
        if (c >= 0xC2 && c <= 0xDF) { count = 1; codepoint = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { count = 2; codepoint = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { count = 3; codepoint = c & 0x07; }
        else return false;
        uint32_t minimum = count == 1 ? 0x80 : (count == 2 ? 0x800 : 0x10000);
        for (uint8_t i = 0; i < count; ++i) {
          if (cursor == end || (static_cast<unsigned char>(*cursor) & 0xC0) != 0x80) return false;
          codepoint = (codepoint << 6) | (static_cast<unsigned char>(*cursor++) & 0x3F);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return false;
      }
    }
    return false;
  }
  bool value(uint8_t depth) {
    whitespace();
    char c = peek();
    if (c == '"') return string();
    if (c == '{' || c == '[') {
      // Match ArduinoJson's default limit and bound recursive stack usage.
      if (depth >= 10) return false;
      ++cursor;
      char close = c == '{' ? '}' : ']';
      whitespace();
      if (take(close)) return true;
      do {
        whitespace();
        if (c == '{') {
          if (!string()) return false;
          whitespace();
          if (!take(':')) return false;
        }
        if (!value(depth + 1)) return false;
        whitespace();
        if (take(close)) return true;
      } while (take(','));
      return false;
    }
    if (c == 't') return literal("true");
    if (c == 'f') return literal("false");
    if (c == 'n') return literal("null");
    take('-');
    if (!take('0') && !digits()) return false;
    if (take('.') && !digits()) return false;
    if (take('e') || take('E')) {
      if (!take('+')) take('-');
      if (!digits()) return false;
    }
    return true;
  }
};

void handleStatusPost() {
  String body = server.arg("plain");
  if (body.length() > MAX_REQUEST_BYTES) {
    sendJsonError(400, "body exceeds 256 bytes");
    return;
  }

  StaticJsonDocument<256> doc;
  if (!StrictJson(body).isObject() || deserializeJson(doc, body)) {
    sendJsonError(400, "malformed JSON");
    return;
  }
  if (!doc["state"].is<const char*>()) {
    sendJsonError(400, "state must be a string");
    return;
  }

  AgentState parsed;
  if (!parseState(doc["state"].as<JsonString>(), parsed)) {
    sendJsonError(400, "unsupported state");
    return;
  }

  JsonString agent("");
  if (doc.as<JsonObjectConst>().containsKey("agent")) {
    if (!doc["agent"].is<const char*>()) {
      sendJsonError(400, "agent must be a string");
      return;
    }
    agent = doc["agent"].as<JsonString>();
    if (agent.size() > MAX_AGENT_BYTES) {
      sendJsonError(400, "agent exceeds 32 bytes");
      return;
    }
  }

  setCurrentState(parsed, agent, millis());
  sendStatusResponse();
}

void setupRoutes() {
  server.on("/api/status", HTTP_GET, handleStatusGet);
  server.on("/api/status", HTTP_POST, handleStatusPost);
  server.onNotFound([]() {
    if (server.uri() == "/api/status") sendJsonError(405, "method not allowed");
    else sendJsonError(404, "not found");
  });
}

void setup() {
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  writeLeds(false, false, false);

  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (!WiFi.hostname(DEVICE_HOSTNAME)) {
    Serial.println("Failed to set DHCP hostname");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();

  setupRoutes();
  server.begin();
}

void loop() {
  uint32_t now = millis();
  maintainWifi(now);
  expireStatusIfNeeded(now);
  server.handleClient();
  renderLeds(now);
  yield();
}
