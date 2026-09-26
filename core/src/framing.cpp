#include <mcx/framing.h>

namespace mcx {

Writer packet(std::int32_t id) {
    Writer writer;
    writer.varInt(id);
    return writer;
}

Bytes frame(const Writer& body) {
    Writer framed;
    framed.varInt(std::int32_t(body.data.size()));
    framed.raw(body.data);
    return framed.data;
}

void Reassembler::feed(std::span<const std::uint8_t> data) {
    buffer.insert(buffer.end(), data.begin(), data.end());
}

std::optional<Bytes> Reassembler::next() {
    auto length = peekVarInt(buffer);
    if (!length) return std::nullopt;
    if (length->value <= 0 || length->value > maxPacketSize) throw ProtocolError("packet length out of range: " + std::to_string(length->value));
    if (buffer.size() - length->size < std::size_t(length->value)) return std::nullopt;
    auto start = buffer.begin() + length->size;
    Bytes body(start, start + length->value);
    buffer.erase(buffer.begin(), start + length->value);
    return body;
}

}
