#pragma once

// CyberWatch: holds incoming CyberAlert texts until they are played.
//
// Written by the BLE task (ANCS client), read by the display task (CyberAlert
// screen). One writer and one reader, so a small ring buffer with atomic
// indexes is enough: one slot is always free for writing, the others hold
// texts waiting to be played. A text is removed once it has been played.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Pinetime {
  namespace Controllers {
    class CyberAlertStore {
    public:
      static constexpr size_t senderMaxLength = 32;
      static constexpr size_t textMaxLength = 480;

      struct Alert {
        std::array<char, senderMaxLength + 1> sender {};
        uint16_t senderLength = 0;
        std::array<char, textMaxLength + 1> text {};
        uint16_t textLength = 0;
      };

      // Writer (BLE task)
      // Returns the free slot to fill, or nullptr if every slot is waiting to be played
      Alert* BeginWrite();
      // Cleans up the text for the watch font and makes it available to the reader
      void Commit();

      // Reader (display task)
      bool HasPending() const;
      const Alert* Front() const;
      void Pop();

    private:
      static constexpr uint8_t slotCount = 3; // 2 waiting + 1 being written
      static uint16_t Sanitize(char* data, uint16_t length);

      std::array<Alert, slotCount> slots;
      std::atomic<uint8_t> head {0}; // oldest waiting text
      std::atomic<uint8_t> tail {0}; // slot being written
    };
  }
}
