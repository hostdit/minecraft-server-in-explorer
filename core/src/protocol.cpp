#include <mcx/protocol.h>

#include <bit>

namespace mcx {

int floorDiv(int value, int divisor) {
    int quotient = value / divisor;
    return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

int floorMod(int value, int divisor) {
    return value - floorDiv(value, divisor) * divisor;
}

std::uint64_t packPosition(Position position) {
    return (std::uint64_t(position.x & 0x3FFFFFF) << 38) | (std::uint64_t(position.y & 0xFFF) << 26) | std::uint64_t(position.z & 0x3FFFFFF);
}

Position unpackPosition(std::uint64_t packed) {
    auto signedField = [](std::uint64_t field, int bits) {
        int value = int(field);
        return value >= (1 << (bits - 1)) ? value - (1 << bits) : value;
    };
    return {signedField(packed >> 38, 26), signedField((packed >> 26) & 0xFFF, 12), signedField(packed & 0x3FFFFFF, 26)};
}

std::optional<Peeked> peekVarInt(std::span<const std::uint8_t> data) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 5; i++) {
        if (i >= data.size()) return std::nullopt;
        value |= std::uint32_t(data[i] & 0x7F) << (7 * i);
        if (!(data[i] & 0x80)) return Peeked{std::int32_t(value), i + 1};
    }
    throw ProtocolError("varint longer than five bytes");
}

Writer& Writer::varInt(std::int32_t value) {
    auto rest = std::uint32_t(value);
    do {
        std::uint8_t part = rest & 0x7F;
        rest >>= 7;
        data.push_back(rest ? part | 0x80 : part);
    } while (rest);
    return *this;
}

Writer& Writer::u8(std::uint8_t value) {
    data.push_back(value);
    return *this;
}

Writer& Writer::boolean(bool value) {
    return u8(value ? 1 : 0);
}

template <class T>
static Writer& bigEndian(Writer& writer, T value) {
    auto bits = std::make_unsigned_t<T>(value);
    for (int shift = int(sizeof(T) - 1) * 8; shift >= 0; shift -= 8) writer.data.push_back(std::uint8_t(bits >> shift));
    return writer;
}

Writer& Writer::i16(std::int16_t value) { return bigEndian(*this, value); }
Writer& Writer::u16(std::uint16_t value) { return bigEndian(*this, value); }
Writer& Writer::i32(std::int32_t value) { return bigEndian(*this, value); }
Writer& Writer::i64(std::int64_t value) { return bigEndian(*this, value); }
Writer& Writer::f32(float value) { return i32(std::bit_cast<std::int32_t>(value)); }
Writer& Writer::f64(double value) { return i64(std::bit_cast<std::int64_t>(value)); }

Writer& Writer::string(std::string_view value) {
    varInt(std::int32_t(value.size()));
    data.insert(data.end(), value.begin(), value.end());
    return *this;
}

Writer& Writer::raw(std::span<const std::uint8_t> value) {
    data.insert(data.end(), value.begin(), value.end());
    return *this;
}

Writer& Writer::position(Position value) {
    return bigEndian(*this, packPosition(value));
}

Reader::Reader(std::span<const std::uint8_t> data) : data(data) {}

std::int32_t Reader::varInt() {
    auto peeked = peekVarInt(data.subspan(offset));
    if (!peeked) throw ProtocolError("truncated varint");
    offset += peeked->size;
    return peeked->value;
}

std::uint8_t Reader::u8() {
    return std::uint8_t(bigEndian(1));
}

bool Reader::boolean() {
    return u8() != 0;
}

std::int16_t Reader::i16() { return std::int16_t(bigEndian(2)); }
std::uint16_t Reader::u16() { return std::uint16_t(bigEndian(2)); }
std::int32_t Reader::i32() { return std::int32_t(bigEndian(4)); }
std::int64_t Reader::i64() { return std::int64_t(bigEndian(8)); }
float Reader::f32() { return std::bit_cast<float>(std::uint32_t(bigEndian(4))); }
double Reader::f64() { return std::bit_cast<double>(bigEndian(8)); }

std::string Reader::string() {
    std::int32_t size = varInt();
    if (size < 0 || std::size_t(size) > remaining()) throw ProtocolError("truncated string");
    std::string value(data.begin() + offset, data.begin() + offset + size);
    offset += size;
    return value;
}

Position Reader::position() {
    return unpackPosition(bigEndian(8));
}

std::size_t Reader::remaining() const {
    return data.size() - offset;
}

std::uint64_t Reader::bigEndian(std::size_t size) {
    if (remaining() < size) throw ProtocolError("truncated number");
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < size; i++) value = (value << 8) | data[offset + i];
    offset += size;
    return value;
}

}
