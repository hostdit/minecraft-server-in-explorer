#include <mcx/chunk.h>

#include <algorithm>
#include <bit>

namespace mcx {

namespace {

constexpr int sectionSize = 16 * 16 * 16;

bool sectionHasBlocks(const Chunk& chunk, int section) {
    auto start = chunk.blocks.begin() + section * sectionSize;
    return std::any_of(start, start + sectionSize, [](BlockState value) { return value != 0; });
}

}

EncodedChunk encodeChunk(const Chunk& chunk) {
    std::uint16_t bitmask = 0;
    for (int section = 0; section < 16; section++)
        if (sectionHasBlocks(chunk, section)) bitmask |= std::uint16_t(1 << section);
    if (!bitmask) bitmask = 1;

    int sections = std::popcount(bitmask);
    Bytes data;
    data.reserve(sections * sectionSize * 3 + 256);
    for (int section = 0; section < 16; section++) {
        if (!(bitmask & (1 << section))) continue;
        auto start = chunk.blocks.begin() + section * sectionSize;
        for (auto value = start; value != start + sectionSize; value++) {
            data.push_back(std::uint8_t(*value));
            data.push_back(std::uint8_t(*value >> 8));
        }
    }
    data.insert(data.end(), sections * sectionSize / 2, 0x00);
    data.insert(data.end(), sections * sectionSize / 2, 0xFF);
    data.insert(data.end(), 256, plainsBiome);
    return {bitmask, std::move(data)};
}

Writer chunkPacket(int chunkX, int chunkZ, const Chunk& chunk) {
    EncodedChunk encoded = encodeChunk(chunk);
    Writer writer = packet(0x21);
    writer.i32(chunkX).i32(chunkZ).boolean(true).u16(encoded.bitmask).varInt(std::int32_t(encoded.data.size())).raw(encoded.data);
    return writer;
}

}
