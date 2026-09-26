#include <mcx/autosave.h>

#include <condition_variable>
#include <format>
#include <thread>

namespace mcx {

struct Autosave::Impl {
    World& world;
    std::filesystem::path path;
    std::chrono::milliseconds delay;
    std::function<void(const std::string&)> log;
    std::mutex lock;
    std::condition_variable wake;
    bool pending = false;
    bool stopping = false;
    std::uint64_t generation = 0;
    std::size_t token = 0;
    std::thread worker;

    Impl(World& world, std::filesystem::path path, std::chrono::milliseconds delay, std::function<void(const std::string&)> log)
        : world(world), path(std::move(path)), delay(delay), log(std::move(log)) {}

    void changed() {
        {
            std::lock_guard guard(lock);
            pending = true;
            generation++;
        }
        wake.notify_all();
    }

    void run() {
        std::unique_lock guard(lock);
        while (!stopping) {
            wake.wait(guard, [&] { return pending || stopping; });
            if (stopping) break;
            auto seen = generation;
            if (wake.wait_for(guard, delay, [&] { return generation != seen || stopping; })) continue;
            writeUnlocked(guard);
        }
        if (pending) writeUnlocked(guard);
    }

    void writeUnlocked(std::unique_lock<std::mutex>& guard) {
        pending = false;
        guard.unlock();
        try {
            Bytes data = encodeSave(world);
            writeFileAtomically(path, data);
            log(std::format("saved {} edits to {}", world.edits().size(), path.string()));
        } catch (const std::exception& error) {
            log(std::format("saving {} failed: {}", path.string(), error.what()));
        }
        guard.lock();
    }
};

std::unique_ptr<World> loadWorld(const std::filesystem::path& path, Time now, const std::function<void(const std::string&)>& log) {
    auto data = readFile(path);
    if (!data) {
        log(std::format("no save at {}, starting a new world", path.string()));
        return std::make_unique<World>(now);
    }
    auto saved = decodeSave(*data);
    if (!saved) {
        auto bad = path;
        bad += ".bad";
        std::filesystem::rename(path, bad);
        log(std::format("{} could not be read, moved it to {} and started a new world", path.string(), bad.string()));
        return std::make_unique<World>(now);
    }
    auto world = std::make_unique<World>(saved->created);
    world->restore(saved->edits);
    log(std::format("loaded {} edits from {}", saved->edits.size(), path.string()));
    return world;
}

Autosave::Autosave(World& world, std::filesystem::path path, std::chrono::milliseconds delay, std::function<void(const std::string&)> log)
    : impl(std::make_shared<Impl>(world, std::move(path), delay, std::move(log))) {
    std::weak_ptr<Impl> weak = impl;
    impl->token = world.onChange([weak](const Change&) {
        if (auto self = weak.lock()) self->changed();
    });
    impl->worker = std::thread([self = impl] { self->run(); });
}

Autosave::~Autosave() {
    impl->world.removeListener(impl->token);
    {
        std::lock_guard guard(impl->lock);
        impl->stopping = true;
    }
    impl->wake.notify_all();
    impl->worker.join();
}

void Autosave::flush() {
    std::unique_lock guard(impl->lock);
    if (impl->pending) impl->writeUnlocked(guard);
}

}
