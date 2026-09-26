#pragma once

#include <mcx/sky.h>
#include <mcx/world.h>

#include <functional>
#include <string_view>
#include <memory>

namespace mcx {

constexpr std::int32_t protocolVersion = 47;
constexpr std::size_t chatHistory = 100;
constexpr std::string_view herobrineName = "Herobrine";

struct ServerOptions {
    int port = 25565;
    std::string host = "File Explorer";
    std::chrono::milliseconds keepAlive = std::chrono::seconds(15);
    std::size_t maxQueue = 8 * 1024 * 1024;
    std::chrono::milliseconds lightning = std::chrono::seconds(8);
    std::function<void(const std::string&)> log = [](const std::string&) {};
};

struct PlayerInfo {
    std::int32_t entityId;
    std::string name;
    double x;
    double y;
    double z;
};

struct ChatLine {
    std::uint32_t id;
    std::string text;
};

enum class StartResult { started, portInUse };
enum class RenameResult { renamed, missing, invalid, taken };

std::string jsonText(std::string_view text);
std::string thousands(std::size_t value);

class Server {
public:
    Server(World& world, ServerOptions options);
    ~Server();

    StartResult start();
    void stop(const std::string& reason);
    bool running() const;
    int port() const;
    std::string motd() const;

    std::vector<PlayerInfo> players() const;
    bool kick(std::int32_t entityId, const std::string& reason);
    RenameResult rename(std::int32_t entityId, const std::string& name);
    bool herobrine() const;
    void setHerobrine(bool present);
    void say(const std::string& text);
    std::vector<ChatLine> chat() const;
    std::int64_t time() const;
    void setTime(std::int64_t ticks);
    Weather weather() const;
    void setWeather(Weather weather);

    void onPlayers(std::function<void()> listener);
    void onChat(std::function<void(const ChatLine&)> listener);

private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}
