#include <mcx/save.h>

#include <fstream>
#include <iterator>

namespace mcx {

namespace {

std::int64_t seconds(Time time) {
    return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

Time fromSeconds(std::int64_t value) {
    return Time(std::chrono::seconds(value));
}

bool valid(const Block& edit) {
    return edit.position.y >= 0 && edit.position.y < worldHeight && World::chunkInside(floorDiv(edit.position.x, 16), floorDiv(edit.position.z, 16)) && blockId(edit.state) <= 197;
}

}

Bytes encodeSave(const World& world) {
    return encodeSaved({world.created(), world.edits()});
}

Bytes encodeSaved(const Saved& saved) {
    Writer writer;
    writer.u8('M').u8('C').u8('X').u8('W').u8(saveVersion).i64(seconds(saved.created)).varInt(std::int32_t(saved.edits.size()));
    for (const Block& edit : saved.edits)
        writer.i32(edit.position.x).i16(std::int16_t(edit.position.y)).i32(edit.position.z).u16(edit.state).i64(seconds(edit.modified)).string(edit.author);
    return writer.data;
}

std::optional<Saved> decodeSave(std::span<const std::uint8_t> data) {
    try {
        Reader reader(data);
        if (reader.u8() != 'M' || reader.u8() != 'C' || reader.u8() != 'X' || reader.u8() != 'W' || reader.u8() != saveVersion) return std::nullopt;
        Saved saved{fromSeconds(reader.i64()), {}};
        std::int32_t count = reader.varInt();
        if (count < 0) return std::nullopt;
        for (std::int32_t i = 0; i < count; i++) {
            Block edit;
            edit.position.x = reader.i32();
            edit.position.y = reader.i16();
            edit.position.z = reader.i32();
            edit.state = reader.u16();
            edit.modified = fromSeconds(reader.i64());
            edit.author = reader.string();
            if (!valid(edit)) return std::nullopt;
            saved.edits.push_back(std::move(edit));
        }
        if (reader.remaining()) return std::nullopt;
        return saved;
    } catch (const ProtocolError&) {
        return std::nullopt;
    }
}

void writeFileAtomically(const std::filesystem::path& path, std::span<const std::uint8_t> data) {
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        if (!out) throw std::runtime_error("could not write " + temporary.string());
    }
    std::filesystem::rename(temporary, path);
}

std::optional<Bytes> readFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return std::nullopt;
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("could not read " + path.string());
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}
