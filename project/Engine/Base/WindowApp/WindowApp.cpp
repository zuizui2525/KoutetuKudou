#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "imgui_impl_win32.h"
#include <cassert>
#include <format>
#pragma comment(lib, "winmm.lib")
 
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
 
WindowApp::WindowApp() {}
WindowApp::~WindowApp() {
    if (hwnd_) {
        CloseWindow(hwnd_);
    }
}
 
bool WindowApp::Initialize(const wchar_t* title, int32_t width, int32_t height) {
    timeBeginPeriod(1);
    wrc_ = { 0, 0, width, height };
 
    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = L"MyWindowClass";
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
 
    RegisterClass(&wc_);
 
    UINT windowStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
 
    AdjustWindowRect(&wrc_, windowStyle, false);
 
    hwnd_ = CreateWindow(
        wc_.lpszClassName,
        title,
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc_.right - wrc_.left,
        wrc_.bottom - wrc_.top,
        nullptr,
        nullptr,
        wc_.hInstance,
        nullptr
    );
 
    assert(hwnd_ != nullptr);
    if (hwnd_ != nullptr) {
        Log::Write(std::format(L" ├─ 【ウィンドウ生成成功】 タイトル:「{}」 | 解像度: {} x {}", title, width, height));
    }
    return hwnd_ != nullptr;
}

void WindowApp::Show() {
    ShowWindow(hwnd_, SW_SHOW);
}

bool WindowApp::ProcessMessage() {
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

LRESULT CALLBACK WindowApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // ImGui用の処理を先に通す（ここ重要！）
#ifdef _USEIMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
#endif

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

