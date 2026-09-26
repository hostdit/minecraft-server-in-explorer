#pragma once

#include <mcx/protocol.h>

namespace mcx {

struct Slot {
    std::int16_t id = -1;
    std::uint8_t count = 0;
    std::int16_t damage = 0;
    Bytes nbt;

    bool empty() const {
        return id < 0;
    }
};

Slot readSlot(Reader& reader);
void writeSlot(Writer& writer, const Slot& slot);

}
