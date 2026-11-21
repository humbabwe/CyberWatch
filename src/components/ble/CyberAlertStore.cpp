#include "components/ble/CyberAlertStore.h"

using namespace Pinetime::Controllers;

CyberAlertStore::Alert* CyberAlertStore::BeginWrite() {
  const uint8_t writeIndex = tail.load();
  if ((writeIndex + 1) % slotCount == head.load()) {
    return nullptr;
  }
  Alert& alert = slots[writeIndex];
  alert.senderLength = 0;
  alert.textLength = 0;
  alert.sender[0] = '\0';
  alert.text[0] = '\0';
  return &alert;
}

void CyberAlertStore::Commit() {
  const uint8_t writeIndex = tail.load();
  if ((writeIndex + 1) % slotCount == head.load()) {
    return;
  }
  Alert& alert = slots[writeIndex];
  alert.senderLength = Sanitize(alert.sender.data(), alert.senderLength);
  alert.sender[alert.senderLength] = '\0';
  alert.textLength = Sanitize(alert.text.data(), alert.textLength);
  alert.text[alert.textLength] = '\0';
  tail.store((writeIndex + 1) % slotCount);
}

bool CyberAlertStore::HasPending() const {
  return head.load() != tail.load();
}

const CyberAlertStore::Alert* CyberAlertStore::Front() const {
  if (!HasPending()) {
    return nullptr;
  }
  return &slots[head.load()];
}

void CyberAlertStore::Pop() {
  if (HasPending()) {
    head.store((head.load() + 1) % slotCount);
  }
}

// The watch font only has plain ASCII. iPhone texts are UTF-8, so swap common
// punctuation for its ASCII look-alike, show anything else (emoji, accents) as
// '*', and turn line breaks into spaces for the typing grid. Never makes the
// text longer, so it is done in place.
uint16_t CyberAlertStore::Sanitize(char* data, uint16_t length) {
  const auto* in = reinterpret_cast<const uint8_t*>(data);
  uint16_t read = 0;
  uint16_t write = 0;

  auto put = [&](char c) {
    data[write++] = c;
  };

  while (read < length) {
    const uint8_t c = in[read];

    if (c < 0x80) {
      if (c == '\n' || c == '\r' || c == '\t') {
        put(' ');
      } else if (c >= 0x20 && c != 0x7f) {
        put(static_cast<char>(c));
      }
      read++;
      continue;
    }

    // Length of this UTF-8 sequence
    uint16_t sequenceLength = 1;
    if ((c & 0xe0) == 0xc0) {
      sequenceLength = 2;
    } else if ((c & 0xf0) == 0xe0) {
      sequenceLength = 3;
    } else if ((c & 0xf8) == 0xf0) {
      sequenceLength = 4;
    }
    if (read + sequenceLength > length) {
      break; // cut off mid-character
    }

    const uint8_t b1 = sequenceLength > 1 ? in[read + 1] : 0;
    const uint8_t b2 = sequenceLength > 2 ? in[read + 2] : 0;

    if (c == 0xc2 && b1 == 0xa0) {
      put(' '); // non-breaking space
    } else if (c == 0xe2 && b1 == 0x80) {
      switch (b2) {
        case 0x98: // ‘
        case 0x99: // ’
          put('\'');
          break;
        case 0x9c: // “
        case 0x9d: // ”
          put('"');
          break;
        case 0x93: // –
        case 0x94: // —
          put('-');
          break;
        case 0x8b: // zero width space
        case 0x8c: // zero width non-joiner
        case 0x8d: // zero width joiner (inside combined emoji)
          break;
        case 0xa6: // …
          put('.');
          put('.');
          put('.');
          break;
        default:
          put('*');
          break;
      }
    } else if (c == 0xef && b1 == 0xb8 && b2 == 0x8f) {
      // emoji variation selector: invisible, skip
    } else {
      put('*');
    }
    read += sequenceLength;
  }

  // Collapse "**" runs from combined emoji (e.g. skin tones) into one '*'
  uint16_t out = 0;
  for (uint16_t i = 0; i < write; i++) {
    if (data[i] == '*' && out > 0 && data[out - 1] == '*') {
      continue;
    }
    data[out++] = data[i];
  }
  return out;
}
