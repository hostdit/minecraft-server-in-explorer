#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mcx {

using Bytes = std::vector<std::uint8_t>;

struct ProtocolError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Position {
    int x;
    int y;
    int z;
    bool operator==(const Position&) const = default;
};

struct Peeked {
    std::int32_t value;
    std::size_t size;
};

int floorDiv(int value, int divisor);
int floorMod(int value, int divisor);
std::uint64_t packPosition(Position position);
Position unpackPosition(std::uint64_t packed);
std::optional<Peeked> peekVarInt(std::span<const std::uint8_t> data);

class Writer {
public:
    Bytes data;

    Writer& varInt(std::int32_t value);
    Writer& u8(std::uint8_t value);
    Writer& boolean(bool value);
    Writer& i16(std::int16_t value);
    Writer& u16(std::uint16_t value);
    Writer& i32(std::int32_t value);
    Writer& i64(std::int64_t value);
    Writer& f32(float value);
    Writer& f64(double value);
    Writer& string(std::string_view value);
    Writer& raw(std::span<const std::uint8_t> value);
    Writer& position(Position value);
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data);

    std::int32_t varInt();
    std::uint8_t u8();
    bool boolean();
    std::int16_t i16();
    std::uint16_t u16();
    std::int32_t i32();
    std::int64_t i64();
    float f32();
    double f64();
    std::string string();
    Position position();
    std::size_t remaining() const;

private:
    std::span<const std::uint8_t> data;
    std::size_t offset = 0;

    std::uint64_t bigEndian(std::size_t size);
};

}
