#include <mcx/autosave.h>
#include <mcx/server.h>

#include <atomic>
#include <csignal>
#include <format>
#include <iostream>
#include <mutex>
#include <thread>

using namespace mcx;

namespace {

std::atomic<bool> quit = false;
std::mutex printLock;

void print(const std::string& line) {
    auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    std::lock_guard guard(printLock);
    std::cout << std::format("{:%T} {}", now, line) << std::endl;
}

}

int main(int argc, char** argv) {
    int port = 25565;
    std::filesystem::path path = "world.bin";
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string_view flag = argv[i];
        if (flag == "--port") port = std::stoi(argv[i + 1]);
        else if (flag == "--world") path = argv[i + 1];
        else {
            std::cerr << "usage: mcx_standalone [--port 25565] [--world world.bin]\n";
            return 2;
        }
    }

    auto world = loadWorld(path, std::chrono::system_clock::now(), print);
    Autosave autosave(*world, path, std::chrono::seconds(1), print);
    ServerOptions options;
    options.port = port;
    options.host = "Standalone";
    options.log = print;
    Server server(*world, options);
    server.onChat([](const ChatLine& line) { print(line.text); });
    if (server.start() == StartResult::portInUse) return 1;
    print(server.motd());

    std::signal(SIGINT, [](int) { quit = true; });
    std::thread([&] {
        std::string line;
        while (!quit && std::getline(std::cin, line)) {
            if (line.ends_with('\r')) line.pop_back();
            if (line == "stop") quit = true;
            else if (line == "list") for (const auto& player : server.players()) print(std::format("{} at {:.1f} {:.1f} {:.1f}", player.name, player.x, player.y, player.z));
            else if (line.starts_with("say ")) server.say("[Console] " + line.substr(4));
            else print("commands: say <text>, list, stop");
        }
    }).detach();

    while (!quit) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop("Server closed");
    return 0;
}
