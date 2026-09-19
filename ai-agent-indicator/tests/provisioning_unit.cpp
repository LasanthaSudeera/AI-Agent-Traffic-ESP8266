#include <cstdint>
#include <cstring>
#include <iostream>

#include "../DeviceName.h"
#include "../DoubleResetTrigger.h"
#include "../HoldButton.h"
#include "../ProvisioningPolicy.h"

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { \
  std::cerr << __LINE__ << ": " #expr "\n"; ++failures; \
} } while (0)

static uint32_t rtcValue = 0;
static bool rtcReadOk = true;
static bool rtcWriteOk = true;
static bool fakeRead(uint32_t block, uint32_t* data, size_t size) {
  CHECK(block == 64U); CHECK(size == sizeof(uint32_t));
  if (!rtcReadOk) return false;
  *data = rtcValue;
  return true;
}
static bool fakeWrite(uint32_t block, const uint32_t* data, size_t size) {
  CHECK(block == 64U); CHECK(size == sizeof(uint32_t));
  if (!rtcWriteOk) return false;
  rtcValue = *data;
  return true;
}

static void testDeviceNames() {
  char friendly[DEVICE_NAME_CAPACITY];
  char hostname[DEVICE_NAME_CAPACITY];

  CHECK(normalizeDeviceName("  Desk_light--1  ", friendly, hostname));
  CHECK(std::strcmp(friendly, "Desk_light--1") == 0);
  CHECK(std::strcmp(hostname, "Desk-light-1") == 0);

  CHECK(normalizeDeviceName("12345678901234567890123456789012", friendly, hostname));
  CHECK(std::strcmp(friendly, "12345678901234567890123456789012") == 0);
  CHECK(std::strcmp(hostname, "12345678901234567890123456789012") == 0);

  std::strcpy(friendly, "not-empty");
  std::strcpy(hostname, "not-empty");
  CHECK(!normalizeDeviceName(nullptr, friendly, hostname));
  CHECK(friendly[0] == '\0');
  CHECK(hostname[0] == '\0');

  std::strcpy(friendly, "not-empty");
  std::strcpy(hostname, "not-empty");
  CHECK(!normalizeDeviceName("", friendly, hostname));
  CHECK(friendly[0] == '\0');
  CHECK(hostname[0] == '\0');

  std::strcpy(friendly, "not-empty");
  std::strcpy(hostname, "not-empty");
  CHECK(!normalizeDeviceName("   ", friendly, hostname));
  CHECK(friendly[0] == '\0');
  CHECK(hostname[0] == '\0');

  std::strcpy(friendly, "not-empty");
  std::strcpy(hostname, "not-empty");
  CHECK(!normalizeDeviceName("___", friendly, hostname));
  CHECK(friendly[0] == '\0');
  CHECK(hostname[0] == '\0');

  std::strcpy(friendly, "not-empty");
  std::strcpy(hostname, "not-empty");
  CHECK(!normalizeDeviceName("bad/name", friendly, hostname));
  CHECK(friendly[0] == '\0');
  CHECK(hostname[0] == '\0');

  std::strcpy(friendly, "not-empty");
  std::strcpy(hostname, "not-empty");
  CHECK(!normalizeDeviceName("Caf\xc3\xa9", friendly, hostname));
  CHECK(friendly[0] == '\0');
  CHECK(hostname[0] == '\0');

  std::strcpy(friendly, "not-empty");
  std::strcpy(hostname, "not-empty");
  CHECK(!normalizeDeviceName("123456789012345678901234567890123", friendly, hostname));
  CHECK(friendly[0] == '\0');
  CHECK(hostname[0] == '\0');
}

static void testHoldButton() {
  HoldButton button(30U, 3000U);
  CHECK(!button.update(true, 0U));
  CHECK(!button.update(true, 29U));
  CHECK(!button.update(true, 30U));
  CHECK(!button.update(true, 3029U));
  CHECK(button.update(true, 3030U));
  CHECK(!button.update(true, 3031U));

  CHECK(!button.update(false, 3031U));
  CHECK(!button.update(false, 3061U));
  CHECK(!button.update(true, 3062U));
  CHECK(!button.update(true, 3092U));
  CHECK(button.update(true, 6092U));
}

static void testDoubleReset() {
  rtcValue = 0U;
  rtcReadOk = true;
  rtcWriteOk = true;
  DoubleResetTrigger first(fakeRead, fakeWrite, 10000U);
  CHECK(first.begin(100U) == DoubleResetStatus::Armed);
  CHECK(rtcValue == DoubleResetTrigger::kMarker);
  DoubleResetTrigger second(fakeRead, fakeWrite, 10000U);
  CHECK(second.begin(200U) == DoubleResetStatus::Detected);
  CHECK(rtcValue == 0U);

  rtcValue = 0U;
  DoubleResetTrigger timeout(fakeRead, fakeWrite, 10000U);
  CHECK(timeout.begin(0U) == DoubleResetStatus::Armed);
  CHECK(timeout.process(9999U));
  CHECK(rtcValue == DoubleResetTrigger::kMarker);
  CHECK(timeout.process(10000U));
  CHECK(rtcValue == 0U);

  rtcValue = 0U;
  DoubleResetTrigger rollover(fakeRead, fakeWrite, 10000U);
  CHECK(rollover.begin(UINT32_MAX - 50U) == DoubleResetStatus::Armed);
  CHECK(rollover.process(9948U));
  CHECK(rtcValue == DoubleResetTrigger::kMarker);
  CHECK(rollover.process(9949U));
  CHECK(rtcValue == 0U);

  rtcValue = 0U;
  rtcReadOk = false;
  DoubleResetTrigger readFailure(fakeRead, fakeWrite, 10000U);
  CHECK(readFailure.begin(0U) == DoubleResetStatus::StorageError);
  rtcReadOk = true;
  rtcWriteOk = false;
  DoubleResetTrigger writeFailure(fakeRead, fakeWrite, 10000U);
  CHECK(writeFailure.begin(0U) == DoubleResetStatus::StorageError);
  rtcWriteOk = true;

  rtcValue = DoubleResetTrigger::kMarker;
  rtcWriteOk = false;
  DoubleResetTrigger detectedClearFailure(fakeRead, fakeWrite, 10000U);
  CHECK(detectedClearFailure.begin(0U) == DoubleResetStatus::StorageError);
  CHECK(rtcValue == DoubleResetTrigger::kMarker);
  rtcWriteOk = true;

  rtcValue = 0U;
  DoubleResetTrigger timeoutRetry(fakeRead, fakeWrite, 10000U);
  CHECK(timeoutRetry.begin(0U) == DoubleResetStatus::Armed);
  rtcWriteOk = false;
  CHECK(!timeoutRetry.process(10000U));
  CHECK(rtcValue == DoubleResetTrigger::kMarker);
  rtcWriteOk = true;
  CHECK(timeoutRetry.process(10001U));
  CHECK(rtcValue == 0U);

  rtcValue = 0U;
  DoubleResetTrigger disarmed(fakeRead, fakeWrite, 10000U);
  CHECK(disarmed.begin(0U) == DoubleResetStatus::Armed);
  CHECK(disarmed.disarm());
  CHECK(rtcValue == 0U);
  DoubleResetTrigger laterBoot(fakeRead, fakeWrite, 10000U);
  CHECK(laterBoot.begin(100U) == DoubleResetStatus::Armed);
  CHECK(rtcValue == DoubleResetTrigger::kMarker);
}

static void testProvisioningPolicy() {
  ProvisioningPolicy firstBoot(300000U, 10000U);
  CHECK(firstBoot.begin(0U, false, false) == ProvisioningAction::OpenPortal);
  CHECK(firstBoot.mode() == ProvisioningMode::Portal);

  ProvisioningPolicy saved(300000U, 10000U);
  CHECK(saved.begin(0U, true, false) == ProvisioningAction::StartConnection);
  CHECK(saved.update(299999U, false, false, false) == ProvisioningAction::RetryConnection);
  CHECK(saved.update(300000U, false, false, false) == ProvisioningAction::OpenPortal);
  CHECK(saved.mode() == ProvisioningMode::Portal);

  ProvisioningPolicy reconnect(300000U, 10000U);
  CHECK(reconnect.begin(0U, true, false) == ProvisioningAction::StartConnection);
  CHECK(reconnect.update(10U, true, false, false) == ProvisioningAction::Connected);
  CHECK(reconnect.mode() == ProvisioningMode::Connected);
  CHECK(reconnect.update(20U, false, false, false) == ProvisioningAction::StartConnection);
  CHECK(reconnect.update(300019U, false, false, false) == ProvisioningAction::RetryConnection);
  CHECK(reconnect.update(300020U, false, false, false) == ProvisioningAction::OpenPortal);

  ProvisioningPolicy manual(300000U, 10000U);
  CHECK(manual.begin(0U, true, false) == ProvisioningAction::StartConnection);
  CHECK(manual.update(1U, false, false, true) == ProvisioningAction::OpenPortal);
  CHECK(manual.mode() == ProvisioningMode::Portal);
  CHECK(manual.update(2U, false, true, true) == ProvisioningAction::None);
  CHECK(manual.mode() == ProvisioningMode::Portal);

  ProvisioningPolicy connectedManual(300000U, 10000U);
  CHECK(connectedManual.begin(0U, true, false) == ProvisioningAction::StartConnection);
  CHECK(connectedManual.update(1U, true, false, false) == ProvisioningAction::Connected);
  CHECK(connectedManual.update(2U, true, false, true) == ProvisioningAction::OpenPortal);

  ProvisioningPolicy closesConnected(300000U, 10000U);
  CHECK(closesConnected.begin(0U, false, false) == ProvisioningAction::OpenPortal);
  CHECK(closesConnected.update(1U, true, false, false) == ProvisioningAction::Connected);
  CHECK(closesConnected.mode() == ProvisioningMode::Connected);

  ProvisioningPolicy closesDisconnected(300000U, 10000U);
  CHECK(closesDisconnected.begin(0U, false, false) == ProvisioningAction::OpenPortal);
  CHECK(closesDisconnected.update(1U, false, false, false) == ProvisioningAction::StartConnection);
  CHECK(closesDisconnected.mode() == ProvisioningMode::Connecting);

  ProvisioningPolicy rollover(300000U, 10000U);
  CHECK(rollover.begin(UINT32_MAX - 100U, true, false) == ProvisioningAction::StartConnection);
  CHECK(rollover.update(9899U, false, false, false) == ProvisioningAction::RetryConnection);
  CHECK(rollover.update(299898U, false, false, false) == ProvisioningAction::RetryConnection);
  CHECK(rollover.update(299899U, false, false, false) == ProvisioningAction::OpenPortal);
}

int main() {
  testDeviceNames();
  testHoldButton();
  testDoubleReset();
  testProvisioningPolicy();
  if (failures) return 1;
  std::cout << "provisioning unit tests passed\n";
  return 0;
}
