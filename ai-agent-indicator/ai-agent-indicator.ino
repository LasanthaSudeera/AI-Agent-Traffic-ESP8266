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
String currentAgent;
uint32_t lastValidUpdateMs = 0;
uint32_t lastWifiAttemptMs = 0;
bool hasFreshStatus = false;
bool wasWifiConnected = false;

bool parseState(const char* value, AgentState& parsed) {
  if (strcmp(value, "ready") == 0) parsed = AgentState::READY;
  else if (strcmp(value, "working") == 0) parsed = AgentState::WORKING;
  else if (strcmp(value, "blocked") == 0) parsed = AgentState::BLOCKED;
  else if (strcmp(value, "permission") == 0) parsed = AgentState::PERMISSION;
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

void setCurrentState(AgentState state, const char* agent, uint32_t now) {
  currentState = state;
  currentAgent = agent;
  lastValidUpdateMs = now;
  hasFreshStatus = true;
}

void expireStatusIfNeeded(uint32_t now) {
  if (hasFreshStatus && static_cast<uint32_t>(now - lastValidUpdateMs) >= STATUS_TIMEOUT_MS) {
    currentState = AgentState::READY;
    currentAgent = "";
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
  doc["agent"] = currentAgent;
  doc["expires_in_seconds"] = expiresInSeconds(now);
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleStatusGet() {
  sendStatusResponse();
}

void handleStatusPost() {
  String body = server.arg("plain");
  if (body.length() > MAX_REQUEST_BYTES) {
    sendJsonError(400, "body exceeds 256 bytes");
    return;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) {
    sendJsonError(400, "malformed JSON");
    return;
  }
  if (!doc["state"].is<const char*>()) {
    sendJsonError(400, "state must be a string");
    return;
  }

  AgentState parsed;
  if (!parseState(doc["state"].as<const char*>(), parsed)) {
    sendJsonError(400, "unsupported state");
    return;
  }

  const char* agent = "";
  if (!doc["agent"].isNull()) {
    if (!doc["agent"].is<const char*>()) {
      sendJsonError(400, "agent must be a string");
      return;
    }
    agent = doc["agent"].as<const char*>();
    if (strlen(agent) > MAX_AGENT_BYTES) {
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
