#pragma once

#include "common.h"

#include <deque>
#include <mutex>

namespace mcx {

class Toasts {
public:
    static Toasts& get();

    void show(const std::wstring& text);
    void hide();

private:
    Toasts();

    struct Request {
        bool hide;
        std::wstring text;
    };

    std::mutex lock;
    std::deque<Request> pending;
    std::atomic<HWND> window = nullptr;
    bool added = false;

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void run();
    void drain();
    void send(const Request& request);
};

}
