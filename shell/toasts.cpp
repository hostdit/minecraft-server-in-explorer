#include "toasts.h"

#include "icons.h"

#include <mcx/blocks.h>

#include <strsafe.h>
#include <thread>

namespace mcx {

namespace {

const wchar_t windowClass[] = L"McExplorer.toasts";
constexpr UINT wake = WM_APP + 1;
constexpr UINT iconId = 1;

}

Toasts& Toasts::get() {
    static Toasts* toasts = new Toasts();
    return *toasts;
}

Toasts::Toasts() {
    std::thread([this] {
        guard("toasts", [&] {
            run();
            return S_OK;
        });
    }).detach();
}

void Toasts::show(const std::wstring& text) {
    {
        std::lock_guard guard(lock);
        pending.push_back({false, text});
    }
    if (HWND target = window) PostMessageW(target, wake, 0, 0);
}

void Toasts::hide() {
    {
        std::lock_guard guard(lock);
        pending.push_back({true});
    }
    if (HWND target = window) PostMessageW(target, wake, 0, 0);
}

LRESULT CALLBACK Toasts::windowProc(HWND target, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message != wake) return DefWindowProcW(target, message, wParam, lParam);
    guard("toasts wake", [] {
        get().drain();
        return S_OK;
    });
    return 0;
}

void Toasts::run() {
    WNDCLASSW type{};
    type.lpfnWndProc = windowProc;
    type.hInstance = module;
    type.lpszClassName = windowClass;
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("RegisterClass failed");
    HWND created = CreateWindowExW(0, windowClass, folderName, WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, module, nullptr);
    if (!created) throw std::runtime_error("CreateWindowEx failed");
    window = created;
    drain();
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) DispatchMessageW(&message);
}

void Toasts::drain() {
    while (true) {
        Request request;
        {
            std::lock_guard guard(lock);
            if (pending.empty()) return;
            request = std::move(pending.front());
            pending.pop_front();
        }
        send(request);
    }
}

void Toasts::send(const Request& request) {
    NOTIFYICONDATAW data = {sizeof data};
    data.hWnd = window;
    data.uID = iconId;
    if (request.hide) {
        if (added) Shell_NotifyIconW(NIM_DELETE, &data);
        added = false;
        return;
    }
    if (!added) {
        data.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        data.hIcon = blockIcon(GetSystemMetrics(SM_CXSMICON), state(2, 0));
        StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip), folderName);
        added = Shell_NotifyIconW(NIM_ADD, &data);
        DestroyIcon(data.hIcon);
        if (!added) return log("toast icon could not be added");
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_USER;
    StringCchCopyW(data.szInfoTitle, ARRAYSIZE(data.szInfoTitle), folderName);
    StringCchCopyW(data.szInfo, ARRAYSIZE(data.szInfo), request.text.c_str());
    if (!Shell_NotifyIconW(NIM_MODIFY, &data)) log("toast could not be shown");
}

}
