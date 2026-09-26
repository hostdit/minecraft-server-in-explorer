#include <mcx/world.h>

#include <algorithm>

namespace mcx {

namespace {

constexpr int worldSide = worldRadius * 2 + 1;

}

BlockState generated(Position position) {
    switch (position.y) {
    case 0: return state(7, 0);
    case 1:
    case 2: return state(3, 0);
    case 3: return state(2, 0);
    default: return 0;
    }
}

World::World(Time created) : createdAt(created), chunks(worldSide * worldSide) {
    for (Chunk& chunk : chunks)
        for (int y = 0; y < 4; y++)
            for (int z = 0; z < 16; z++)
                for (int x = 0; x < 16; x++) chunk.set(x, y, z, generated({x, y, z}));
    nonAir = chunks.size() * 16 * 16 * 4;
}

bool World::chunkInside(int chunkX, int chunkZ) {
    return chunkX >= -worldRadius && chunkX <= worldRadius && chunkZ >= -worldRadius && chunkZ <= worldRadius;
}

bool World::inside(Position position) const {
    return position.y >= 0 && position.y < worldHeight && chunkInside(floorDiv(position.x, 16), floorDiv(position.z, 16));
}

Time World::created() const {
    return createdAt;
}

BlockState World::get(Position position) const {
    std::lock_guard guard(lock);
    return inside(position) ? read(position) : 0;
}

std::optional<Block> World::block(Position position) const {
    std::lock_guard guard(lock);
    if (!inside(position)) return std::nullopt;
    BlockState value = read(position);
    if (!value) return std::nullopt;
    return describe(position, value);
}

std::vector<Block> World::blocks(int chunkX, int chunkZ) const {
    std::lock_guard guard(lock);
    std::vector<Block> found;
    if (!chunkInside(chunkX, chunkZ)) return found;
    const Chunk& chunk = chunkAt(chunkX, chunkZ);
    for (int y = 0; y < worldHeight; y++)
        for (int z = 0; z < 16; z++)
            for (int x = 0; x < 16; x++)
                if (BlockState value = chunk.get(x, y, z)) found.push_back(describe({chunkX * 16 + x, y, chunkZ * 16 + z}, value));
    return found;
}

Chunk World::chunk(int chunkX, int chunkZ) const {
    std::lock_guard guard(lock);
    return chunkInside(chunkX, chunkZ) ? chunkAt(chunkX, chunkZ) : Chunk{};
}

std::size_t World::count() const {
    std::lock_guard guard(lock);
    return nonAir;
}

std::size_t World::blockCount(int chunkX, int chunkZ) const {
    std::lock_guard guard(lock);
    if (!chunkInside(chunkX, chunkZ)) return 0;
    const Chunk& chunk = chunkAt(chunkX, chunkZ);
    return std::size_t(std::count_if(chunk.blocks.begin(), chunk.blocks.end(), [](BlockState value) { return value != 0; }));
}

std::vector<Block> World::edits() const {
    std::lock_guard guard(lock);
    std::vector<Block> found;
    for (const auto& [key, edit] : editsByPosition) {
        Position position{std::get<0>(key), std::get<1>(key), std::get<2>(key)};
        found.push_back({position, read(position), edit.modified, edit.author});
    }
    return found;
}

bool World::set(Position position, BlockState value, const std::string& author, Time when) {
    Change change;
    {
        std::lock_guard guard(lock);
        if (!inside(position)) return false;
        BlockState before = read(position);
        if (before == value) return true;
        write(position, value);
        editsByPosition[{position.x, position.y, position.z}] = {when, author};
        ChangeKind kind = !before ? ChangeKind::placed : !value ? ChangeKind::removed : ChangeKind::replaced;
        change = {kind, position, before, value, author, floorDiv(position.x, 16), floorDiv(position.z, 16)};
    }
    fire(change);
    return true;
}

bool World::clearChunk(int chunkX, int chunkZ, const std::string& author, Time when) {
    {
        std::lock_guard guard(lock);
        if (!chunkInside(chunkX, chunkZ)) return false;
        for (int y = 0; y < worldHeight; y++)
            for (int z = 0; z < 16; z++)
                for (int x = 0; x < 16; x++) {
                    Position position{chunkX * 16 + x, y, chunkZ * 16 + z};
                    if (!read(position)) continue;
                    write(position, 0);
                    editsByPosition[{position.x, position.y, position.z}] = {when, author};
                }
    }
    fire({ChangeKind::chunkCleared, {chunkX * 16, 0, chunkZ * 16}, 0, 0, author, chunkX, chunkZ});
    return true;
}

void World::restore(const std::vector<Block>& edits) {
    std::lock_guard guard(lock);
    for (const Block& edit : edits) {
        if (!inside(edit.position)) throw std::out_of_range("restored edit outside the world");
        write(edit.position, edit.state);
        editsByPosition[{edit.position.x, edit.position.y, edit.position.z}] = {edit.modified, edit.author};
    }
}

std::size_t World::onChange(std::function<void(const Change&)> listener) {
    std::lock_guard guard(lock);
    listeners[nextToken] = std::move(listener);
    return nextToken++;
}

void World::removeListener(std::size_t token) {
    std::lock_guard guard(lock);
    listeners.erase(token);
}

Chunk& World::chunkAt(int chunkX, int chunkZ) {
    return chunks[(chunkZ + worldRadius) * worldSide + chunkX + worldRadius];
}

const Chunk& World::chunkAt(int chunkX, int chunkZ) const {
    return chunks[(chunkZ + worldRadius) * worldSide + chunkX + worldRadius];
}

BlockState World::read(Position position) const {
    return chunkAt(floorDiv(position.x, 16), floorDiv(position.z, 16)).get(floorMod(position.x, 16), position.y, floorMod(position.z, 16));
}

void World::write(Position position, BlockState value) {
    BlockState before = read(position);
    nonAir += (value != 0) - (before != 0);
    chunkAt(floorDiv(position.x, 16), floorDiv(position.z, 16)).set(floorMod(position.x, 16), position.y, floorMod(position.z, 16), value);
}

Block World::describe(Position position, BlockState value) const {
    auto edit = editsByPosition.find({position.x, position.y, position.z});
    if (edit == editsByPosition.end()) return {position, value, createdAt, "generated"};
    return {position, value, edit->second.modified, edit->second.author};
}

void World::fire(const Change& change) {
    std::vector<std::function<void(const Change&)>> current;
    {
        std::lock_guard guard(lock);
        for (auto& [token, listener] : listeners) current.push_back(listener);
    }
    for (auto& listener : current) listener(change);
}

}
