#include "common.h"

#include <chrono>
#include <fstream>
#include <mutex>

namespace mcx {

HMODULE module;
std::atomic<long> objects;

namespace {

std::mutex logLock;

}

std::string narrow(std::wstring_view text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring widen(std::string_view text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring out(size_t(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size);
    return out;
}

std::filesystem::path dataDir() {
    PWSTR raw;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) throw std::runtime_error("no LocalAppData");
    std::filesystem::path dir = std::filesystem::path(raw) / L"McExplorer";
    CoTaskMemFree(raw);
    std::filesystem::create_directories(dir);
    return dir;
}

void log(const std::string& line) {
    auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    auto text = std::format("{:%F %T} pid {} tid {} {}\n", now, GetCurrentProcessId(), GetCurrentThreadId(), line);
    OutputDebugStringW(widen(text).c_str());
    std::lock_guard guard(logLock);
    std::ofstream(dataDir() / L"log.txt", std::ios::app | std::ios::binary) << text;
}

void report(const char* where, const char* what) noexcept {
    try {
        log(std::format("{} threw: {}", where, what));
    } catch (...) {
        OutputDebugStringW(L"McExplorer: logging failed\n");
    }
}

bool hostIsExplorer() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return _wcsicmp(PathFindFileNameW(path), L"explorer.exe") == 0;
}

HRESULT toStrRet(const std::wstring& text, STRRET* out) {
    out->uType = STRRET_WSTR;
    return SHStrDupW(text.c_str(), &out->pOleStr);
}

}
