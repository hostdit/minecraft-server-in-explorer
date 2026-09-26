#include <mcx/slot.h>

namespace mcx {

namespace {

constexpr int maxDepth = 512;
constexpr std::uint8_t lastTag = 11;

std::int32_t length(Reader& reader, std::size_t size) {
    std::int32_t value = reader.i32();
    if (value < 0 || std::size_t(value) * size > reader.remaining()) throw ProtocolError("nbt length out of range");
    return value;
}

void copyBytes(Reader& reader, Writer& writer, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) writer.u8(reader.u8());
}

void copyName(Reader& reader, Writer& writer) {
    std::uint16_t size = reader.u16();
    writer.u16(size);
    copyBytes(reader, writer, size);
}

void copyPayload(Reader& reader, Writer& writer, std::uint8_t type, int depth) {
    if (depth > maxDepth) throw ProtocolError("nbt nested too deeply");
    switch (type) {
    case 1: writer.u8(reader.u8()); return;
    case 2: writer.i16(reader.i16()); return;
    case 3:
    case 5: writer.i32(reader.i32()); return;
    case 4:
    case 6: writer.i64(reader.i64()); return;
    case 7: {
        std::int32_t size = length(reader, 1);
        writer.i32(size);
        return copyBytes(reader, writer, std::size_t(size));
    }
    case 8: return copyName(reader, writer);
    case 9: {
        std::uint8_t element = reader.u8();
        std::int32_t size = reader.i32();
        if (element > lastTag || size < 0 || (element == 0 && size > 0)) throw ProtocolError("bad nbt list");
        writer.u8(element).i32(size);
        for (std::int32_t i = 0; i < size; i++) copyPayload(reader, writer, element, depth + 1);
        return;
    }
    case 10:
        while (true) {
            std::uint8_t inner = reader.u8();
            writer.u8(inner);
            if (!inner) return;
            if (inner > lastTag) throw ProtocolError("unknown nbt tag");
            copyName(reader, writer);
            copyPayload(reader, writer, inner, depth + 1);
        }
    case 11: {
        std::int32_t size = length(reader, 4);
        writer.i32(size);
        return copyBytes(reader, writer, std::size_t(size) * 4);
    }
    default: throw ProtocolError("unknown nbt tag");
    }
}

}

Slot readSlot(Reader& reader) {
    Slot slot;
    slot.id = reader.i16();
    if (slot.empty()) return slot;
    slot.count = reader.u8();
    slot.damage = reader.i16();
    std::uint8_t type = reader.u8();
    if (!type) return slot;
    if (type > lastTag) throw ProtocolError("unknown nbt tag");
    Writer nbt;
    nbt.u8(type);
    copyName(reader, nbt);
    copyPayload(reader, nbt, type, 0);
    slot.nbt = std::move(nbt.data);
    return slot;
}

void writeSlot(Writer& writer, const Slot& slot) {
    writer.i16(slot.id);
    if (slot.empty()) return;
    writer.u8(slot.count).i16(slot.damage);
    if (slot.nbt.empty()) writer.u8(0);
    else writer.raw(slot.nbt);
}

}
