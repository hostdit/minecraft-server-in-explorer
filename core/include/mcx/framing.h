#pragma once

#include <mcx/protocol.h>

namespace mcx {

constexpr std::int32_t maxPacketSize = 2 * 1024 * 1024;

Writer packet(std::int32_t id);
Bytes frame(const Writer& body);

class Reassembler {
public:
    void feed(std::span<const std::uint8_t> data);
    std::optional<Bytes> next();

private:
    Bytes buffer;
};

}
