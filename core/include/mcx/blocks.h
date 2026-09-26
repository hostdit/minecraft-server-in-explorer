#pragma once

#include <mcx/chunk.h>

namespace mcx {

constexpr int maxBlockId = 197;

struct NamedBlock {
    BlockState state;
    Position position;
};

std::string typeName(BlockState value);
std::string displayName(BlockState value);
std::optional<BlockState> parseType(std::string_view text);
std::string fileName(BlockState value, Position position);
std::optional<NamedBlock> parseFileName(std::string_view text);
std::optional<BlockState> blockForItem(int itemId, int damage);
std::uint32_t colourOf(BlockState value);

}
