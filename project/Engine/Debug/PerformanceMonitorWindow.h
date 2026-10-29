#ifdef _USEIMGUI
#pragma once
#include <chrono>

class PerformanceMonitorWindow {
public:
    PerformanceMonitorWindow();
    ~PerformanceMonitorWindow() = default;

    void Draw(bool* show, const char* windowIdSuffix = nullptr);

private:
    // 履歴サイズ（マジックナンバー排除）
    static constexpr int kHistorySize = 120;
    static constexpr float kDefaultMaxFps = 75.0f;
    static constexpr float kDefaultMaxMem = 128.0f;

    // 履歴配列
    float fpsHistory_[kHistorySize] = {};
    float midpointFpsHistory_[kHistorySize] = {};
    float memHistory_[kHistorySize] = {};
    int historyOffset_ = 0;

    // 測定用変数
    float minObservedFps_ = -1.0f;
    float maxObservedFps_ = -1.0f;
    int frameCount_ = 0;

    std::chrono::steady_clock::time_point lastFrameTime_;
};
#endif
