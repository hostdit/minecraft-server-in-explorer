#include "host.h"

#include "toasts.h"

#include <mcx/net.h>

#include <algorithm>
#include <thread>

namespace mcx {

namespace {

std::atomic<bool> started = false;
constexpr auto playerRefresh = std::chrono::seconds(1);
const wchar_t advancedKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";

bool hiddenItemsShown() {
    DWORD value = 0;
    DWORD size = sizeof value;
    RegGetValueW(HKEY_CURRENT_USER, advancedKey, L"Hidden", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return value == 1;
}

}

Item statusItem(Status status) {
    return {ItemKind::status, 0, 0, 0, 0, std::uint32_t(status)};
}

Item playerItem(const PlayerInfo& player) {
    return {ItemKind::player, 0, 0, 0, 0, std::uint32_t(player.entityId), player.name};
}

Item chatItem(const ChatLine& line) {
    std::string text = line.text.size() > maxItemText ? line.text.substr(0, maxItemText) : line.text;
    return {ItemKind::chatMessage, 0, 0, 0, 0, line.id, text};
}

Item blockItem(Position position, BlockState state) {
    return {ItemKind::block, position.x, position.y, position.z, state};
}

Item chunkItem(int chunkX, int chunkZ) {
    return {ItemKind::chunk, chunkX, 0, chunkZ};
}

Item timeItem(std::int64_t ticks) {
    return {ItemKind::time, 0, 0, 0, 0, 0, timeName(ticks)};
}

Item weatherItem(Weather weather) {
    return {ItemKind::weather, 0, 0, 0, 0, 0, weatherName(weather)};
}

Host& Host::get() {
    static Host* host = new Host();
    return *host;
}

bool Host::everStarted() {
    return started;
}

Host::Host() : explorer(hostIsExplorer()) {
    if (!explorer) return;
    std::thread([this] {
        HKEY settings = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, advancedKey, 0, KEY_NOTIFY | KEY_QUERY_VALUE, &settings)) settings = nullptr;
        HANDLE changed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        while (true) {
            if (settings) RegNotifyChangeKeyValue(settings, FALSE, REG_NOTIFY_CHANGE_LAST_SET, changed, TRUE);
            WaitForSingleObject(changed, DWORD(std::chrono::milliseconds(playerRefresh).count()));
            guard("player refresh", [&] {
                syncPlayers();
                if (hasWorld()) serverState->setHerobrine(hiddenItemsShown());
                return S_OK;
            });
        }
    }).detach();
}

bool Host::active() const {
    return explorer;
}

void Host::setRoot(PCIDLIST_ABSOLUTE pidl) {
    std::lock_guard guard(lock);
    if (root) return;
    root.reset(ILCloneFull(pidl));
    if (!root) throw std::bad_alloc();
}

void Host::open() {
    if (!explorer) return;
    {
        std::lock_guard guard(lock);
        if (opened) return;
        opened = true;
    }
    setRunning(true);
}

Status Host::status() {
    if (!explorer) return Status::elsewhere;
    std::lock_guard guard(lock);
    return current;
}

bool Host::hasWorld() {
    std::lock_guard guard(lock);
    return worldState != nullptr;
}

void Host::load() {
    auto path = dataDir() / L"world.bin";
    auto loaded = loadWorld(path, std::chrono::system_clock::now(), log);
    ServerOptions options;
    options.port = serverPort;
    options.host = explorerName;
    options.log = log;
    auto server = std::make_unique<Server>(*loaded, options);
    loaded->onChange([this](const Change& change) { worldChanged(change); });
    server->onPlayers([this] { syncPlayers(); });
    server->onChat([this](const ChatLine& line) { chatAdded(line); });
    std::lock_guard guard(lock);
    worldState = std::move(loaded);
    serverState = std::move(server);
}

void Host::setRunning(bool on) {
    if (!explorer) return;
    if (on) {
        if (!hasWorld()) load();
        if (serverState->start() == StartResult::portInUse) {
            if (!autosave) {
                std::lock_guard guard(lock);
                serverState.reset();
                worldState.reset();
            }
            return changeStatus(Status::inUse);
        }
        started = true;
        if (!autosave) autosave = std::make_unique<Autosave>(*worldState, dataDir() / L"world.bin", std::chrono::seconds(1), log);
        {
            std::lock_guard guard(lock);
            if (!processReference && FAILED(SHGetInstanceExplorer(&processReference))) processReference = nullptr;
        }
        return changeStatus(Status::running);
    }
    serverState->stop("The server was deleted");
    {
        std::lock_guard guard(lock);
        if (processReference) processReference->Release();
        processReference = nullptr;
    }
    Toasts::get().hide();
    changeStatus(Status::stopped);
}

World& Host::world() {
    if (!worldState) throw std::logic_error("the world only exists inside explorer.exe");
    return *worldState;
}

Server& Host::server() {
    if (!serverState) throw std::logic_error("the server only exists inside explorer.exe");
    return *serverState;
}

std::optional<PlayerInfo> Host::player(std::int32_t entityId) {
    for (const PlayerInfo& player : server().players())
        if (player.entityId == entityId) return player;
    return std::nullopt;
}

void Host::setTime(std::int64_t ticks) {
    Item before = timeItem(server().time());
    server().setTime(ticks);
    server().say(std::format("[{}] set the time to {}", explorerName, timeName(ticks)));
    rename({before}, {timeItem(ticks)});
}

void Host::setWeather(Weather weather) {
    Item before = weatherItem(server().weather());
    server().setWeather(weather);
    server().say(std::format("[{}] set the weather to {}", explorerName, weatherName(weather)));
    rename({before}, {weatherItem(weather)});
}

void Host::notify(LONG event, std::initializer_list<Item> path) {
    std::lock_guard guard(lock);
    if (!root) return;
    Pidl full = absolute(reinterpret_cast<PCIDLIST_ABSOLUTE>(root.get()), path);
    SHChangeNotify(event, SHCNF_IDLIST, full.get(), nullptr);
}

void Host::rename(std::initializer_list<Item> from, std::initializer_list<Item> to) {
    std::lock_guard guard(lock);
    if (!root) return;
    Pidl before = absolute(reinterpret_cast<PCIDLIST_ABSOLUTE>(root.get()), from);
    Pidl after = absolute(reinterpret_cast<PCIDLIST_ABSOLUTE>(root.get()), to);
    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_IDLIST, before.get(), after.get());
}

void Host::changeStatus(Status next) {
    Status previous;
    {
        std::lock_guard guard(lock);
        previous = current;
        current = next;
    }
    if (previous == next) return notify(SHCNE_UPDATEITEM, {statusItem(next)});
    notify(SHCNE_DELETE, {statusItem(previous)});
    notify(SHCNE_CREATE, {statusItem(next)});
}

void Host::worldChanged(const Change& change) {
    Item world{ItemKind::world};
    Item chunk = chunkItem(change.chunkX, change.chunkZ);
    switch (change.kind) {
    case ChangeKind::placed:
        if (worldState->blockCount(change.chunkX, change.chunkZ) == 1) notify(SHCNE_MKDIR, {world, chunk});
        return notify(SHCNE_CREATE, {world, chunk, blockItem(change.position, change.after)});
    case ChangeKind::removed:
        notify(SHCNE_DELETE, {world, chunk, blockItem(change.position, change.before)});
        if (!worldState->blockCount(change.chunkX, change.chunkZ)) notify(SHCNE_RMDIR, {world, chunk});
        return;
    case ChangeKind::replaced:
        return rename({world, chunk, blockItem(change.position, change.before)}, {world, chunk, blockItem(change.position, change.after)});
    case ChangeKind::chunkCleared:
        return notify(SHCNE_RMDIR, {world, chunk});
    }
}

void Host::syncPlayers() {
    Item folder{ItemKind::players};
    auto now = std::chrono::steady_clock::now();
    if (!hasWorld()) return;
    auto players = serverState->players();
    std::vector<std::pair<LONG, Item>> events;
    std::vector<std::pair<Item, Item>> renames;
    {
        std::lock_guard guard(playersLock);
        for (auto entry = seen.begin(); entry != seen.end();) {
            bool present = std::any_of(players.begin(), players.end(), [&](const PlayerInfo& player) { return player.entityId == entry->first; });
            if (present) {
                ++entry;
                continue;
            }
            events.push_back({SHCNE_DELETE, playerItem({entry->first, entry->second.name, 0, 0, 0})});
            entry = seen.erase(entry);
        }
        for (const PlayerInfo& player : players) {
            auto found = seen.find(player.entityId);
            if (found == seen.end()) {
                seen[player.entityId] = {player.name, player.x, player.y, player.z, now};
                events.push_back({SHCNE_CREATE, playerItem(player)});
                Toasts::get().show(widen(player.name) + L" joined the game");
                continue;
            }
            Seen& last = found->second;
            if (last.name != player.name) {
                renames.push_back({playerItem({player.entityId, last.name, 0, 0, 0}), playerItem(player)});
                last.name = player.name;
            }
            bool moved = last.x != player.x || last.y != player.y || last.z != player.z;
            if (!moved || now - last.notified < playerRefresh) continue;
            last = {player.name, player.x, player.y, player.z, now};
            events.push_back({SHCNE_UPDATEITEM, playerItem(player)});
        }
    }
    for (auto& [from, to] : renames) rename({folder, from}, {folder, to});
    for (auto& [event, item] : events) notify(event, {folder, item});
}

void Host::chatAdded(const ChatLine& line) {
    Item folder{ItemKind::chat};
    std::optional<ChatLine> dropped;
    {
        std::lock_guard guard(chatLock);
        shownChat.push_back(line);
        if (shownChat.size() > chatHistory) {
            dropped = shownChat.front();
            shownChat.pop_front();
        }
    }
    notify(SHCNE_CREATE, {folder, chatItem(line)});
    if (dropped) notify(SHCNE_DELETE, {folder, chatItem(*dropped)});
}

}
