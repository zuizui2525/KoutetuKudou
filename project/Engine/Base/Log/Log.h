#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <format>
#include <Windows.h>
#include <vector>
#include <deque>

enum class LogType {
    Info,
    Warning,
    Error
};

struct LogEntry {
    std::string fullMessage; // タイムスタンプ付きのフルメッセージ
    float timestamp = 0.0f;   // 記録時のタイムスタンプ
    LogType type = LogType::Info;
};

class Log {
public:
    Log() = default;
    ~Log() = default;

    // 出力関数（ファイル＋デバッグ出力）
    static void Write(const std::string& message);
    static void Write(const std::wstring& message);

    // ostream（例：std::cout）にも出力
    static void Write(std::ostream& os, const std::string& message);
    static void Write(std::ostream& os, const std::wstring& message);

    // ImGui Console用ゲッターとクリア関数
    static std::vector<std::string> GetLogMessages();
    static void ClearLog();

    // ImGui用描画関数
    static void DrawConsoleWindow(float maxTimestamp = -1.0f, const char* windowIdSuffix = nullptr);

    // システム環境情報の出力
    static void LogSystemInfo();

    // 起動からの経過時間を秒単位で取得
    static float GetElapsedTime();


    // ゲーム内稼働タイマーの更新
    static void Update(float deltaTime);

    // コンソール表示フラグのポインタ取得
    static bool* GetShowConsolePtr();


private:
    static void Initialize();

    static std::ofstream logStream_;   // ログファイル出力用
    static std::string logFileName_;   // ログファイル名
    static bool isInitialized_;         // 初期化フラグ
    static std::chrono::steady_clock::time_point startTime_; // 起動時の基準時間
    static float activeElapsedTime_;                          // ポーズ中進まない経過時間

    static std::deque<LogEntry> logEntries_; // 蓄積されたログ
    static constexpr size_t kMaxLogLines = 500;   // 最大保持行数

    static bool showConsole_;                     // コンソールウィンドウの表示フラグ
    static size_t lastLogSize_;                   // 自動スクロール追跡用のログサイズ
};
