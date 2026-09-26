#pragma once

#include <mcx/protocol.h>

#include <chrono>
#include <cstdint>

namespace mcx {

struct NetError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Socket {
public:
    Socket() = default;
    explicit Socket(std::uintptr_t handle);
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    ~Socket();

    static std::optional<Socket> listen(int port);
    static Socket connect(const std::string& host, int port);
    static std::optional<Socket> tryConnect(const std::string& host, int port);

    int localPort() const;
    std::string peer() const;
    std::optional<Socket> accept() const;
    std::size_t receive(std::span<std::uint8_t> buffer) const;
    bool sendAll(std::span<const std::uint8_t> data) const;
    bool waitReadable(std::chrono::milliseconds timeout) const;
    void setReceiveTimeout(std::chrono::milliseconds timeout) const;
    void setSendTimeout(std::chrono::milliseconds timeout) const;
    void shutdown() const;
    void finish() const;
    void close();
    bool open() const;

private:
    std::uintptr_t handle = invalid;
    static constexpr std::uintptr_t invalid = ~std::uintptr_t(0);
};

void netStartup();
int processId();

}
