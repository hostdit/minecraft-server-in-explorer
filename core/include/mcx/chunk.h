#pragma once

#include <mcx/framing.h>

#include <array>

namespace mcx {

using BlockState = std::uint16_t;

constexpr std::uint8_t plainsBiome = 1;

constexpr BlockState state(int id, int meta) {
    return BlockState((id << 4) | meta);
}

constexpr int blockId(BlockState value) {
    return value >> 4;
}

constexpr int blockMeta(BlockState value) {
    return value & 15;
}

struct Chunk {
    std::array<BlockState, 16 * 16 * 256> blocks{};

    static int index(int x, int y, int z) {
        return (y << 8) | (z << 4) | x;
    }

    BlockState get(int x, int y, int z) const {
        return blocks[index(x, y, z)];
    }

    void set(int x, int y, int z, BlockState value) {
        blocks[index(x, y, z)] = value;
    }
};

struct EncodedChunk {
    std::uint16_t bitmask;
    Bytes data;
};

EncodedChunk encodeChunk(const Chunk& chunk);
Writer chunkPacket(int chunkX, int chunkZ, const Chunk& chunk);

}
