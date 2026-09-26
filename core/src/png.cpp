#include <mcx/png.h>

#include <array>

namespace mcx {

namespace {

constexpr std::size_t maxStoredBlock = 65535;

void chunk(Bytes& png, const char* type, std::span<const std::uint8_t> data) {
    Writer header;
    header.i32(std::int32_t(data.size()));
    png.insert(png.end(), header.data.begin(), header.data.end());
    std::size_t start = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    Writer crc;
    crc.i32(std::int32_t(crc32(std::span(png).subspan(start))));
    png.insert(png.end(), crc.data.begin(), crc.data.end());
}

Bytes storedZlib(std::span<const std::uint8_t> raw) {
    Bytes out{0x78, 0x01};
    std::size_t offset = 0;
    do {
        std::size_t length = std::min(maxStoredBlock, raw.size() - offset);
        bool last = offset + length == raw.size();
        out.insert(out.end(), {std::uint8_t(last), std::uint8_t(length), std::uint8_t(length >> 8), std::uint8_t(~length), std::uint8_t(~length >> 8)});
        out.insert(out.end(), raw.begin() + offset, raw.begin() + offset + length);
        offset += length;
    } while (offset < raw.size());
    Writer adler;
    adler.i32(std::int32_t(adler32(raw)));
    out.insert(out.end(), adler.data.begin(), adler.data.end());
    return out;
}

void fill(Bytes& pixels, int left, int top, int right, int bottom, std::array<std::uint8_t, 4> colour) {
    for (int y = top; y < bottom; y++)
        for (int x = left; x < right; x++) std::copy(colour.begin(), colour.end(), pixels.begin() + (y * 64 + x) * 4);
}

}

std::uint32_t crc32(std::span<const std::uint8_t> data) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (std::uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1)));
    }
    return ~crc;
}

std::uint32_t adler32(std::span<const std::uint8_t> data) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::uint8_t byte : data) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

Bytes encodePng(int width, int height, std::span<const std::uint8_t> rgba) {
    if (width <= 0 || height <= 0 || rgba.size() != std::size_t(width) * height * 4) throw std::invalid_argument("pixel count does not match the image size");
    Bytes raw;
    raw.reserve(std::size_t(height) * (1 + width * 4));
    for (int y = 0; y < height; y++) {
        raw.push_back(0);
        auto row = rgba.subspan(std::size_t(y) * width * 4, std::size_t(width) * 4);
        raw.insert(raw.end(), row.begin(), row.end());
    }
    Writer header;
    header.i32(width).i32(height).u8(8).u8(6).u8(0).u8(0).u8(0);
    Bytes png{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    chunk(png, "IHDR", header.data);
    chunk(png, "IDAT", storedZlib(raw));
    chunk(png, "IEND", {});
    return png;
}

std::string base64(std::span<const std::uint8_t> data) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < data.size(); i += 3) {
        std::uint32_t group = data[i] << 16;
        if (i + 1 < data.size()) group |= data[i + 1] << 8;
        if (i + 2 < data.size()) group |= data[i + 2];
        out += alphabet[(group >> 18) & 63];
        out += alphabet[(group >> 12) & 63];
        out += i + 1 < data.size() ? alphabet[(group >> 6) & 63] : '=';
        out += i + 2 < data.size() ? alphabet[group & 63] : '=';
    }
    return out;
}

Bytes folderPixels() {
    Bytes pixels(64 * 64 * 4, 0);
    const std::array<std::uint8_t, 4> edge{0xC2, 0x8E, 0x1E, 0xFF};
    const std::array<std::uint8_t, 4> back{0xE8, 0xB0, 0x3A, 0xFF};
    const std::array<std::uint8_t, 4> front{0xFF, 0xD5, 0x5C, 0xFF};
    fill(pixels, 4, 8, 28, 18, edge);
    fill(pixels, 5, 9, 27, 18, back);
    fill(pixels, 4, 14, 60, 56, edge);
    fill(pixels, 5, 15, 59, 55, back);
    fill(pixels, 4, 22, 60, 56, edge);
    fill(pixels, 5, 23, 59, 55, front);
    return pixels;
}

std::string faviconDataUri() {
    return "data:image/png;base64," + base64(encodePng(64, 64, folderPixels()));
}

}
