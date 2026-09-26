#include <mcx/net.h>

#include <format>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
using SocketHandle = SOCKET;
using SocketLength = int;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
using SocketHandle = int;
using SocketLength = socklen_t;
#endif

namespace mcx {

namespace {

SocketHandle raw(std::uintptr_t handle) {
    return SocketHandle(handle);
}

int lastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool inUse(int error) {
#ifdef _WIN32
    return error == WSAEADDRINUSE || error == WSAEACCES;
#else
    return error == EADDRINUSE;
#endif
}

bool timedOut(int error) {
#ifdef _WIN32
    return error == WSAETIMEDOUT;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}

bool closedByPeer(int error) {
#ifdef _WIN32
    return error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAESHUTDOWN || error == WSAENOTSOCK || error == WSAEINTR;
#else
    return error == ECONNRESET || error == EPIPE || error == EBADF || error == ENOTCONN || error == EINTR;
#endif
}

[[noreturn]] void fail(const char* what, int error) {
    throw NetError(std::format("{} failed with error {}", what, error));
}

}

void netStartup() {
#ifdef _WIN32
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA data;
        if (int error = WSAStartup(MAKEWORD(2, 2), &data)) fail("WSAStartup", error);
    });
#endif
}

int processId() {
#ifdef _WIN32
    return _getpid();
#else
    return int(getpid());
#endif
}

Socket::Socket(std::uintptr_t handle) : handle(handle) {}

Socket::Socket(Socket&& other) noexcept : handle(std::exchange(other.handle, invalid)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle = std::exchange(other.handle, invalid);
    }
    return *this;
}

Socket::~Socket() {
    close();
}

std::optional<Socket> Socket::listen(int port) {
    netStartup();
    Socket socket(std::uintptr_t(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)));
    if (!socket.open()) fail("socket", lastError());
    int on = 1;
#ifdef _WIN32
    setsockopt(raw(socket.handle), SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&on), sizeof on);
#else
    setsockopt(raw(socket.handle), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(std::uint16_t(port));
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(raw(socket.handle), reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
        int error = lastError();
        if (inUse(error)) return std::nullopt;
        fail("bind", error);
    }
    if (::listen(raw(socket.handle), SOMAXCONN) != 0) fail("listen", lastError());
    return socket;
}

Socket Socket::connect(const std::string& host, int port) {
    auto socket = tryConnect(host, port);
    if (!socket) fail("connect", lastError());
    return std::move(*socket);
}

std::optional<Socket> Socket::tryConnect(const std::string& host, int port) {
    netStartup();
    Socket socket(std::uintptr_t(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)));
    if (!socket.open()) fail("socket", lastError());
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(std::uint16_t(port));
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) throw NetError("not an IPv4 address: " + host);
    if (::connect(raw(socket.handle), reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) return std::nullopt;
    return socket;
}

int Socket::localPort() const {
    sockaddr_in address{};
    SocketLength size = sizeof address;
    if (getsockname(raw(handle), reinterpret_cast<sockaddr*>(&address), &size) != 0) fail("getsockname", lastError());
    return ntohs(address.sin_port);
}

std::string Socket::peer() const {
    sockaddr_in address{};
    SocketLength size = sizeof address;
    if (getpeername(raw(handle), reinterpret_cast<sockaddr*>(&address), &size) != 0) return "unknown";
    char text[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &address.sin_addr, text, sizeof text);
    return std::format("{}:{}", text, ntohs(address.sin_port));
}

std::optional<Socket> Socket::accept() const {
    auto accepted = ::accept(raw(handle), nullptr, nullptr);
    if (accepted == SocketHandle(invalid)) {
        int error = lastError();
        if (closedByPeer(error) || !open()) return std::nullopt;
#ifdef _WIN32
        if (error == WSAEINVAL) return std::nullopt;
#else
        if (error == EINVAL || error == ECONNABORTED) return std::nullopt;
#endif
        fail("accept", error);
    }
    Socket socket{std::uintptr_t(accepted)};
    int on = 1;
    setsockopt(raw(socket.handle), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof on);
#ifdef SO_NOSIGPIPE
    setsockopt(raw(socket.handle), SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#endif
    return socket;
}

std::size_t Socket::receive(std::span<std::uint8_t> buffer) const {
    auto read = ::recv(raw(handle), reinterpret_cast<char*>(buffer.data()), int(buffer.size()), 0);
    if (read >= 0) return std::size_t(read);
    int error = lastError();
    if (closedByPeer(error)) return 0;
    if (timedOut(error)) throw NetError("receive timed out");
    fail("recv", error);
}

bool Socket::sendAll(std::span<const std::uint8_t> data) const {
    std::size_t sent = 0;
    while (sent < data.size()) {
#ifdef MSG_NOSIGNAL
        int flags = MSG_NOSIGNAL;
#else
        int flags = 0;
#endif
        auto wrote = ::send(raw(handle), reinterpret_cast<const char*>(data.data() + sent), int(std::min<std::size_t>(data.size() - sent, 16 * 1024)), flags);
        if (wrote < 0) {
            int error = lastError();
            if (closedByPeer(error) || timedOut(error)) return false;
            fail("send", error);
        }
        sent += std::size_t(wrote);
    }
    return true;
}

bool Socket::waitReadable(std::chrono::milliseconds timeout) const {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(raw(handle), &readable);
    timeval wait{long(timeout.count() / 1000), long(timeout.count() % 1000 * 1000)};
    int ready = ::select(int(raw(handle)) + 1, &readable, nullptr, nullptr, &wait);
    if (ready < 0) {
        int error = lastError();
        if (closedByPeer(error)) return true;
        fail("select", error);
    }
    return ready > 0;
}

void Socket::setReceiveTimeout(std::chrono::milliseconds timeout) const {
#ifdef _WIN32
    DWORD value = DWORD(timeout.count());
    setsockopt(raw(handle), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&value), sizeof value);
#else
    timeval value{time_t(timeout.count() / 1000), suseconds_t(timeout.count() % 1000 * 1000)};
    setsockopt(raw(handle), SOL_SOCKET, SO_RCVTIMEO, &value, sizeof value);
#endif
}

void Socket::setSendTimeout(std::chrono::milliseconds timeout) const {
#ifdef _WIN32
    DWORD value = DWORD(timeout.count());
    setsockopt(raw(handle), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&value), sizeof value);
#else
    timeval value{time_t(timeout.count() / 1000), suseconds_t(timeout.count() % 1000 * 1000)};
    setsockopt(raw(handle), SOL_SOCKET, SO_SNDTIMEO, &value, sizeof value);
#endif
}

void Socket::shutdown() const {
#ifdef _WIN32
    ::shutdown(raw(handle), SD_BOTH);
#else
    ::shutdown(raw(handle), SHUT_RDWR);
#endif
}

void Socket::finish() const {
#ifdef _WIN32
    ::shutdown(raw(handle), SD_SEND);
#else
    ::shutdown(raw(handle), SHUT_WR);
#endif
}

void Socket::close() {
    if (!open()) return;
#ifdef _WIN32
    closesocket(raw(handle));
#else
    ::close(raw(handle));
#endif
    handle = invalid;
}

bool Socket::open() const {
    return handle != invalid;
}

}
