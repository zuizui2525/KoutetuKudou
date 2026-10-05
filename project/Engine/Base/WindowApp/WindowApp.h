#pragma once
#include <Windows.h>
#include <string>
#include <functional>

class WindowApp {
public:
    // 定数
    static const int32_t kClientWidth = 1280;
    static const int32_t kClientHeight = 720;

    // コンストラクタ・デストラクタ
    WindowApp();
    ~WindowApp();

    // 初期化・表示
    bool Initialize(const wchar_t* title, const int32_t width = kClientWidth, const int32_t height = kClientHeight);
    void Show();

    // メッセージループ
    bool ProcessMessage();

    // 終了ハンドラ設定
    void SetCloseHandler(std::function<bool()> handler) { closeHandler_ = handler; }

    // getter
    HWND GetHWND() const { return hwnd_; }
    HINSTANCE GetInstance() const { return wc_.hInstance; }
    RECT GetClientRect() const { return wrc_; }

private:
    // ウィンドウプロシージャ（静的）
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    // 終了前ハンドラ（静的）
    static inline std::function<bool()> closeHandler_ = nullptr;

    // 内部データ
    HWND hwnd_ = nullptr;
    RECT wrc_ = { 0, 0, kClientWidth, kClientHeight };
    WNDCLASS wc_ = {};
};
