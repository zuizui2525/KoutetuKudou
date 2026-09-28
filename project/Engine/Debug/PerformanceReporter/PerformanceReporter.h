#pragma once
#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_4.h> // DXGI 1.4 for QueryVideoMemoryInfo
#include <psapi.h>    // for GetProcessMemoryInfo

#include "SharedPerfData.h"

class PerformanceReporter {
public:
    // 統計ログに記録するFPSデータのエントリ
    struct PerfLogEntry {
        float time = 0.0f;
        float fps = 0.0f;
        float memory = 0.0f; // CPUメモリ使用量 (MB)
        float vram = 0.0f;   // GPU VRAM使用量 (MB)
    };

    // 初期化と終了処理
    static void Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, UINT width, UINT height);
    static void Finalize();

    // 毎フレームの更新（自己完結型: 内部でFPS、経過時間、CPU/GPUメモリを計測）
    static void Update();

    // 手動または特定のイベントでパフォーマンスレポートのダンプを実行
    static void TriggerReport(const std::string& reason, const std::string& detail);

    // 描画フレーム完了時のキャプチャ（コマンドリストでVRAMコピーをスケジュール）
    static void CaptureFrame(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* backBuffer, D3D12_RESOURCE_STATES currentState);

    // ロード時間計測用
    static void StartLoadTimer();
    static void EndLoadTimer(const std::string& loadName, float maxAllowedSeconds = 3.0f);

    // 外部からデバッグ用メタデータを動的に登録できるインターフェース
    static void SetMetaData(const std::string& key, const std::string& value);

    // 設定と状態取得
    static void SetFpsDropThreshold(float threshold) { fpsDropThreshold_ = threshold; }
    static float GetFpsDropThreshold() { return fpsDropThreshold_; }
    static void SetEnabled(bool enabled) { isEnabled_ = enabled; }
    static bool IsEnabled() { return isEnabled_; }
    static bool IsDumping() { return isDumping_; }

private:
    // キャプチャされた1フレームのデータ
    struct CapturedFrame {
        Microsoft::WRL::ComPtr<ID3D12Resource> gpuTexture;
        float timestamp = 0.0f;
    };

    // バックグラウンドスレッドに渡すデータ
    struct DumpData {
        std::wstring mp4Path;
        std::wstring pngPath;
        std::string jsonPath;
        std::string promptPath;
        std::string folderName;
        std::vector<std::vector<BYTE>> rawFrames; // MP4用ピクセルバッファ (BGRA)
        std::vector<BYTE> screenshotRgba;         // スクリーンショット用RGBAバッファ
        std::vector<PerfLogEntry> perfLog;
        std::vector<std::string> logMessages;     // 直近 of システムログメッセージ
        std::unordered_map<std::string, std::string> customMetaData; // カスタム登録されたメタデータ
        std::string reason;
        std::string detail;
        std::tm tm_info;
        UINT width = 0;
        UINT height = 0;
    };

    // レポートパッケージの出力（メインスレッド：GPUバッファリードバック）
    static void DumpReportPackage(const std::string& reason, const std::string& detail);

    // 非同期書き出しスレッドのメイン処理
    static void ExecuteDumpThread(DumpData data);

    // MP4およびPNGのファイル書き出しヘルパー
    static void SaveMp4File(const std::wstring& filePath, const std::vector<std::vector<BYTE>>& rawFrames, UINT width, UINT height);
    static void SavePngFile(const std::wstring& filePath, BYTE* rawRgbaData, UINT width, UINT height);

    // 文字列変換ヘルパー
    static std::wstring ConvertToWstring(const std::string& str);

    // 静的メンバー変数
    static ID3D12Device* device_;
    static ID3D12CommandQueue* commandQueue_;
    static Microsoft::WRL::ComPtr<IDXGIAdapter3> dxgiAdapter3_;
    static UINT bufferWidth_;
    static UINT bufferHeight_;


    static float fpsDropThreshold_;
    static bool isEnabled_;
    static bool isTriggeredThisFrame_;
    static float cooldownTimer_;

    // ロード時間計測用
    static std::chrono::steady_clock::time_point loadStartTime_;
    static bool isLoading_;

    // 自己計測用の基準時間と前フレーム時間
    static std::chrono::steady_clock::time_point sessionStartTime_;
    static std::chrono::steady_clock::time_point lastFrameTime_;

    // リプレイ用リングバッファ
    static std::deque<CapturedFrame> frameRingBuffer_;
    static size_t maxRingBufferSize_;
    static float runningTime_;

    // 統計ログバッファ
    static std::deque<PerfLogEntry> perfLog_;

    // 非同期ダンプ用スレッドとステート
    static std::thread dumpThread_;
    static std::atomic<bool> isDumping_;

    // セッション（ゲーム起動）フォルダ名
    static std::string sessionFolderName_;

    // 外部から登録されたカスタムメタデータ
    static std::unordered_map<std::string, std::string> customMetaData_;

    // トリガー制限用カウント
    static int triggerCount_;

    // 移動平均FPS算出用
    static std::deque<float> fpsDeltaHistory_;
    static float fpsDeltaSum_;
    static uint64_t totalFrameCount_;

    // 共有メモリ管理ハンドラ
    static void* hMapFile_;
    static ZuizuiPerf::SharedPerfData* sharedData_;

    // 定数（マジックナンバー排除）
    static constexpr size_t kFpsLogLimit = 300; // 直近の約30秒分（10fps換算）
    static constexpr float kDefaultFpsDropThreshold = 30.0f;
    static constexpr size_t kFpsHistorySampleLimit = 60; // FPS移動平均の最大サンプルフレーム数
};

