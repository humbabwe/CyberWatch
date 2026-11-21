#include "components/ble/AppleNotificationCenterClient.h"
#include <algorithm>
#include <cstring>
#include <FreeRTOS.h>
#include <task.h>
#include <nrf_log.h>
#include "systemtask/SystemTask.h"

using namespace Pinetime::Controllers;

constexpr ble_uuid128_t AppleNotificationCenterClient::ancsServiceUuid;
constexpr ble_uuid128_t AppleNotificationCenterClient::notificationSourceUuid;
constexpr ble_uuid128_t AppleNotificationCenterClient::controlPointUuid;
constexpr ble_uuid128_t AppleNotificationCenterClient::dataSourceUuid;

namespace {
  int OnServiceDiscoveredCallback(uint16_t conn_handle, const struct ble_gatt_error* error, const struct ble_gatt_svc* service, void* arg) {
    return static_cast<AppleNotificationCenterClient*>(arg)->OnServiceDiscoveryEvent(conn_handle, error, service);
  }

  int OnCharacteristicDiscoveredCallback(uint16_t conn_handle,
                                         const struct ble_gatt_error* error,
                                         const struct ble_gatt_chr* chr,
                                         void* arg) {
    return static_cast<AppleNotificationCenterClient*>(arg)->OnCharacteristicDiscoveryEvent(conn_handle, error, chr);
  }

  int OnDescriptorDiscoveredCallback(uint16_t conn_handle,
                                     const struct ble_gatt_error* error,
                                     uint16_t chr_val_handle,
                                     const struct ble_gatt_dsc* dsc,
                                     void* arg) {
    return static_cast<AppleNotificationCenterClient*>(arg)->OnDescriptorDiscoveryEvent(conn_handle, error, chr_val_handle, dsc);
  }

  int OnSubscribeCallback(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
    return static_cast<AppleNotificationCenterClient*>(arg)->OnSubscribeEvent(conn_handle, error, attr);
  }

  int OnControlPointWriteCallback(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
    return static_cast<AppleNotificationCenterClient*>(arg)->OnControlPointWriteEvent(conn_handle, error, attr);
  }

  uint32_t ReadUint32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) | (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
  }
}

AppleNotificationCenterClient::AppleNotificationCenterClient(Pinetime::System::SystemTask& systemTask, CyberAlertStore& cyberAlertStore)
  : systemTask {systemTask}, cyberAlertStore {cyberAlertStore} {
}

// ---------------------------------------------------------------------------
// Discovery and subscription
// ---------------------------------------------------------------------------

void AppleNotificationCenterClient::Discover(uint16_t connectionHandle, std::function<void(uint16_t)> onServiceDiscovered) {
  NRF_LOG_INFO("[ANCS] Starting discovery");
  this->onServiceDiscovered = onServiceDiscovered;
  this->connectionHandle = connectionHandle;

  ble_gap_conn_desc desc;
  if (ble_gap_conn_find(connectionHandle, &desc) != 0) {
    FinishDiscoveryStep(connectionHandle);
    return;
  }

  if (!desc.sec_state.encrypted) {
    // iOS only publishes ANCS on an encrypted link. Ask the phone to encrypt
    // (or to pair, the first time). Discovery resumes in OnEncryptionChanged().
    RequestSecurity(connectionHandle);
    FinishDiscoveryStep(connectionHandle);
    return;
  }

  StartServiceDiscovery(connectionHandle);
}

void AppleNotificationCenterClient::OnEncryptionChanged(uint16_t connectionHandle) {
  if (state == State::WaitingForEncryption && connectionHandle == this->connectionHandle) {
    NRF_LOG_INFO("[ANCS] Link encrypted, resuming discovery");
    StartServiceDiscovery(connectionHandle);
  }
}

void AppleNotificationCenterClient::RequestSecurity(uint16_t connectionHandle) {
  NRF_LOG_INFO("[ANCS] Requesting security");
  state = State::WaitingForEncryption;
  ble_gap_security_initiate(connectionHandle);
}

void AppleNotificationCenterClient::StartServiceDiscovery(uint16_t connectionHandle) {
  state = State::DiscoveringService;
  serviceStartHandle = 0;
  serviceEndHandle = 0;
  if (ble_gattc_disc_svc_by_uuid(connectionHandle, &ancsServiceUuid.u, OnServiceDiscoveredCallback, this) != 0) {
    state = State::Idle;
    FinishDiscoveryStep(connectionHandle);
  }
}

void AppleNotificationCenterClient::FinishDiscoveryStep(uint16_t connectionHandle) {
  // Let ServiceDiscovery move on to the next client. Only done once per Discover().
  if (onServiceDiscovered) {
    auto callback = std::move(onServiceDiscovered);
    onServiceDiscovered = nullptr;
    callback(connectionHandle);
  }
}

bool AppleNotificationCenterClient::IsSecurityError(const ble_gatt_error* error) {
  return error->status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) || error->status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC);
}

int AppleNotificationCenterClient::OnServiceDiscoveryEvent(uint16_t connectionHandle,
                                                           const ble_gatt_error* error,
                                                           const ble_gatt_svc* service) {
  if (state != State::DiscoveringService) {
    return 0;
  }

  if (error->status == 0 && service != nullptr) {
    if (ble_uuid_cmp(&ancsServiceUuid.u, &service->uuid.u) == 0) {
      NRF_LOG_INFO("[ANCS] Service found: 0x%x - 0x%x", service->start_handle, service->end_handle);
      serviceStartHandle = service->start_handle;
      serviceEndHandle = service->end_handle;
    }
    return 0;
  }

  if (serviceStartHandle == 0) {
    NRF_LOG_INFO("[ANCS] Service not found (not an iPhone, or notifications not shared)");
    state = State::Idle;
    FinishDiscoveryStep(connectionHandle);
    return 0;
  }

  state = State::DiscoveringCharacteristics;
  notificationSourceHandle = notificationSourceEndHandle = notificationSourceCccdHandle = 0;
  controlPointHandle = 0;
  dataSourceHandle = dataSourceEndHandle = dataSourceCccdHandle = 0;
  pendingEndHandle = nullptr;
  if (ble_gattc_disc_all_chrs(connectionHandle, serviceStartHandle, serviceEndHandle, OnCharacteristicDiscoveredCallback, this) != 0) {
    state = State::Idle;
    FinishDiscoveryStep(connectionHandle);
  }
  return 0;
}

int AppleNotificationCenterClient::OnCharacteristicDiscoveryEvent(uint16_t connectionHandle,
                                                                  const ble_gatt_error* error,
                                                                  const ble_gatt_chr* characteristic) {
  if (state != State::DiscoveringCharacteristics) {
    return 0;
  }

  if (error->status == 0 && characteristic != nullptr) {
    // A characteristic ends right before the next one starts. Remember where
    // Notification Source and Data Source end so their descriptors can be found.
    if (pendingEndHandle != nullptr) {
      *pendingEndHandle = characteristic->def_handle - 1;
      pendingEndHandle = nullptr;
    }

    if (ble_uuid_cmp(&notificationSourceUuid.u, &characteristic->uuid.u) == 0) {
      notificationSourceHandle = characteristic->val_handle;
      notificationSourceEndHandle = serviceEndHandle;
      pendingEndHandle = &notificationSourceEndHandle;
    } else if (ble_uuid_cmp(&dataSourceUuid.u, &characteristic->uuid.u) == 0) {
      dataSourceHandle = characteristic->val_handle;
      dataSourceEndHandle = serviceEndHandle;
      pendingEndHandle = &dataSourceEndHandle;
    } else if (ble_uuid_cmp(&controlPointUuid.u, &characteristic->uuid.u) == 0) {
      controlPointHandle = characteristic->val_handle;
    }
    return 0;
  }

  pendingEndHandle = nullptr;
  if (notificationSourceHandle == 0 || controlPointHandle == 0 || dataSourceHandle == 0) {
    NRF_LOG_INFO("[ANCS] Missing characteristics");
    state = State::Idle;
    FinishDiscoveryStep(connectionHandle);
    return 0;
  }

  // Apple recommends subscribing to Data Source before Notification Source
  state = State::DiscoveringDataSourceDescriptor;
  if (ble_gattc_disc_all_dscs(connectionHandle, dataSourceHandle, dataSourceEndHandle, OnDescriptorDiscoveredCallback, this) != 0) {
    state = State::Idle;
    FinishDiscoveryStep(connectionHandle);
  }
  return 0;
}

int AppleNotificationCenterClient::OnDescriptorDiscoveryEvent(uint16_t connectionHandle,
                                                              const ble_gatt_error* error,
                                                              uint16_t /*characteristicValueHandle*/,
                                                              const ble_gatt_dsc* descriptor) {
  const bool forDataSource = state == State::DiscoveringDataSourceDescriptor;
  const bool forNotificationSource = state == State::DiscoveringNotificationSourceDescriptor;
  if (!forDataSource && !forNotificationSource) {
    return 0;
  }

  if (error->status == 0 && descriptor != nullptr) {
    if (descriptor->uuid.u.type == BLE_UUID_TYPE_16 && BLE_UUID16(&descriptor->uuid.u)->value == cccdUuid) {
      (forDataSource ? dataSourceCccdHandle : notificationSourceCccdHandle) = descriptor->handle;
    }
    return 0;
  }

  const uint16_t cccdHandle = forDataSource ? dataSourceCccdHandle : notificationSourceCccdHandle;
  if (cccdHandle == 0) {
    NRF_LOG_INFO("[ANCS] CCCD not found");
    state = State::Idle;
    FinishDiscoveryStep(connectionHandle);
    return 0;
  }

  state = forDataSource ? State::SubscribingDataSource : State::SubscribingNotificationSource;
  Subscribe(connectionHandle, cccdHandle);
  return 0;
}

void AppleNotificationCenterClient::Subscribe(uint16_t connectionHandle, uint16_t cccdHandle) {
  static constexpr uint8_t enableNotifications[2] = {0x01, 0x00};
  if (ble_gattc_write_flat(connectionHandle, cccdHandle, enableNotifications, sizeof(enableNotifications), OnSubscribeCallback, this) !=
      0) {
    state = State::Idle;
    FinishDiscoveryStep(connectionHandle);
  }
}

int AppleNotificationCenterClient::OnSubscribeEvent(uint16_t connectionHandle, const ble_gatt_error* error, ble_gatt_attr* /*attribute*/) {
  if (error->status != 0) {
    if (IsSecurityError(error)) {
      // Shouldn't happen since discovery waits for encryption, but iOS can ask
      // for a stronger link. Re-request security; discovery restarts afterwards.
      RequestSecurity(connectionHandle);
    } else {
      NRF_LOG_INFO("[ANCS] Subscribe failed: %d", error->status);
      state = State::Idle;
    }
    FinishDiscoveryStep(connectionHandle);
    return 0;
  }

  if (state == State::SubscribingDataSource) {
    state = State::DiscoveringNotificationSourceDescriptor;
    if (ble_gattc_disc_all_dscs(connectionHandle,
                                notificationSourceHandle,
                                notificationSourceEndHandle,
                                OnDescriptorDiscoveredCallback,
                                this) != 0) {
      state = State::Idle;
      FinishDiscoveryStep(connectionHandle);
    }
  } else if (state == State::SubscribingNotificationSource) {
    NRF_LOG_INFO("[ANCS] Ready");
    state = State::Ready;
    FinishDiscoveryStep(connectionHandle);
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Incoming notifications
// ---------------------------------------------------------------------------

void AppleNotificationCenterClient::OnNotification(ble_gap_event* event) {
  if (state != State::Ready || event->notify_rx.conn_handle != connectionHandle) {
    return;
  }
  if (event->notify_rx.attr_handle == notificationSourceHandle) {
    OnNotificationSource(event->notify_rx.om);
  } else if (event->notify_rx.attr_handle == dataSourceHandle) {
    OnDataSource(event->notify_rx.om);
  }
}

void AppleNotificationCenterClient::OnNotificationSource(const os_mbuf* om) {
  // EventID, EventFlags, CategoryID, CategoryCount, NotificationUID (4 bytes, little endian)
  uint8_t data[8];
  if (OS_MBUF_PKTLEN(om) < sizeof(data) || os_mbuf_copydata(om, 0, sizeof(data), data) != 0) {
    return;
  }
  const uint8_t eventId = data[0];
  const uint8_t eventFlags = data[1];
  const uint32_t uid = ReadUint32(&data[4]);

  if (eventId == eventIdAdded && (eventFlags & eventFlagPreExisting) == 0) {
    QueuePush(uid);
    FetchNext();
  } else if (eventId == eventIdRemoved) {
    // Read or cleared on the phone before the watch fetched it
    QueueRemove(uid);
  }
}

void AppleNotificationCenterClient::QueuePush(uint32_t uid) {
  if (queueCount == queueSize) {
    // Drop the oldest
    for (size_t i = 1; i < queueSize; i++) {
      queue[i - 1] = queue[i];
    }
    queueCount--;
  }
  queue[queueCount++] = uid;
}

void AppleNotificationCenterClient::QueueRemove(uint32_t uid) {
  size_t out = 0;
  for (size_t i = 0; i < queueCount; i++) {
    if (queue[i] != uid) {
      queue[out++] = queue[i];
    }
  }
  queueCount = out;
}

void AppleNotificationCenterClient::FetchNext() {
  if (requestPending) {
    // Don't stay stuck if the phone never finished answering the last request
    if ((xTaskGetTickCount() - requestStartTick) < pdMS_TO_TICKS(requestTimeoutMs)) {
      return;
    }
    NRF_LOG_INFO("[ANCS] Request timed out");
    EndRequest();
  }
  if (queueCount == 0) {
    return;
  }

  requestUid = queue[0];
  QueueRemove(requestUid);

  // Get Notification Attributes: app ID, title (sender) and message
  const uint8_t request[] = {
    commandGetNotificationAttributes,
    static_cast<uint8_t>(requestUid & 0xff),
    static_cast<uint8_t>((requestUid >> 8) & 0xff),
    static_cast<uint8_t>((requestUid >> 16) & 0xff),
    static_cast<uint8_t>((requestUid >> 24) & 0xff),
    attributeAppIdentifier,
    attributeTitle,
    static_cast<uint8_t>(titleMaxSize & 0xff),
    static_cast<uint8_t>(titleMaxSize >> 8),
    attributeMessage,
    static_cast<uint8_t>(messageMaxSize & 0xff),
    static_cast<uint8_t>(messageMaxSize >> 8),
  };

  parseState = ParseState::Header;
  headerCount = 0;
  attributesParsed = 0;
  appIdLength = 0;
  alert = cyberAlertStore.BeginWrite();
  requestPending = true;
  requestStartTick = xTaskGetTickCount();

  if (ble_gattc_write_flat(connectionHandle, controlPointHandle, request, sizeof(request), OnControlPointWriteCallback, this) != 0) {
    EndRequest();
  }
}

int AppleNotificationCenterClient::OnControlPointWriteEvent(uint16_t /*connectionHandle*/,
                                                            const ble_gatt_error* error,
                                                            ble_gatt_attr* /*attribute*/) {
  if (error->status != 0) {
    // e.g. 0xA2 "invalid parameter" if the notification is already gone
    NRF_LOG_INFO("[ANCS] Control point error: %d", error->status);
    EndRequest();
    FetchNext();
  }
  return 0;
}

void AppleNotificationCenterClient::EndRequest() {
  requestPending = false;
  parseState = ParseState::Done;
}

void AppleNotificationCenterClient::OnDataSource(const os_mbuf* om) {
  if (!requestPending) {
    return;
  }
  const uint16_t length = OS_MBUF_PKTLEN(om);
  uint8_t chunk[32];
  for (uint16_t offset = 0; offset < length && requestPending; offset += sizeof(chunk)) {
    const uint16_t count = std::min<uint16_t>(sizeof(chunk), length - offset);
    if (os_mbuf_copydata(om, offset, count, chunk) != 0) {
      EndRequest();
      break;
    }
    for (uint16_t i = 0; i < count && requestPending; i++) {
      ParseByte(chunk[i]);
    }
  }
  if (!requestPending) {
    FetchNext();
  }
}

void AppleNotificationCenterClient::ParseByte(uint8_t byte) {
  switch (parseState) {
    case ParseState::Header:
      header[headerCount++] = byte;
      if (headerCount == header.size()) {
        if (header[0] != commandGetNotificationAttributes || ReadUint32(&header[1]) != requestUid) {
          NRF_LOG_INFO("[ANCS] Unexpected response");
          EndRequest();
          return;
        }
        parseState = ParseState::AttributeId;
      }
      break;

    case ParseState::AttributeId:
      currentAttribute = byte;
      parseState = ParseState::LengthLow;
      break;

    case ParseState::LengthLow:
      valueRemaining = byte;
      parseState = ParseState::LengthHigh;
      break;

    case ParseState::LengthHigh:
      valueRemaining |= static_cast<uint16_t>(byte) << 8;
      parseState = ParseState::Value;
      if (valueRemaining == 0) {
        attributesParsed++;
        parseState = ParseState::AttributeId;
      }
      break;

    case ParseState::Value: {
      // Keep what fits; extra bytes (if any) are dropped
      if (currentAttribute == attributeAppIdentifier) {
        if (appIdLength < appIdMaxSize) {
          appId[appIdLength++] = static_cast<char>(byte);
        }
      } else if (alert != nullptr && currentAttribute == attributeTitle) {
        if (alert->senderLength < CyberAlertStore::senderMaxLength) {
          alert->sender[alert->senderLength++] = static_cast<char>(byte);
        }
      } else if (alert != nullptr && currentAttribute == attributeMessage) {
        if (alert->textLength < CyberAlertStore::textMaxLength) {
          alert->text[alert->textLength++] = static_cast<char>(byte);
        }
      }
      valueRemaining--;
      if (valueRemaining == 0) {
        attributesParsed++;
        parseState = ParseState::AttributeId;
      }
    } break;

    case ParseState::Done:
      return;
  }

  if (parseState == ParseState::AttributeId && attributesParsed == requestedAttributeCount) {
    OnAttributesComplete();
    EndRequest();
  }
}

void AppleNotificationCenterClient::OnAttributesComplete() {
  appId[appIdLength] = '\0';

  if (std::strcmp(appId.data(), messagesAppId) != 0) {
    return; // CyberAlert only plays texts from Messages
  }
  if (alert == nullptr) {
    NRF_LOG_INFO("[ANCS] CyberAlert queue full, text dropped");
    return;
  }
  if (alert->textLength == 0) {
    return;
  }

  cyberAlertStore.Commit();
  alert = nullptr;

  NRF_LOG_INFO("[ANCS] New CyberAlert");
  systemTask.PushMessage(Pinetime::System::Messages::OnCyberAlert);
}

void AppleNotificationCenterClient::Reset() {
  state = State::Idle;
  connectionHandle = BLE_HS_CONN_HANDLE_NONE;
  onServiceDiscovered = nullptr;
  serviceStartHandle = serviceEndHandle = 0;
  notificationSourceHandle = notificationSourceEndHandle = notificationSourceCccdHandle = 0;
  controlPointHandle = 0;
  dataSourceHandle = dataSourceEndHandle = dataSourceCccdHandle = 0;
  pendingEndHandle = nullptr;
  queueCount = 0;
  alert = nullptr;
  EndRequest();
}
