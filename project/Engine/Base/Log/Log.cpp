#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "imgui.h"
#include <intrin.h>

#ifdef _USEIMGUI
#endif

// 静的変数の実体定義
std::ofstream Log::logStream_;
std::string Log::logFileName_;
bool Log::isInitialized_ = false;
std::chrono::steady_clock::time_point Log::startTime_;
float Log::activeElapsedTime_ = 0.0f;

std::deque<LogEntry> Log::logEntries_;
bool Log::showConsole_ = true;
size_t Log::lastLogSize_ = 0;

void Log::Initialize() {
    if (isInitialized_) return;
    isInitialized_ = true;
    startTime_ = std::chrono::steady_clock::now();

    // ディレクトリ作成
    std::filesystem::create_directory("logs");

    // 現在時間取得（秒単位に丸める）
    auto now = std::chrono::system_clock::now();
    auto nowSec = std::chrono::time_point_cast<std::chrono::seconds>(now);
    std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSec };

    // ファイル名作成
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    logFileName_ = "logs/" + dateString + ".log";

    // ファイルオープン
    logStream_.open(logFileName_, std::ios::out);
    if (!logStream_) {
        OutputDebugStringW(L"Log file could not be created.\n");
    } else {
        std::wstring wLogFile = ConvertString(logFileName_);
        OutputDebugStringW((L"Log file created: " + wLogFile + L"\n").c_str());
    }
}

void Log::Write(const std::string& message) {
    if (!isInitialized_) {
        Initialize();
    }

    // 入力メッセージ末尾の改行コードを削除
    std::string cleanMessage = message;
    while (!cleanMessage.empty() && (cleanMessage.back() == '\n' || cleanMessage.back() == '\r')) {
        cleanMessage.pop_back();
    }

    // 空メッセージの場合は改行のみ出力して終了
    if (cleanMessage.empty()) {
        if (logStream_.is_open()) {
            logStream_ << std::endl;
        }
        OutputDebugStringW(L"\n");
        return;
    }

    // 起動からの経過時間を算出
    float realElapsedTime = 0.0f;
    if (isInitialized_) {
        auto now = std::chrono::steady_clock::now();
        realElapsedTime = std::chrono::duration<float>(now - startTime_).count();
    }

    float elapsedSeconds = activeElapsedTime_;
    long long totalMs = static_cast<long long>(elapsedSeconds * 1000.0f);

    // 時・分・秒・ミリ秒を計算
    long long ms = totalMs % 1000;
    long long totalSec = totalMs / 1000;
    long long s = totalSec % 60;
    long long totalMin = totalSec / 60;
    long long m = totalMin % 60;
    long long h = totalMin / 60;

    // 1分毎（経過）の区切り文字を出力
    static long long lastMin = 0;
    if (totalMin > lastMin) {
        lastMin = totalMin;
        
        std::string markerStr = std::format("\n[real|{:7.3f}s] [game|{:02d}h:{:02d}m:00.000s] -------------------- 【 経過 {}分 】 --------------------\n", realElapsedTime, h, m, totalMin);
        std::wstring wMarkerStr = ConvertString(markerStr);

        if (logStream_.is_open()) {
            logStream_ << markerStr << std::endl;
        }
        OutputDebugStringW(wMarkerStr.c_str());
    }

    // タイムスタンプ文字列を作成
    std::string timeStr = std::format("[real|{:7.3f}s] [game|{:02d}h:{:02d}m:{:02d}.{:03d}s] ", realElapsedTime, h, m, s, ms);
    std::string fullMessage = timeStr + cleanMessage;

    // ログタイプ判定
    LogType type = LogType::Info;
    if (cleanMessage.find("error") != std::string::npos || cleanMessage.find("failed") != std::string::npos || 
        cleanMessage.find("Error") != std::string::npos || cleanMessage.find("エラー") != std::string::npos || 
        cleanMessage.find("ERROR") != std::string::npos) {
        type = LogType::Error;
    } else if (cleanMessage.find("warning") != std::string::npos || cleanMessage.find("Warning") != std::string::npos || 
               cleanMessage.find("警告") != std::string::npos || cleanMessage.find("WARNING") != std::string::npos) {
        type = LogType::Warning;
    }

    LogEntry entry;
    entry.fullMessage = fullMessage;
    entry.timestamp = elapsedSeconds;
    entry.type = type;

    // リングバッファへの追加
    if (logEntries_.size() >= kMaxLogLines) {
        logEntries_.pop_front();
    }
    logEntries_.push_back(entry);

    if (logStream_.is_open()) {
        logStream_ << fullMessage << std::endl;
    }

    // デバッグ出力
    std::wstring wMsg = ConvertString(fullMessage);
    OutputDebugStringW((wMsg + L"\n").c_str());
}

void Log::Write(std::ostream& os, const std::string& message) {
    os << message;
    if (message.empty() || message.back() != '\n') {
        os << std::endl;
    }
    Write(message);
}

void Log::Write(const std::wstring& message) {
    Write(ConvertString(message));
}

void Log::Write(std::ostream& os, const std::wstring& message) {
    Write(os, ConvertString(message));
}

std::vector<std::string> Log::GetLogMessages() {
    std::vector<std::string> messages;
    messages.reserve(logEntries_.size());
    for (const auto& entry : logEntries_) {
        messages.push_back(entry.fullMessage);
    }
    return messages;
}

void Log::ClearLog() {
    logEntries_.clear();
}

void Log::DrawConsoleWindow(float maxTimestamp) {
#ifdef _USEIMGUI
    if (!showConsole_) return;

    if (ImGui::Begin("コンソール###Console", &showConsole_)) {
        static bool filterReplayRangeOnly = false;

        // リプレイ範囲フィルター UI（ReplaySystem削除に伴い常に無効化表示）
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Checkbox("リプレイ範囲のみ", &filterReplayRangeOnly);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[リプレイ機能削除済み]");

        ImGui::Separator();

        // スクロール可能な子ウィンドウ
        ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        // リアルタイム起動経過時間
        float realTime = 0.0f;
        if (isInitialized_) {
            auto now = std::chrono::steady_clock::now();
            realTime = std::chrono::duration<float>(now - startTime_).count();
        }

        // タイムスタンプ文字列フォーマット用ラムダ式
        auto FormatPlainGameTimestamp = [](float seconds) -> std::string {
            long long totalMs = static_cast<long long>(seconds * 1000.0f);
            long long ms = totalMs % 1000;
            long long totalSec = totalMs / 1000;
            long long s = totalSec % 60;
            long long totalMin = totalSec / 60;
            long long m = totalMin % 60;
            long long h = totalMin / 60;
            return std::format("{:02d}h:{:02d}m:{:02d}.{:03d}s", h, m, s, ms);
        };
        auto FormatRealTimestamp = [](float seconds) -> std::string {
            return std::format("[real|{:7.3f}s] ", seconds);
        };
        auto FormatGameTimestamp = [&](float seconds) -> std::string {
            return "[game|" + FormatPlainGameTimestamp(seconds) + "] ";
        };

        // 現在時間 (currentTime) を決定 (通常再生時は現在の経過時間を使用)
        float currentTime = maxTimestamp;
        if (currentTime < 0.0f) {
            currentTime = GetElapsedTime();
        }

        // 最も近いログ行の検索
        size_t nearestIdx = static_cast<size_t>(-1);
        float minDiff = 10000.0f;
        if (!logEntries_.empty()) {
            for (size_t i = 0; i < logEntries_.size(); ++i) {
                float diff = std::abs(currentTime - logEntries_[i].timestamp);
                if (diff < minDiff) {
                    minDiff = diff;
                    nearestIdx = i;
                }
            }
        }

        // 被り判定の閾値 (0.15秒)
        constexpr float kOverlapThreshold = 0.15f;
        bool isOverlapped = (nearestIdx != static_cast<size_t>(-1) && minDiff <= kOverlapThreshold);

        // 仮想タイマー行の挿入位置決定
        size_t insertAfterIdx = static_cast<size_t>(-1);
        if (!logEntries_.empty()) {
            if (currentTime < logEntries_[0].timestamp) {
                insertAfterIdx = static_cast<size_t>(-2); // 最初のログの前に挿入する特殊フラグ
            } else {
                for (size_t i = 0; i < logEntries_.size(); ++i) {
                    if (logEntries_[i].timestamp <= currentTime) {
                        insertAfterIdx = i;
                    } else {
                        break;
                    }
                }
            }
        }

        // 最初のログより前にタイマーを挿入する場合
        if (!isOverlapped && insertAfterIdx == static_cast<size_t>(-2)) {
            std::string timerStr = "--> " + FormatRealTimestamp(realTime) + FormatGameTimestamp(currentTime) + "------ 【進行中】 ------";
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            ImGui::TextUnformatted(timerStr.c_str());
            ImGui::PopStyleColor();
        }

        for (size_t i = 0; i < logEntries_.size(); ++i) {
            const auto& entry = logEntries_[i];
            
            constexpr float kMaxReplayTimeRange = 60.0f;
            bool shouldFade = (currentTime - entry.timestamp > kMaxReplayTimeRange);

            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // デフォルト白
            bool isCurrentActive = (isOverlapped && i == nearestIdx);

            // ログタイプに応じた色の決定
            if (entry.type == LogType::Error) {
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // エラー：マイルドレッド
            } else if (entry.type == LogType::Warning) {
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // 警告：マイルドイエロー
            } else {
                color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // 一般・システム：マイルドグリーン
            }

            if (isCurrentActive) {
                color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // 被ったログは赤
            } else if (shouldFade) {
                color.w = 0.4f; // 古いログは半透明化
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            
            if (isCurrentActive) {
                std::string markedLog = "--> " + entry.fullMessage;
                ImGui::TextUnformatted(markedLog.c_str());

                static size_t lastActiveIndex = static_cast<size_t>(-1);
                if (nearestIdx != lastActiveIndex) {
                    ImGui::SetScrollHereY(0.5f); // 画面中央にスクロール
                    lastActiveIndex = nearestIdx;
                }
            } else {
                ImGui::TextUnformatted(entry.fullMessage.c_str());
            }

            ImGui::PopStyleColor();

            // ログの行間に仮想タイマーを挿入する場合
            if (!isOverlapped && insertAfterIdx == i) {
                std::string timerStr = "--> " + FormatRealTimestamp(realTime) + FormatGameTimestamp(currentTime) + "------ 【進行中】 ------";
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                ImGui::TextUnformatted(timerStr.c_str());
                ImGui::PopStyleColor();

                static size_t lastInsertIdx = static_cast<size_t>(-1);
                if (insertAfterIdx != lastInsertIdx) {
                    ImGui::SetScrollHereY(0.5f); // 画面中央にスクロール
                    lastInsertIdx = insertAfterIdx;
                }
            }
        }

        // 新規ログ追加時に自動で最下部へスクロール
        bool shouldScroll = (maxTimestamp < 0.0f);
        if (shouldScroll && !isOverlapped && logEntries_.size() > lastLogSize_) {
            ImGui::SetScrollHereY(1.0f);
            lastLogSize_ = logEntries_.size();
        }
        
        ImGui::EndChild();
    }
    ImGui::End();
#endif
}

void Log::Update(float deltaTime) {
    if (!isInitialized_) {
        Initialize();
    }
    activeElapsedTime_ += deltaTime;
}

float Log::GetElapsedTime() {
    if (!isInitialized_) return 0.0f;
    return activeElapsedTime_;
}

bool* Log::GetShowConsolePtr() {
    return &showConsole_;
}

void Log::LogSystemInfo() {
    Log::Write(" ========================================= [システム環境情報] =========================================");

    // 1. OS情報の取得 (RtlGetVersion を動的リンク)
    std::wstring osInfoStr = L"Unknown Windows Version";
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        typedef LONG(WINAPI* fnRtlGetVersion)(PRTL_OSVERSIONINFOW);
        fnRtlGetVersion pRtlGetVersion = reinterpret_cast<fnRtlGetVersion>(GetProcAddress(hNtdll, "RtlGetVersion"));
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW osvi = {};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (pRtlGetVersion(&osvi) == 0) { // STATUS_SUCCESS
                osInfoStr = std::format(L"Windows {}.{} (Build {})", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
            }
        }
    }
    Log::Write(std::format(L" ├─ 【OSバージョン】 {}", osInfoStr));

    // 2. CPU情報の取得 (__cpuid)
    constexpr size_t kCpuBrandLength = 49;
    char cpuBrand[kCpuBrandLength] = { 0 };
    int cpuInfo[4] = { 0 };
    
    constexpr int kCpuIdBrandPart1 = 0x80000002;
    constexpr int kCpuIdBrandPart2 = 0x80000003;
    constexpr int kCpuIdBrandPart3 = 0x80000004;
    constexpr size_t kInfoChunkSize = sizeof(cpuInfo);

    __cpuid(cpuInfo, kCpuIdBrandPart1);
    std::memcpy(cpuBrand, cpuInfo, kInfoChunkSize);
    __cpuid(cpuInfo, kCpuIdBrandPart2);
    std::memcpy(cpuBrand + 16, cpuInfo, kInfoChunkSize);
    __cpuid(cpuInfo, kCpuIdBrandPart3);
    std::memcpy(cpuBrand + 32, cpuInfo, kInfoChunkSize);

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    std::string cpuStr(cpuBrand);
    
    // 前後の空白をトリム
    constexpr char kWhitespaceChars[] = " ";
    cpuStr.erase(0, cpuStr.find_first_not_of(kWhitespaceChars));
    cpuStr.erase(cpuStr.find_last_not_of(kWhitespaceChars) + 1);

    Log::Write(std::format(L" ├─ 【CPU】 {} ({} threads)", ConvertString(cpuStr), sysInfo.dwNumberOfProcessors));

    // 3. メモリ容量の取得
    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        constexpr float kBytesToGB = 1024.0f * 1024.0f * 1024.0f;
        float totalPhysMemGB = static_cast<float>(memStatus.ullTotalPhys) / kBytesToGB;
        float availPhysMemGB = static_cast<float>(memStatus.ullAvailPhys) / kBytesToGB;
        Log::Write(std::format(L" ├─ 【システムメモリ(RAM)】 総容量: {:.2f} GB | 空き容量: {:.2f} GB", totalPhysMemGB, availPhysMemGB));
    }

    // 4. 実行環境・ビルド情報
    std::string buildConfig = "Release";
#ifdef _DEBUG
    buildConfig = "Debug";
#endif
    Log::Write(std::format(L" ├─ 【ビルド構成】 {}", ConvertString(buildConfig)));

    std::wstring currentPath = std::filesystem::current_path().wstring();
    Log::Write(std::format(L" ├─ 【作業ディレクトリ】 {}", currentPath));

    Log::Write(" ======================================================================================================");
}
