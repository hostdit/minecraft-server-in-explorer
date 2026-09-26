#pragma once

#include <mcx/save.h>

#include <memory>

namespace mcx {

std::unique_ptr<World> loadWorld(const std::filesystem::path& path, Time now, const std::function<void(const std::string&)>& log);

class Autosave {
public:
    Autosave(World& world, std::filesystem::path path, std::chrono::milliseconds delay, std::function<void(const std::string&)> log);
    ~Autosave();

    void flush();

private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}
