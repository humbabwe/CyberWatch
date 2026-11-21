#pragma once

// CyberWatch: Apple Notification Center Service (ANCS) client.
//
// The iPhone is the ANCS server (Notification Provider); the watch is the
// client (Notification Consumer). iOS only publishes ANCS on an encrypted,
// bonded link, so this client asks for security first if the link is not
// encrypted yet, then finds ANCS, subscribes to Data Source and Notification
// Source, and fetches the sender + text of each new notification.
//
// Only notifications from the Messages app (com.apple.MobileSMS) are kept;
// they go to the CyberAlertStore for the CyberAlert screen.
// Notifications that already existed on the phone when the watch connected
// are skipped, so old texts aren't replayed after a reconnect. The watch never
// asks the phone to remove anything, so the original always stays on the phone.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#undef max
#undef min
#include "components/ble/BleClient.h"
#include "components/ble/CyberAlertStore.h"

namespace Pinetime {

  namespace System {
    class SystemTask;
  }

  namespace Controllers {

    class AppleNotificationCenterClient : public BleClient {
    public:
      AppleNotificationCenterClient(Pinetime::System::SystemTask& systemTask, CyberAlertStore& cyberAlertStore);

      void Discover(uint16_t connectionHandle, std::function<void(uint16_t)> onServiceDiscovered) override;
      void OnEncryptionChanged(uint16_t connectionHandle);
      void OnNotification(ble_gap_event* event);
      void Reset();

      bool IsReady() const {
        return state == State::Ready;
      }

      // Called from the NimBLE C callbacks
      int OnServiceDiscoveryEvent(uint16_t connectionHandle, const ble_gatt_error* error, const ble_gatt_svc* service);
      int OnCharacteristicDiscoveryEvent(uint16_t connectionHandle, const ble_gatt_error* error, const ble_gatt_chr* characteristic);
      int OnDescriptorDiscoveryEvent(uint16_t connectionHandle,
                                     const ble_gatt_error* error,
                                     uint16_t characteristicValueHandle,
                                     const ble_gatt_dsc* descriptor);
      int OnSubscribeEvent(uint16_t connectionHandle, const ble_gatt_error* error, ble_gatt_attr* attribute);
      int OnControlPointWriteEvent(uint16_t connectionHandle, const ble_gatt_error* error, ble_gatt_attr* attribute);

      static constexpr ble_uuid128_t ancsServiceUuid {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0xd0, 0x00, 0x2d, 0x12, 0x1e, 0x4b, 0x0f, 0xa4, 0x99, 0x4e, 0xce, 0xb5, 0x31, 0xf4, 0x05, 0x79}};

    private:
      enum class State : uint8_t {
        Idle,
        WaitingForEncryption,
        DiscoveringService,
        DiscoveringCharacteristics,
        DiscoveringDataSourceDescriptor,
        SubscribingDataSource,
        DiscoveringNotificationSourceDescriptor,
        SubscribingNotificationSource,
        Ready
      };

      // Response parser for Get Notification Attributes, which iOS may split over several Data Source notifications
      enum class ParseState : uint8_t { Header, AttributeId, LengthLow, LengthHigh, Value, Done };

      static constexpr ble_uuid128_t notificationSourceUuid {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0xbd, 0x1d, 0xa2, 0x99, 0xe6, 0x25, 0x58, 0x8c, 0xd9, 0x42, 0x01, 0x63, 0x0d, 0x12, 0xbf, 0x9f}};
      static constexpr ble_uuid128_t controlPointUuid {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0xd9, 0xd9, 0xaa, 0xfd, 0xbd, 0x9b, 0x21, 0x98, 0xa8, 0x49, 0xe1, 0x45, 0xf3, 0xd8, 0xd1, 0x69}};
      static constexpr ble_uuid128_t dataSourceUuid {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0xfb, 0x7b, 0x7c, 0xce, 0x6a, 0xb3, 0x44, 0xbe, 0xb5, 0x4b, 0xd6, 0x24, 0xe9, 0xc6, 0xea, 0x22}};
      static constexpr uint16_t cccdUuid = 0x2902;

      // ANCS protocol values (Apple ANCS specification)
      static constexpr uint8_t eventIdAdded = 0;
      static constexpr uint8_t eventIdRemoved = 2;
      static constexpr uint8_t eventFlagPreExisting = 1 << 2;
      static constexpr uint8_t commandGetNotificationAttributes = 0;
      static constexpr uint8_t attributeAppIdentifier = 0;
      static constexpr uint8_t attributeTitle = 1;
      static constexpr uint8_t attributeMessage = 3;
      static constexpr uint8_t requestedAttributeCount = 3;

      static constexpr const char* messagesAppId = "com.apple.MobileSMS";
      static constexpr size_t appIdMaxSize = 32;
      static constexpr size_t titleMaxSize = CyberAlertStore::senderMaxLength;
      static constexpr size_t messageMaxSize = CyberAlertStore::textMaxLength;
      static constexpr uint32_t requestTimeoutMs = 5000;

      void StartServiceDiscovery(uint16_t connectionHandle);
      void RequestSecurity(uint16_t connectionHandle);
      void FinishDiscoveryStep(uint16_t connectionHandle);
      void Subscribe(uint16_t connectionHandle, uint16_t cccdHandle);
      static bool IsSecurityError(const ble_gatt_error* error);

      void OnNotificationSource(const os_mbuf* om);
      void OnDataSource(const os_mbuf* om);
      void ParseByte(uint8_t byte);
      void OnAttributesComplete();

      void QueuePush(uint32_t uid);
      void QueueRemove(uint32_t uid);
      void FetchNext();
      void EndRequest();

      Pinetime::System::SystemTask& systemTask;
      CyberAlertStore& cyberAlertStore;
      std::function<void(uint16_t)> onServiceDiscovered;

      State state = State::Idle;
      uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE;
      uint16_t serviceStartHandle = 0;
      uint16_t serviceEndHandle = 0;
      uint16_t notificationSourceHandle = 0;
      uint16_t notificationSourceEndHandle = 0;
      uint16_t notificationSourceCccdHandle = 0;
      uint16_t controlPointHandle = 0;
      uint16_t dataSourceHandle = 0;
      uint16_t dataSourceEndHandle = 0;
      uint16_t dataSourceCccdHandle = 0;
      uint16_t* pendingEndHandle = nullptr;

      // Notification UIDs waiting to be fetched
      static constexpr size_t queueSize = 4;
      std::array<uint32_t, queueSize> queue {};
      size_t queueCount = 0;

      // Request in flight
      bool requestPending = false;
      uint32_t requestUid = 0;
      uint32_t requestStartTick = 0;

      ParseState parseState = ParseState::Header;
      std::array<uint8_t, 5> header {};
      uint8_t headerCount = 0;
      uint8_t attributesParsed = 0;
      uint8_t currentAttribute = 0;
      uint16_t valueRemaining = 0;

      std::array<char, appIdMaxSize + 1> appId {};
      size_t appIdLength = 0;
      CyberAlertStore::Alert* alert = nullptr; // where the sender and text are written (nullptr: no room, discard)
    };
  }
}
