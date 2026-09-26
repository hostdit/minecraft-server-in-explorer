#include <mcx/item.h>

#include <tuple>

namespace mcx {

namespace {

bool hasChunk(ItemKind kind) { return kind == ItemKind::chunk; }
bool hasBlock(ItemKind kind) { return kind == ItemKind::block; }
bool hasId(ItemKind kind) { return kind == ItemKind::player || kind == ItemKind::chatMessage; }
bool hasText(ItemKind kind) { return kind == ItemKind::player || kind == ItemKind::chatMessage || kind == ItemKind::time || kind == ItemKind::weather; }

template <class T>
int compare(const T& a, const T& b) {
    return a < b ? -1 : b < a ? 1 : 0;
}

auto identity(const Item& item) {
    bool status = item.kind == ItemKind::status;
    return std::tuple(item.kind, item.x, item.y, item.z, hasId(item.kind) || status ? item.id : 0);
}

}

Bytes encodeItem(const Item& item) {
    Writer writer;
    writer.u8('M').u8('C').u8('X').u8(itemVersion).u8(std::uint8_t(item.kind));
    if (item.kind == ItemKind::status) writer.u8(std::uint8_t(item.id));
    if (hasChunk(item.kind)) writer.i32(item.x).i32(item.z);
    if (hasBlock(item.kind)) writer.i32(item.x).i32(item.y).i32(item.z).u16(item.state);
    if (hasId(item.kind)) writer.i32(std::int32_t(item.id));
    if (hasText(item.kind)) writer.string(item.text);
    return writer.data;
}

std::optional<Item> decodeItem(std::span<const std::uint8_t> data) {
    try {
        Reader reader(data);
        if (reader.u8() != 'M' || reader.u8() != 'C' || reader.u8() != 'X' || reader.u8() != itemVersion) return std::nullopt;
        std::uint8_t kind = reader.u8();
        if (kind < std::uint8_t(ItemKind::world) || kind > std::uint8_t(ItemKind::herobrine)) return std::nullopt;
        Item item{ItemKind(kind)};
        if (item.kind == ItemKind::status) item.id = reader.u8();
        if (hasChunk(item.kind)) {
            item.x = reader.i32();
            item.z = reader.i32();
        }
        if (hasBlock(item.kind)) {
            item.x = reader.i32();
            item.y = reader.i32();
            item.z = reader.i32();
            item.state = reader.u16();
        }
        if (hasId(item.kind)) item.id = std::uint32_t(reader.i32());
        if (hasText(item.kind)) item.text = reader.string();
        if (reader.remaining()) return std::nullopt;
        if (item.kind == ItemKind::status && item.id > 3) return std::nullopt;
        if (hasBlock(item.kind) && (item.y < 0 || item.y > 255 || blockId(item.state) > 197)) return std::nullopt;
        if (item.text.size() > maxItemText) return std::nullopt;
        return item;
    } catch (const ProtocolError&) {
        return std::nullopt;
    }
}

int canonicalOrder(const Item& a, const Item& b) {
    return compare(identity(a), identity(b));
}

int fullOrder(const Item& a, const Item& b) {
    if (int order = canonicalOrder(a, b)) return order;
    return compare(std::tie(a.state, a.id, a.text), std::tie(b.state, b.id, b.text));
}

}
