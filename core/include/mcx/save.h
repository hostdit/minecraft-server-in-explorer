#pragma once

#include <mcx/world.h>

#include <filesystem>

namespace mcx {

constexpr std::uint8_t saveVersion = 1;

struct Saved {
    Time created;
    std::vector<Block> edits;
};

Bytes encodeSave(const World& world);
Bytes encodeSaved(const Saved& saved);
std::optional<Saved> decodeSave(std::span<const std::uint8_t> data);
void writeFileAtomically(const std::filesystem::path& path, std::span<const std::uint8_t> data);
std::optional<Bytes> readFile(const std::filesystem::path& path);

}
