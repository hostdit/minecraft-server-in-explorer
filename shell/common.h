#pragma once

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <mcx/item.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <string>

namespace mcx {

extern HMODULE module;
extern std::atomic<long> objects;

inline const CLSID clsid = {0xA8DBB7AA, 0x9EA6, 0x47E3, {0xAD, 0xE7, 0x86, 0x15, 0xED, 0xA2, 0x29, 0xC7}};
inline const wchar_t clsidText[] = L"{A8DBB7AA-9EA6-47E3-ADE7-8615EDA229C7}";
inline const wchar_t folderName[] = L"Minecraft Server";

std::string narrow(std::wstring_view text);
std::wstring widen(std::string_view text);
std::filesystem::path dataDir();
void log(const std::string& line);
void report(const char* where, const char* what) noexcept;
bool hostIsExplorer();

template <class F>
HRESULT guard(const char* where, F&& body) noexcept {
    try {
        return body();
    } catch (const std::exception& error) {
        report(where, error.what());
        return E_UNEXPECTED;
    } catch (...) {
        report(where, "unknown exception");
        return E_UNEXPECTED;
    }
}

HRESULT toStrRet(const std::wstring& text, STRRET* out);

template <class T>
class Com : public T {
public:
    template <class... Arguments>
    explicit Com(Arguments&&... arguments) : T(std::forward<Arguments>(arguments)...) {
        objects++;
    }
    virtual ~Com() { objects--; }
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;

    IFACEMETHODIMP_(ULONG) AddRef() override { return ++references; }
    IFACEMETHODIMP_(ULONG) Release() override {
        long left = --references;
        if (!left) delete this;
        return ULONG(left);
    }

private:
    std::atomic<long> references = 1;
};

}
