#pragma once

#include <mcx/protocol.h>

namespace mcx {

std::uint32_t crc32(std::span<const std::uint8_t> data);
std::uint32_t adler32(std::span<const std::uint8_t> data);
Bytes encodePng(int width, int height, std::span<const std::uint8_t> rgba);
std::string base64(std::span<const std::uint8_t> data);
Bytes folderPixels();
std::string faviconDataUri();

}
