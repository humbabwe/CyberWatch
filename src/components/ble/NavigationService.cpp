/*  Copyright (C) 2021  Adam Pigg

    This file is part of InfiniTime.

    InfiniTime is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    InfiniTime is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "components/ble/NavigationService.h"
#include "components/motor/MotorController.h"

namespace {
  // 0001yyxx-78fc-48fe-8e23-433b3a1942d0
  constexpr ble_uuid128_t CharUuid(uint8_t x, uint8_t y) {
    return ble_uuid128_t {.u = {.type = BLE_UUID_TYPE_128},
                          .value = {0xd0, 0x42, 0x19, 0x3a, 0x3b, 0x43, 0x23, 0x8e, 0xfe, 0x48, 0xfc, 0x78, x, y, 0x01, 0x00}};
  }

  // 00010000-78fc-48fe-8e23-433b3a1942d0
  constexpr ble_uuid128_t BaseUuid() {
    return CharUuid(0x00, 0x00);
  }

  constexpr ble_uuid128_t navUuid {BaseUuid()};

  // CyberWatch turn alerts, in vibration Morse style
  // left = dot dash, right = dash dot, U-turn = dash dash dash, arrived = one long buzz
  constexpr uint16_t dot = 150;
  constexpr uint16_t dash = 450;
  constexpr uint16_t gap = 200;
  constexpr uint16_t leftPattern[] = {dot, gap, dash};
  constexpr uint16_t rightPattern[] = {dash, gap, dot};
  constexpr uint16_t uturnPattern[] = {dash, gap, dash, gap, dash};
  constexpr uint16_t arrivedPattern[] = {1500};

  // Buzz once the turn is this close (or right away if the distance can't be read)
  constexpr uint32_t alertDistanceMeters = 160; // about 0.1 mi / 500 ft

  bool Contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
  }

  // Reads distances like "150 m", "1.2 km", "0.1 mi", "300 ft". Returns false if it can't.
  bool ParseMeters(const std::string& text, uint32_t& meters) {
    size_t i = 0;
    while (i < text.size() && text[i] == ' ') {
      i++;
    }
    uint32_t whole = 0;
    uint32_t tenths = 0;
    bool anyDigit = false;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
      whole = whole * 10 + static_cast<uint32_t>(text[i] - '0');
      anyDigit = true;
      i++;
    }
    if (i < text.size() && (text[i] == '.' || text[i] == ',')) {
      i++;
      if (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        tenths = static_cast<uint32_t>(text[i] - '0');
        anyDigit = true;
      }
      while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        i++;
      }
    }
    if (!anyDigit) {
      return false;
    }
    while (i < text.size() && text[i] == ' ') {
      i++;
    }
    std::string unit = text.substr(i);
    uint32_t tenthsTotal = whole * 10 + tenths; // value in tenths of a unit
    if (unit.rfind("km", 0) == 0) {
      meters = tenthsTotal * 100;
    } else if (unit.rfind("mi", 0) == 0) {
      meters = tenthsTotal * 1609 / 10;
    } else if (unit.rfind("ft", 0) == 0) {
      meters = tenthsTotal * 3048 / 100000;
    } else if (unit.rfind("yd", 0) == 0) {
      meters = tenthsTotal * 9144 / 100000;
    } else if (unit.empty() || unit[0] == 'm') {
      meters = tenthsTotal / 10;
    } else {
      return false;
    }
    return true;
  }

  constexpr ble_uuid128_t navFlagCharUuid {CharUuid(0x01, 0x00)};
  constexpr ble_uuid128_t navNarrativeCharUuid {CharUuid(0x02, 0x00)};
  constexpr ble_uuid128_t navManDistCharUuid {CharUuid(0x03, 0x00)};
  constexpr ble_uuid128_t navProgressCharUuid {CharUuid(0x04, 0x00)};

  int NAVCallback(uint16_t /*conn_handle*/, uint16_t /*attr_handle*/, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    auto* navService = static_cast<Pinetime::Controllers::NavigationService*>(arg);
    return navService->OnCommand(ctxt);
  }
} // namespace

Pinetime::Controllers::NavigationService::NavigationService() {
  characteristicDefinition[0] = {.uuid = &navFlagCharUuid.u,
                                 .access_cb = NAVCallback,
                                 .arg = this,
                                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ};

  characteristicDefinition[1] = {.uuid = &navNarrativeCharUuid.u,
                                 .access_cb = NAVCallback,
                                 .arg = this,
                                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ};
  characteristicDefinition[2] = {.uuid = &navManDistCharUuid.u,
                                 .access_cb = NAVCallback,
                                 .arg = this,
                                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ};
  characteristicDefinition[3] = {.uuid = &navProgressCharUuid.u,
                                 .access_cb = NAVCallback,
                                 .arg = this,
                                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ};

  characteristicDefinition[4] = {0};

  serviceDefinition[0] = {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &navUuid.u, .characteristics = characteristicDefinition};
  serviceDefinition[1] = {0};

  m_progress = 0;
}

void Pinetime::Controllers::NavigationService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

int Pinetime::Controllers::NavigationService::OnCommand(struct ble_gatt_access_ctxt* ctxt) {

  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    size_t notifSize = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t data[notifSize + 1];
    data[notifSize] = '\0';
    os_mbuf_copydata(ctxt->om, 0, notifSize, data);
    char* s = (char*) &data[0];
    if (ble_uuid_cmp(ctxt->chr->uuid, &navFlagCharUuid.u) == 0) {
      m_flag = s;
    } else if (ble_uuid_cmp(ctxt->chr->uuid, &navNarrativeCharUuid.u) == 0) {
      m_narrative = s;
    } else if (ble_uuid_cmp(ctxt->chr->uuid, &navManDistCharUuid.u) == 0) {
      m_manDist = s;
    } else if (ble_uuid_cmp(ctxt->chr->uuid, &navProgressCharUuid.u) == 0) {
      m_progress = data[0];
    }
    CheckTurnAlert();
  }
  return 0;
}

std::string Pinetime::Controllers::NavigationService::getFlag() {
  return m_flag;
}

std::string Pinetime::Controllers::NavigationService::getNarrative() {
  return m_narrative;
}

std::string Pinetime::Controllers::NavigationService::getManDist() {
  return m_manDist;
}

int Pinetime::Controllers::NavigationService::getProgress() {
  return m_progress;
}

void Pinetime::Controllers::NavigationService::SetMotorController(MotorController* motor) {
  motorController = motor;
}

void Pinetime::Controllers::NavigationService::CheckTurnAlert() {
  if (motorController == nullptr || m_flag.empty()) {
    return;
  }
  uint32_t meters = 0;
  bool haveDistance = ParseMeters(m_manDist, meters);
  if (haveDistance) {
    // Distance jumped back up: a new maneuver started, even if it has the same arrow as the last one
    if (meters > lastMeters + 50) {
      alertedFlag.clear();
    }
    lastMeters = meters;
  }
  if (m_flag == alertedFlag) {
    return; // already buzzed for this maneuver
  }
  if (haveDistance && meters > alertDistanceMeters) {
    return; // not close yet
  }

  const uint16_t* pattern = nullptr;
  uint8_t count = 0;
  if (Contains(m_flag, "arrive")) {
    pattern = arrivedPattern;
    count = sizeof(arrivedPattern) / sizeof(arrivedPattern[0]);
  } else if (Contains(m_flag, "uturn")) {
    pattern = uturnPattern;
    count = sizeof(uturnPattern) / sizeof(uturnPattern[0]);
  } else if (Contains(m_flag, "left")) {
    pattern = leftPattern;
    count = sizeof(leftPattern) / sizeof(leftPattern[0]);
  } else if (Contains(m_flag, "right")) {
    pattern = rightPattern;
    count = sizeof(rightPattern) / sizeof(rightPattern[0]);
  }

  alertedFlag = m_flag;
  if (pattern != nullptr) {
    motorController->PlayPattern(pattern, count);
  }
}
