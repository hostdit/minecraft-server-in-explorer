#pragma once

#include <mcx/chunk.h>

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <tuple>

namespace mcx {

using Time = std::chrono::system_clock::time_point;

constexpr int worldRadius = 3;
constexpr int worldHeight = 256;

struct Block {
    Position position;
    BlockState state;
    Time modified;
    std::string author;
};

enum class ChangeKind { placed, removed, replaced, chunkCleared };

struct Change {
    ChangeKind kind;
    Position position;
    BlockState before;
    BlockState after;
    std::string author;
    int chunkX;
    int chunkZ;
};

BlockState generated(Position position);

class World {
public:
    explicit World(Time created);

    static bool chunkInside(int chunkX, int chunkZ);
    bool inside(Position position) const;
    Time created() const;
    BlockState get(Position position) const;
    std::optional<Block> block(Position position) const;
    std::vector<Block> blocks(int chunkX, int chunkZ) const;
    Chunk chunk(int chunkX, int chunkZ) const;
    std::size_t count() const;
    std::size_t blockCount(int chunkX, int chunkZ) const;
    std::vector<Block> edits() const;

    bool set(Position position, BlockState value, const std::string& author, Time when);
    bool clearChunk(int chunkX, int chunkZ, const std::string& author, Time when);
    void restore(const std::vector<Block>& edits);
    std::size_t onChange(std::function<void(const Change&)> listener);
    void removeListener(std::size_t token);

private:
    struct Edit {
        Time modified;
        std::string author;
    };

    using Key = std::tuple<int, int, int>;

    Time createdAt;
    mutable std::mutex lock;
    std::vector<Chunk> chunks;
    std::map<Key, Edit> editsByPosition;
    std::size_t nonAir = 0;
    std::map<std::size_t, std::function<void(const Change&)>> listeners;
    std::size_t nextToken = 0;

    Chunk& chunkAt(int chunkX, int chunkZ);
    const Chunk& chunkAt(int chunkX, int chunkZ) const;
    BlockState read(Position position) const;
    void write(Position position, BlockState value);
    Block describe(Position position, BlockState value) const;
    void fire(const Change& change);
};

}
