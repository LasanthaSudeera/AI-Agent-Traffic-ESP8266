#include "DeviceName.h"

namespace {

bool isAllowed(char value) {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9') ||
         value == ' ' || value == '_' || value == '-';
}

bool isHostnameSeparator(char value) {
  return value == ' ' || value == '_' || value == '-';
}

}  // namespace

bool normalizeDeviceName(const char* input,
                         char friendly[DEVICE_NAME_CAPACITY],
                         char hostname[DEVICE_NAME_CAPACITY]) {
  if (friendly != nullptr) friendly[0] = '\0';
  if (hostname != nullptr) hostname[0] = '\0';
  if (input == nullptr || friendly == nullptr || hostname == nullptr) return false;

  const char* start = input;
  while (*start == ' ') ++start;
  const char* end = start;
  while (*end != '\0') ++end;
  while (end != start && *(end - 1) == ' ') --end;

  size_t length = static_cast<size_t>(end - start);
  if (length == 0U || length >= DEVICE_NAME_CAPACITY) return false;
  for (size_t index = 0U; index < length; ++index) {
    unsigned char value = static_cast<unsigned char>(start[index]);
    if (value > 0x7FU || !isAllowed(start[index])) return false;
  }

  char normalized[DEVICE_NAME_CAPACITY] = {};
  size_t hostnameLength = 0U;
  for (size_t index = 0U; index < length; ++index) {
    char value = start[index];
    if (isHostnameSeparator(value)) {
      if (hostnameLength != 0U && normalized[hostnameLength - 1U] != '-') {
        normalized[hostnameLength++] = '-';
      }
    } else {
      normalized[hostnameLength++] = value;
    }
  }
  if (hostnameLength != 0U && normalized[hostnameLength - 1U] == '-') --hostnameLength;
  if (hostnameLength == 0U) return false;

  for (size_t index = 0U; index < length; ++index) friendly[index] = start[index];
  friendly[length] = '\0';
  for (size_t index = 0U; index < hostnameLength; ++index) hostname[index] = normalized[index];
  hostname[hostnameLength] = '\0';
  return true;
}
