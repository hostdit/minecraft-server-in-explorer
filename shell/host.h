#pragma once

#include "pidl.h"

#include <mcx/autosave.h>
#include <mcx/server.h>

#include <deque>
#include <map>
#include <mutex>

namespace mcx {

enum class Status : std::uint32_t { running, stopped, inUse, elsewhere };

constexpr int serverPort = 25565;
constexpr char explorerName[] = "File Explorer";

class Host {
public:
    static Host& get();
    static bool everStarted();

    bool active() const;
    void setRoot(PCIDLIST_ABSOLUTE pidl);
    void open();
    Status status();
    bool hasWorld();
    void setRunning(bool on);
    World& world();
    Server& server();
    std::optional<PlayerInfo> player(std::int32_t entityId);
    void setTime(std::int64_t ticks);
    void setWeather(Weather weather);

private:
    Host();

    bool explorer;
    std::mutex lock;
    Pidl root;
    Status current = Status::stopped;
    bool opened = false;
    IUnknown* processReference = nullptr;
    std::unique_ptr<World> worldState;
    std::unique_ptr<Autosave> autosave;
    std::unique_ptr<Server> serverState;

    struct Seen {
        std::string name;
        double x;
        double y;
        double z;
        std::chrono::steady_clock::time_point notified;
    };
    std::mutex playersLock;
    std::map<std::int32_t, Seen> seen;
    std::mutex chatLock;
    std::deque<ChatLine> shownChat;

    void load();
    void notify(LONG event, std::initializer_list<Item> path);
    void rename(std::initializer_list<Item> from, std::initializer_list<Item> to);
    void changeStatus(Status next);
    void worldChanged(const Change& change);
    void syncPlayers();
    void chatAdded(const ChatLine& line);
};

Item statusItem(Status status);
Item playerItem(const PlayerInfo& player);
Item chatItem(const ChatLine& line);
Item blockItem(Position position, BlockState state);
Item chunkItem(int chunkX, int chunkZ);
Item timeItem(std::int64_t ticks);
Item weatherItem(Weather weather);

}
