#pragma once

#include <mcx/chunk.h>

namespace mcx {

constexpr std::uint8_t itemVersion = 2;
constexpr std::size_t maxItemText = 256;

enum class ItemKind : std::uint8_t {
    world = 1,
    players,
    chat,
    status,
    chunk,
    block,
    player,
    chatInput,
    chatMessage,
    time,
    weather,
    herobrine,
};

struct Item {
    ItemKind kind;
    int x = 0;
    int y = 0;
    int z = 0;
    BlockState state = 0;
    std::uint32_t id = 0;
    std::string text;
    bool operator==(const Item&) const = default;
};

Bytes encodeItem(const Item& item);
std::optional<Item> decodeItem(std::span<const std::uint8_t> data);
int canonicalOrder(const Item& a, const Item& b);
int fullOrder(const Item& a, const Item& b);

}
