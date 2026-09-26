#include <mcx/server.h>

#include <mcx/blocks.h>
#include <mcx/framing.h>
#include <mcx/net.h>
#include <mcx/png.h>
#include <mcx/slot.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <format>
#include <random>
#include <thread>

namespace mcx {

namespace {

using Clock = std::chrono::steady_clock;

constexpr int maxPlayers = 20;
constexpr Position spawn{0, 4, 0};
constexpr auto closeGrace = std::chrono::seconds(1);
constexpr auto poll = std::chrono::milliseconds(250);
constexpr auto sendTimeout = std::chrono::seconds(2);
constexpr auto topUp = std::chrono::milliseconds(250);

enum class Stage { handshake, status, login, play };

struct Uuid {
    std::uint64_t high;
    std::uint64_t low;

    std::string text() const {
        auto hex = std::format("{:016x}{:016x}", high, low);
        return std::format("{}-{}-{}-{}-{}", hex.substr(0, 8), hex.substr(8, 4), hex.substr(12, 4), hex.substr(16, 4), hex.substr(20));
    }
};

Uuid randomUuid() {
    static std::mutex lock;
    static std::mt19937_64 random{std::random_device{}()};
    std::lock_guard guard(lock);
    Uuid uuid{random(), random()};
    uuid.high = (uuid.high & ~0xF000ull) | 0x4000ull;
    uuid.low = (uuid.low & ~(0xC000ull << 48)) | (0x8000ull << 48);
    return uuid;
}

std::int32_t fixed(double value) {
    return std::int32_t(std::lround(value * 32));
}

std::uint8_t angle(float degrees) {
    return std::uint8_t(int(std::lround(degrees * 256.0f / 360.0f)) & 0xFF);
}

constexpr std::int32_t herobrineEntity = 1 << 30;
constexpr Uuid herobrineUuid{0x4865726F62726E65, 0x8000000000000000};
constexpr double herobrineZ = -44.5;

bool reserved(std::string_view name) {
    return name.size() == herobrineName.size() && std::equal(name.begin(), name.end(), herobrineName.begin(), [](char a, char b) { return std::tolower(std::uint8_t(a)) == std::tolower(std::uint8_t(b)); });
}

std::string escape(std::string_view text) {
    std::string out;
    for (char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        default:
            if (std::uint8_t(c) < 0x20) out += std::format("\\u{:04x}", int(c));
            else out += c;
        }
    }
    return out;
}

bool validName(std::string_view name) {
    return !name.empty() && name.size() <= 16 && std::all_of(name.begin(), name.end(), [](char c) { return std::isalnum(std::uint8_t(c)) || c == '_'; });
}

Position facing(Position position, int face) {
    switch (face) {
    case 0: return {position.x, position.y - 1, position.z};
    case 1: return {position.x, position.y + 1, position.z};
    case 2: return {position.x, position.y, position.z - 1};
    case 3: return {position.x, position.y, position.z + 1};
    case 4: return {position.x - 1, position.y, position.z};
    default: return {position.x + 1, position.y, position.z};
    }
}

struct Session {
    Socket socket;
    std::string peer;
    std::atomic<Stage> stage = Stage::handshake;
    std::atomic<bool> abandoned = false;
    std::int32_t protocol = 0;
    std::int32_t entityId = 0;
    std::string name;
    mutable std::mutex nameLock;
    Uuid uuid{};
    bool playing = false;
    double x = spawn.x + 0.5;
    double y = spawn.y;
    double z = spawn.z + 0.5;
    float yaw = 0;
    float pitch = 0;
    std::array<Slot, 9> hotbar{};
    int held = 0;
    Clock::time_point lastKeepAlive = Clock::now();
    std::int32_t keepAliveId = 0;

    std::mutex queueLock;
    std::condition_variable queueWake;
    std::deque<Bytes> queue;
    std::size_t queued = 0;
    bool closing = false;
    bool dead = false;
    Clock::time_point closingSince;
    std::thread writer;

    explicit Session(Socket accepted) : socket(std::move(accepted)), peer(socket.peer()) {}
};

using SessionPtr = std::shared_ptr<Session>;

}

std::string jsonText(std::string_view text) {
    return std::format(R"({{"text":"{}"}})", escape(text));
}

std::string thousands(std::size_t value) {
    std::string digits = std::to_string(value);
    for (int at = int(digits.size()) - 3; at > 0; at -= 3) digits.insert(std::size_t(at), ",");
    return digits;
}

struct Server::Impl : std::enable_shared_from_this<Impl> {
    World& world;
    ServerOptions options;
    std::string favicon = faviconDataUri();

    std::mutex runLock;
    std::atomic<bool> live = false;
    std::optional<Socket> listener;
    int boundPort = 0;
    std::optional<std::size_t> worldToken;
    std::thread acceptThread;
    std::thread tickThread;
    std::mutex tickLock;
    std::condition_variable tickWake;
    bool ticking = false;

    mutable std::mutex lock;
    std::condition_variable sessionsGone;
    std::vector<SessionPtr> sessions;
    std::size_t activeReaders = 0;
    std::int32_t nextEntity = 1;
    std::deque<ChatLine> chatLog;
    std::uint32_t nextChat = 1;
    std::vector<std::function<void()>> playerListeners;
    std::vector<std::function<void(const ChatLine&)>> chatListeners;
    bool haunted = false;
    std::int64_t timeOfDay = noon;
    Weather sky = Weather::clear;
    Clock::time_point lastTopUp;
    Clock::time_point nextLightning;
    std::mt19937 random{std::random_device{}()};

    Impl(World& world, ServerOptions options) : world(world), options(std::move(options)) {}

    void log(const std::string& line) {
        options.log(line);
    }

    StartResult start() {
        std::lock_guard run(runLock);
        if (live) return StartResult::started;
        listener = Socket::listen(options.port);
        if (!listener) {
            log(std::format("port {} is in use", options.port));
            return StartResult::portInUse;
        }
        boundPort = listener->localPort();
        live = true;
        std::weak_ptr<Impl> weak = shared_from_this();
        worldToken = world.onChange([weak](const Change& change) {
            if (auto self = weak.lock()) self->worldChanged(change);
        });
        auto self = shared_from_this();
        acceptThread = std::thread([self] { self->guarded("accept thread", [&] { self->acceptLoop(); }); });
        ticking = true;
        tickThread = std::thread([self] { self->guarded("tick thread", [&] { self->tickLoop(); }); });
        log(std::format("listening on {} in pid {}", boundPort, processId()));
        return StartResult::started;
    }

    void stop(const std::string& reason) {
        std::lock_guard run(runLock);
        if (!live.exchange(false)) return;
        world.removeListener(*worldToken);
        Socket::tryConnect("127.0.0.1", boundPort);
        acceptThread.join();
        listener->close();
        listener.reset();
        {
            std::lock_guard guard(tickLock);
            ticking = false;
        }
        tickWake.notify_all();
        tickThread.join();
        std::unique_lock guard(lock);
        for (auto& session : sessions) disconnect(*session, reason);
        if (!sessionsGone.wait_for(guard, closeGrace, [&] { return activeReaders == 0; }))
            for (auto& session : sessions) session->abandoned = true;
        sessionsGone.wait(guard, [&] { return activeReaders == 0; });
        guard.unlock();
        firePlayers();
        log(std::format("stopped: {}", reason));
    }

    template <class F>
    void guarded(const char* where, F&& body) {
        try {
            body();
        } catch (const std::exception& error) {
            log(std::format("{} failed: {}", where, error.what()));
        } catch (...) {
            log(std::format("{} failed with an unknown exception", where));
        }
    }

    void acceptLoop() {
        while (live) {
            auto accepted = listener->accept();
            if (!accepted || !live) break;
            accepted->setSendTimeout(sendTimeout);
            auto session = std::make_shared<Session>(std::move(*accepted));
            {
                std::lock_guard guard(lock);
                sessions.push_back(session);
                activeReaders++;
            }
            auto self = shared_from_this();
            session->writer = std::thread([self, session] { self->guarded("writer thread", [&] { self->writeLoop(*session); }); });
            std::thread([self, session] { self->readLoop(session); }).detach();
        }
    }

    void tickLoop() {
        std::unique_lock guard(tickLock);
        while (ticking) {
            tickWake.wait_for(guard, std::chrono::milliseconds(50));
            auto now = Clock::now();
            std::lock_guard sessionsGuard(lock);
            for (auto& session : sessions) {
                if (session->playing && now - session->lastKeepAlive >= options.keepAlive) {
                    session->lastKeepAlive = now;
                    Writer keepAlive = packet(0x00);
                    keepAlive.varInt(++session->keepAliveId);
                    send(*session, keepAlive);
                }
                std::lock_guard queueGuard(session->queueLock);
                if (session->closing && now - session->closingSince > closeGrace) session->abandoned = true;
            }
            if (sky != Weather::clear && now - lastTopUp >= topUp) {
                lastTopUp = now;
                for (auto& session : sessions)
                    if (session->playing) sendStrength(*session);
            }
            if (sky == Weather::thunder && now >= nextLightning) {
                nextLightning = now + lightningDelay();
                strike();
            }
        }
    }

    Writer timePacket() const {
        Writer update = packet(0x03);
        update.i64(0).i64(-(timeOfDay ? timeOfDay : dayLength));
        return update;
    }

    Writer gameState(std::uint8_t reason, float value) const {
        Writer state = packet(0x2B);
        state.u8(reason).f32(value);
        return state;
    }

    void sendStrength(Session& session) {
        send(session, gameState(7, sky == Weather::clear ? 0.0f : 1.0f));
        send(session, gameState(8, sky == Weather::thunder ? 1.0f : 0.0f));
    }

    void sendWeather(Session& session) {
        send(session, gameState(sky == Weather::clear ? 2 : 1, 0));
        sendStrength(session);
    }

    Clock::duration lightningDelay() {
        return std::chrono::duration_cast<Clock::duration>(options.lightning * std::uniform_real_distribution(0.5, 1.5)(random));
    }

    void strike() {
        std::vector<Session*> playing;
        for (auto& session : sessions)
            if (session->playing) playing.push_back(session.get());
        if (playing.empty()) return;
        Session& near = *playing[std::uniform_int_distribution<std::size_t>(0, playing.size() - 1)(random)];
        std::uniform_real_distribution offset(-16.0, 16.0);
        Writer bolt = packet(0x2C);
        bolt.varInt(nextEntity++).u8(1).i32(fixed(near.x + offset(random))).i32(fixed(spawn.y)).i32(fixed(near.z + offset(random)));
        for (Session* session : playing) send(*session, bolt);
    }

    void setTime(std::int64_t ticks) {
        if (ticks < 0 || ticks >= dayLength) throw std::invalid_argument(std::format("time {} is outside the day", ticks));
        std::lock_guard guard(lock);
        timeOfDay = ticks;
        for (auto& session : sessions)
            if (session->playing) send(*session, timePacket());
    }

    void setWeather(Weather weather) {
        std::lock_guard guard(lock);
        sky = weather;
        nextLightning = Clock::now() + lightningDelay();
        for (auto& session : sessions)
            if (session->playing) sendWeather(*session);
    }

    void writeLoop(Session& session) {
        while (true) {
            Bytes data;
            {
                std::unique_lock guard(session.queueLock);
                session.queueWake.wait(guard, [&] { return session.dead || !session.queue.empty() || session.closing; });
                if (session.dead) return;
                if (session.queue.empty()) {
                    session.socket.finish();
                    return;
                }
                data = std::move(session.queue.front());
                session.queue.pop_front();
                session.queued -= data.size();
            }
            if (!session.socket.sendAll(data)) {
                session.socket.shutdown();
                return;
            }
        }
    }

    void readLoop(SessionPtr session) {
        guarded("session", [&] {
            Reassembler reassembler;
            std::uint8_t buffer[8192];
            try {
                while (!session->abandoned) {
                    if (!session->socket.waitReadable(poll)) continue;
                    std::size_t read = session->socket.receive(buffer);
                    if (!read) break;
                    reassembler.feed(std::span(buffer, read));
                    while (auto body = reassembler.next()) handle(session, *body);
                }
            } catch (const ProtocolError& error) {
                log(std::format("{} sent a bad packet: {}", describe(*session), error.what()));
                disconnect(*session, "Bad packet");
            } catch (const NetError& error) {
                log(std::format("{} dropped: {}", describe(*session), error.what()));
            }
        });
        guarded("session cleanup", [&] { finish(session); });
    }

    void finish(const SessionPtr& session) {
        {
            std::lock_guard guard(session->queueLock);
            if (!session->closing) session->dead = true;
        }
        session->queueWake.notify_all();
        session->writer.join();
        session->socket.shutdown();
        bool wasPlaying;
        {
            std::lock_guard guard(lock);
            wasPlaying = session->playing;
            session->playing = false;
            std::erase(sessions, session);
            if (wasPlaying) {
                Writer destroy = packet(0x13);
                destroy.varInt(1).varInt(session->entityId);
                Writer unlist = packet(0x38);
                unlist.varInt(4).varInt(1);
                uuid(unlist, session->uuid);
                for (auto& other : sessions) {
                    if (!other->playing) continue;
                    send(*other, destroy);
                    send(*other, unlist);
                }
            }
        }
        session->socket.close();
        if (wasPlaying) {
            log(std::format("{} left", nameOf(*session)));
            say(std::format("{} left the game", nameOf(*session)));
            firePlayers();
        }
        std::lock_guard guard(lock);
        activeReaders--;
        sessionsGone.notify_all();
    }

    static std::string nameOf(const Session& session) {
        std::lock_guard guard(session.nameLock);
        return session.name;
    }

    std::string describe(const Session& session) const {
        std::string name = nameOf(session);
        return name.empty() ? session.peer : name;
    }

    void send(Session& session, const Writer& body) {
        Bytes framed = frame(body);
        {
            std::lock_guard guard(session.queueLock);
            if (session.dead || session.closing) return;
            if (session.queued + framed.size() > options.maxQueue) {
                session.queue.clear();
                session.queued = 0;
                session.closing = true;
                session.closingSince = Clock::now();
                log(std::format("{} fell too far behind", describe(session)));
            } else {
                session.queued += framed.size();
                session.queue.push_back(std::move(framed));
            }
        }
        session.queueWake.notify_all();
    }

    void disconnect(Session& session, const std::string& reason) {
        if (session.stage == Stage::play) {
            Writer body = packet(0x40);
            body.string(jsonText(reason));
            send(session, body);
        } else if (session.stage == Stage::login) {
            Writer body = packet(0x00);
            body.string(jsonText(reason));
            send(session, body);
        }
        {
            std::lock_guard guard(session.queueLock);
            if (!session.closing) session.closingSince = Clock::now();
            session.closing = true;
        }
        session.queueWake.notify_all();
    }

    void uuid(Writer& writer, const Uuid& value) {
        writer.i64(std::int64_t(value.high)).i64(std::int64_t(value.low));
    }

    void handle(const SessionPtr& session, const Bytes& body) {
        Reader reader(body);
        std::int32_t id = reader.varInt();
        switch (session->stage.load()) {
        case Stage::handshake: return handshake(*session, id, reader);
        case Stage::status: return status(*session, id, reader);
        case Stage::login: return login(session, id, reader);
        case Stage::play: return play(session, id, reader);
        }
    }

    void handshake(Session& session, std::int32_t id, Reader& reader) {
        if (id != 0x00) throw ProtocolError(std::format("expected a handshake, got {}", id));
        session.protocol = reader.varInt();
        reader.string();
        reader.u16();
        std::int32_t next = reader.varInt();
        if (next == 1) session.stage = Stage::status;
        else if (next == 2) session.stage = Stage::login;
        else throw ProtocolError(std::format("unknown next state {}", next));
    }

    void status(Session& session, std::int32_t id, Reader& reader) {
        if (id == 0x00) {
            Writer response = packet(0x00);
            response.string(statusJson());
            send(session, response);
        } else if (id == 0x01) {
            Writer pong = packet(0x01);
            pong.i64(reader.i64());
            send(session, pong);
            disconnect(session, "");
        } else {
            throw ProtocolError(std::format("unknown status packet {}", id));
        }
    }

    std::string statusJson() {
        std::string sample;
        std::size_t online = 0;
        {
            std::lock_guard guard(lock);
            for (auto& session : sessions) {
                if (!session->playing) continue;
                if (online++) sample += ",";
                sample += std::format(R"({{"name":"{}","id":"{}"}})", escape(session->name), session->uuid.text());
            }
        }
        return std::format(R"({{"version":{{"name":"1.8.9","protocol":{}}},"players":{{"max":{},"online":{},"sample":[{}]}},"description":{},"favicon":"{}"}})",
                           protocolVersion, maxPlayers, online, sample, jsonText(motd()), favicon);
    }

    std::string motd() const {
        return std::format("{} | pid {} | {} files", options.host, processId(), thousands(world.count()));
    }

    void login(const SessionPtr& session, std::int32_t id, Reader& reader) {
        if (id != 0x00) throw ProtocolError(std::format("expected login start, got {}", id));
        std::string name = reader.string();
        if (session->protocol != protocolVersion) return disconnect(*session, "This server runs Minecraft 1.8.9");
        if (!validName(name)) return disconnect(*session, "That name won't fit in a file name");
        if (reserved(name)) return disconnect(*session, "Herobrine is already here");
        {
            std::lock_guard guard(lock);
            for (auto& other : sessions)
                if (other != session && other->name == name) return disconnect(*session, std::format("Someone called {} is already here", name));
            {
                std::lock_guard nameGuard(session->nameLock);
                session->name = name;
            }
            session->entityId = nextEntity++;
            session->uuid = randomUuid();
        }
        Writer success = packet(0x02);
        success.string(session->uuid.text()).string(name);
        send(*session, success);
        session->stage = Stage::play;

        Writer join = packet(0x01);
        join.i32(session->entityId).u8(1).u8(0).u8(0).u8(maxPlayers).string("flat").boolean(false);
        Writer spawnPosition = packet(0x05);
        spawnPosition.position(spawn);
        Writer abilities = packet(0x39);
        abilities.u8(0x0D).f32(0.05f).f32(0.1f);

        {
            std::lock_guard guard(lock);
            send(*session, join);
            send(*session, spawnPosition);
            send(*session, abilities);
            send(*session, timePacket());
            if (sky != Weather::clear) sendWeather(*session);
            sendWorld(*session);
            placeAtSpawn(*session);
            session->playing = true;
            session->lastKeepAlive = Clock::now();
            send(*session, listEntry(*session));
            for (auto& other : sessions) {
                if (other == session || !other->playing) continue;
                send(*session, listEntry(*other));
                send(*session, spawnPlayer(*other));
                if (!other->hotbar[other->held].empty()) send(*session, equipment(*other));
                send(*other, listEntry(*session));
                send(*other, spawnPlayer(*session));
            }
            if (haunted) showHerobrine(*session);
        }
        log(std::format("{} joined from {}", name, session->peer));
        say(std::format("{} joined the game", name));
        firePlayers();
    }

    void sendWorld(Session& session) {
        for (int chunkX = -worldRadius; chunkX <= worldRadius; chunkX++)
            for (int chunkZ = -worldRadius; chunkZ <= worldRadius; chunkZ++) send(session, chunkPacket(chunkX, chunkZ, world.chunk(chunkX, chunkZ)));
    }

    void placeAtSpawn(Session& session) {
        session.x = spawn.x + 0.5;
        session.y = spawn.y;
        session.z = spawn.z + 0.5;
        Writer look = packet(0x08);
        look.f64(spawn.x + 0.5).f64(spawn.y).f64(spawn.z + 0.5).f32(0).f32(0).u8(0);
        send(session, look);
    }

    Writer listEntry(const Session& session) {
        Writer entry = packet(0x38);
        entry.varInt(0).varInt(1);
        uuid(entry, session.uuid);
        entry.string(session.name).varInt(0).varInt(1).varInt(0).boolean(false);
        return entry;
    }

    Writer spawnPlayer(const Session& session) {
        Writer spawned = packet(0x0C);
        spawned.varInt(session.entityId);
        uuid(spawned, session.uuid);
        spawned.i32(fixed(session.x)).i32(fixed(session.y)).i32(fixed(session.z)).u8(angle(session.yaw)).u8(angle(session.pitch)).i16(std::max<std::int16_t>(session.hotbar[session.held].id, 0)).u8(0x7F);
        return spawned;
    }

    void play(const SessionPtr& session, std::int32_t id, Reader& reader) {
        switch (id) {
        case 0x01: {
            std::string text = reader.string();
            if (text.size() > 256) throw ProtocolError("chat message too long");
            say(std::format("<{}> {}", nameOf(*session), text));
            return;
        }
        case 0x04: {
            double x = reader.f64();
            double y = reader.f64();
            double z = reader.f64();
            return moved(session, x, y, z, std::nullopt);
        }
        case 0x05: {
            float yaw = reader.f32();
            float pitch = reader.f32();
            return turned(session, yaw, pitch);
        }
        case 0x06: {
            double x = reader.f64();
            double y = reader.f64();
            double z = reader.f64();
            float yaw = reader.f32();
            float pitch = reader.f32();
            return moved(session, x, y, z, std::pair(yaw, pitch));
        }
        case 0x07: {
            std::uint8_t action = reader.u8();
            Position position = reader.position();
            if (action == 0) dig(*session, position);
            return;
        }
        case 0x08: return place(*session, reader);
        case 0x09: {
            std::int16_t slot = reader.i16();
            if (slot < 0 || slot > 8) throw ProtocolError(std::format("hotbar slot {} out of range", slot));
            std::lock_guard guard(lock);
            session->held = slot;
            return broadcastOthers(*session, equipment(*session));
        }
        case 0x0A: {
            Writer animation = packet(0x0B);
            animation.varInt(session->entityId).u8(0);
            std::lock_guard guard(lock);
            return broadcastOthers(*session, animation);
        }
        case 0x10: {
            std::int16_t slot = reader.i16();
            Slot item = readSlot(reader);
            if (slot < 36 || slot > 44) return;
            std::lock_guard guard(lock);
            session->hotbar[slot - 36] = std::move(item);
            if (slot - 36 == session->held) broadcastOthers(*session, equipment(*session));
            return;
        }
        case 0x16:
            if (reader.varInt() == 0) respawn(*session);
            return;
        default:
            if (id < 0 || id > 0x19) throw ProtocolError(std::format("unknown play packet {}", id));
        }
    }

    void moved(const SessionPtr& session, double x, double y, double z, std::optional<std::pair<float, float>> look) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) throw ProtocolError("position is not a number");
        {
            std::lock_guard guard(lock);
            session->x = x;
            session->y = y;
            session->z = z;
            if (look) {
                session->yaw = look->first;
                session->pitch = look->second;
            }
            broadcastMovement(*session);
        }
        firePlayers();
    }

    void turned(const SessionPtr& session, float yaw, float pitch) {
        std::lock_guard guard(lock);
        session->yaw = yaw;
        session->pitch = pitch;
        broadcastMovement(*session);
    }

    void broadcastMovement(const Session& session) {
        Writer teleport = packet(0x18);
        teleport.varInt(session.entityId).i32(fixed(session.x)).i32(fixed(session.y)).i32(fixed(session.z)).u8(angle(session.yaw)).u8(angle(session.pitch)).boolean(true);
        Writer head = packet(0x19);
        head.varInt(session.entityId).u8(angle(session.yaw));
        for (auto& other : sessions) {
            if (other.get() == &session || !other->playing) continue;
            send(*other, teleport);
            send(*other, head);
        }
    }

    Writer equipment(const Session& session) {
        Writer equipped = packet(0x04);
        equipped.varInt(session.entityId).i16(0);
        writeSlot(equipped, session.hotbar[session.held]);
        return equipped;
    }

    void broadcastOthers(const Session& session, const Writer& body) {
        for (auto& other : sessions)
            if (other.get() != &session && other->playing) send(*other, body);
    }

    void correct(Session& session, Position position) {
        Writer change = packet(0x23);
        change.position(position).varInt(world.get(position));
        send(session, change);
    }

    void dig(Session& session, Position position) {
        if (!world.inside(position) || !world.get(position)) return correct(session, position);
        world.set(position, 0, nameOf(session), std::chrono::system_clock::now());
    }

    void place(Session& session, Reader& reader) {
        Position clicked = reader.position();
        std::uint8_t face = reader.u8();
        std::int16_t item = reader.i16();
        std::int16_t damage = 0;
        if (item >= 0) {
            reader.u8();
            damage = reader.i16();
        }
        if (face > 5 || item < 0) return;
        Position target = facing(clicked, face);
        auto block = blockForItem(item, damage);
        if (!block || !world.inside(target) || world.get(target)) return correct(session, target);
        world.set(target, *block, nameOf(session), std::chrono::system_clock::now());
    }

    void respawn(Session& session) {
        Writer respawned = packet(0x07);
        respawned.i32(0).u8(0).u8(1).string("flat");
        std::lock_guard guard(lock);
        send(session, respawned);
        sendWorld(session);
        placeAtSpawn(session);
        broadcastMovement(session);
    }

    void worldChanged(const Change& change) {
        Writer update;
        if (change.kind == ChangeKind::chunkCleared) {
            update = chunkPacket(change.chunkX, change.chunkZ, world.chunk(change.chunkX, change.chunkZ));
        } else {
            update = packet(0x23);
            update.position(change.position).varInt(change.after);
        }
        std::lock_guard guard(lock);
        for (auto& session : sessions)
            if (session->playing) send(*session, update);
    }

    void say(const std::string& text) {
        ChatLine line;
        std::vector<std::function<void(const ChatLine&)>> listeners;
        {
            std::lock_guard guard(lock);
            line = {nextChat++, text};
            chatLog.push_back(line);
            if (chatLog.size() > chatHistory) chatLog.pop_front();
            Writer chat = packet(0x02);
            chat.string(jsonText(text)).u8(0);
            for (auto& session : sessions)
                if (session->playing) send(*session, chat);
            listeners = chatListeners;
        }
        for (auto& callback : listeners) callback(line);
    }

    void firePlayers() {
        std::vector<std::function<void()>> listeners;
        {
            std::lock_guard guard(lock);
            listeners = playerListeners;
        }
        for (auto& callback : listeners) callback();
    }

    bool kick(std::int32_t entityId, const std::string& reason) {
        std::lock_guard guard(lock);
        for (auto& session : sessions) {
            if (!session->playing || session->entityId != entityId) continue;
            {
                std::lock_guard queueGuard(session->queueLock);
                if (session->closing) return false;
            }
            log(std::format("kicked {}: {}", session->name, reason));
            disconnect(*session, reason);
            return true;
        }
        return false;
    }

    void showHerobrine(Session& session) {
        Writer entry = packet(0x38);
        entry.varInt(0).varInt(1);
        uuid(entry, herobrineUuid);
        entry.string(std::string(herobrineName)).varInt(0).varInt(1).varInt(0).boolean(false);
        Writer spawned = packet(0x0C);
        spawned.varInt(herobrineEntity);
        uuid(spawned, herobrineUuid);
        spawned.i32(fixed(spawn.x + 0.5)).i32(fixed(spawn.y)).i32(fixed(herobrineZ)).u8(0).u8(0).i16(0).u8(0x7F);
        Writer unlist = packet(0x38);
        unlist.varInt(4).varInt(1);
        uuid(unlist, herobrineUuid);
        send(session, entry);
        send(session, spawned);
        send(session, unlist);
    }

    void setHerobrine(bool present) {
        std::lock_guard guard(lock);
        if (haunted == present) return;
        haunted = present;
        Writer destroy = packet(0x13);
        destroy.varInt(1).varInt(herobrineEntity);
        for (auto& session : sessions) {
            if (!session->playing) continue;
            if (present) showHerobrine(*session);
            else send(*session, destroy);
        }
        log(present ? "Herobrine appeared" : "Herobrine left");
    }

    RenameResult rename(std::int32_t entityId, const std::string& name) {
        if (!validName(name)) return RenameResult::invalid;
        if (reserved(name)) return RenameResult::taken;
        std::string before;
        {
            std::lock_guard guard(lock);
            auto target = std::find_if(sessions.begin(), sessions.end(), [&](const SessionPtr& session) { return session->playing && session->entityId == entityId; });
            if (target == sessions.end()) return RenameResult::missing;
            Session& renamed = **target;
            for (auto& other : sessions)
                if (other.get() != &renamed && other->name == name) return RenameResult::taken;
            before = renamed.name;
            {
                std::lock_guard nameGuard(renamed.nameLock);
                renamed.name = name;
            }
            Writer unlist = packet(0x38);
            unlist.varInt(4).varInt(1);
            uuid(unlist, renamed.uuid);
            Writer destroy = packet(0x13);
            destroy.varInt(1).varInt(renamed.entityId);
            for (auto& other : sessions) {
                if (!other->playing) continue;
                send(*other, unlist);
                send(*other, listEntry(renamed));
                if (other.get() == &renamed) continue;
                send(*other, destroy);
                send(*other, spawnPlayer(renamed));
                if (!renamed.hotbar[renamed.held].empty()) send(*other, equipment(renamed));
            }
        }
        log(std::format("renamed {} to {}", before, name));
        say(std::format("{} is now called {}", before, name));
        firePlayers();
        return RenameResult::renamed;
    }

    std::vector<PlayerInfo> players() const {
        std::lock_guard guard(lock);
        std::vector<PlayerInfo> found;
        for (auto& session : sessions) {
            if (!session->playing) continue;
            std::lock_guard queueGuard(session->queueLock);
            if (session->closing || session->dead) continue;
            found.push_back({session->entityId, session->name, session->x, session->y, session->z});
        }
        return found;
    }
};

Server::Server(World& world, ServerOptions options) : impl(std::make_shared<Impl>(world, std::move(options))) {}

Server::~Server() {
    impl->guarded("server shutdown", [&] { impl->stop("Server closed"); });
}

StartResult Server::start() { return impl->start(); }
void Server::stop(const std::string& reason) { impl->stop(reason); }
bool Server::running() const { return impl->live; }
int Server::port() const { return impl->boundPort; }
std::string Server::motd() const { return impl->motd(); }
std::vector<PlayerInfo> Server::players() const { return impl->players(); }
bool Server::kick(std::int32_t entityId, const std::string& reason) { return impl->kick(entityId, reason); }
RenameResult Server::rename(std::int32_t entityId, const std::string& name) { return impl->rename(entityId, name); }
void Server::say(const std::string& text) { impl->say(text); }

std::vector<ChatLine> Server::chat() const {
    std::lock_guard guard(impl->lock);
    return {impl->chatLog.begin(), impl->chatLog.end()};
}

std::int64_t Server::time() const {
    std::lock_guard guard(impl->lock);
    return impl->timeOfDay;
}

void Server::setTime(std::int64_t ticks) { impl->setTime(ticks); }

Weather Server::weather() const {
    std::lock_guard guard(impl->lock);
    return impl->sky;
}

void Server::setWeather(Weather weather) { impl->setWeather(weather); }

bool Server::herobrine() const {
    std::lock_guard guard(impl->lock);
    return impl->haunted;
}

void Server::setHerobrine(bool present) { impl->setHerobrine(present); }

void Server::onPlayers(std::function<void()> callback) {
    std::lock_guard guard(impl->lock);
    impl->playerListeners.push_back(std::move(callback));
}

void Server::onChat(std::function<void(const ChatLine&)> callback) {
    std::lock_guard guard(impl->lock);
    impl->chatListeners.push_back(std::move(callback));
}

}
