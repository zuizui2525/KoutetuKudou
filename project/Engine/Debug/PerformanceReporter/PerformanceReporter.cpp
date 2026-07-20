#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "PerformanceReporter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <format>
#include <algorithm>
#include <iomanip>
#include <shellapi.h> // シェル起動用
#include "Engine/Base/Log/Log.h" // ゲーム内ログ出力用

// Media Foundation 関連の初期化
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// WIC (Windows Imaging Component) 関連の初期化
#include <wincodec.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Windowscodecs.lib")


// 静的メンバ変数の実体化
ID3D12Device* PerformanceReporter::device_ = nullptr;
ID3D12CommandQueue* PerformanceReporter::commandQueue_ = nullptr;
UINT PerformanceReporter::bufferWidth_ = 0;
UINT PerformanceReporter::bufferHeight_ = 0;

float PerformanceReporter::fpsDropThreshold_ = PerformanceReporter::kDefaultFpsDropThreshold;
bool PerformanceReporter::isEnabled_ = true;
bool PerformanceReporter::isTriggeredThisFrame_ = false;
float PerformanceReporter::cooldownTimer_ = 0.0f;

std::chrono::steady_clock::time_point PerformanceReporter::loadStartTime_;
bool PerformanceReporter::isLoading_ = false;

std::chrono::steady_clock::time_point PerformanceReporter::sessionStartTime_;
std::chrono::steady_clock::time_point PerformanceReporter::lastFrameTime_;

std::deque<PerformanceReporter::CapturedFrame> PerformanceReporter::frameRingBuffer_;
size_t PerformanceReporter::maxRingBufferSize_ = 30; // 10fps で 3秒分 (30フレーム)
float PerformanceReporter::runningTime_ = 0.0f;

std::deque<PerformanceReporter::PerfLogEntry> PerformanceReporter::perfLog_;

std::thread PerformanceReporter::dumpThread_;
std::atomic<bool> PerformanceReporter::isDumping_ = false;
std::string PerformanceReporter::sessionFolderName_ = "";
std::unordered_map<std::string, std::string> PerformanceReporter::customMetaData_;

// 追加メンバ変数の実体化
int PerformanceReporter::triggerCount_ = 0;
std::thread PerformanceReporter::liveSyncThread_;
std::atomic<bool> PerformanceReporter::isLiveSyncRunning_ = false;
std::atomic<float> PerformanceReporter::liveFps_ = 0.0f;
std::atomic<float> PerformanceReporter::liveCpu_ = 0.0f;
std::atomic<float> PerformanceReporter::liveVram_ = 0.0f;

namespace {
    // 10fps 間隔でキャプチャするためのタイマー (100msに1回)
    constexpr float kCaptureInterval = 1.0f / 10.0f;
    float s_captureTimer = 0.0f;

    // クールタイム（連続ダンプ防止：15秒に延長してメモリの蓄積を防止）
    constexpr float kCooldownDuration = 15.0f;

    // サーキュラーバッファの書き込みインデックス
    size_t s_writeIndex = 0;

    // 遅延ダンプ用フラグと一時変数
    bool s_shouldDumpNextFrame = false;
    std::string s_triggerReason = "";
    std::string s_triggerDetail = "";
}

// 常駐スレッドの処理本体 (300ms間隔でWinsock通信を実行)
void PerformanceReporter::LiveSyncThreadWork() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return;
    }

    while (isLiveSyncRunning_) {
        // 300ms 待機
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (!isLiveSyncRunning_) break;

        float time = runningTime_;
        float fps = liveFps_.load();
        float cpu = liveCpu_.load();
        float vram = liveVram_.load();

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock != INVALID_SOCKET) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8080);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

            // 送信/受信タイムアウトを 100ms に設定 (ストール防止)
            DWORD timeout = 100;
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR) {
                std::string req = std::format(
                    "GET /update_live_metrics?time={:.3f}&fps={:.2f}&cpu={:.2f}&vram={:.2f} HTTP/1.1\r\n"
                    "Host: localhost:8080\r\n"
                    "Connection: close\r\n\r\n",
                    time, fps, cpu, vram
                );
                send(sock, req.c_str(), (int)req.size(), 0);
                
                // ダミー受信でサーバー側のソケット切断に同調する
                char buf[64];
                recv(sock, buf, sizeof(buf), 0);
            }
            closesocket(sock);
        }
    }

    WSACleanup();
}

void PerformanceReporter::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, UINT width, UINT height) {
    device_ = device;
    commandQueue_ = commandQueue;
    bufferWidth_ = width;
    bufferHeight_ = height;
    isEnabled_ = true;
    isTriggeredThisFrame_ = false;
    cooldownTimer_ = 0.0f;
    runningTime_ = 0.0f;
    s_captureTimer = 0.0f;
    s_writeIndex = 0;
    s_shouldDumpNextFrame = false;
    s_triggerReason = "";
    s_triggerDetail = "";
    isDumping_ = false;

    // トリガー制限の初期化
    triggerCount_ = 0;

    // セッション開始時間の記録
    sessionStartTime_ = std::chrono::steady_clock::now();
    lastFrameTime_ = sessionStartTime_;

    // 起動日時フォルダ名の生成 (run_YYYYMMDD_HHMMSS)
    std::time_t t = std::time(nullptr);
    std::tm tm_info;
    localtime_s(&tm_info, &t);
    char timeBuffer[64];
    std::strftime(timeBuffer, sizeof(timeBuffer), "run_%Y%m%d_%H%M%S", &tm_info);
    sessionFolderName_ = std::string(timeBuffer);

    frameRingBuffer_.clear();
    perfLog_.clear();
    customMetaData_.clear();

    // 常駐ライブ同期スレッドの起動
    isLiveSyncRunning_ = true;
    liveFps_ = 0.0f;
    liveCpu_ = 0.0f;
    liveVram_ = 0.0f;
    liveSyncThread_ = std::thread(LiveSyncThreadWork);
}

void PerformanceReporter::Finalize() {
    // 常駐スレッドの終了
    isLiveSyncRunning_ = false;
    if (liveSyncThread_.joinable()) {
        liveSyncThread_.join();
    }



    if (dumpThread_.joinable()) {
        dumpThread_.join();
    }
    frameRingBuffer_.clear();
    perfLog_.clear();
    customMetaData_.clear();
    device_ = nullptr;
    commandQueue_ = nullptr;
}

void PerformanceReporter::Update() {
    if (!isEnabled_ || !device_) return;

    // 経過時間とデルタタイムの自己計算
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    if (deltaTime < 0.0001f) { deltaTime = 0.0001f; }
    if (deltaTime > 1.0f) { deltaTime = 1.0f; }

    float currentFps = 1.0f / deltaTime;
    runningTime_ = std::chrono::duration<float>(now - sessionStartTime_).count();
    s_captureTimer += deltaTime;

    if (cooldownTimer_ > 0.0f) {
        cooldownTimer_ -= deltaTime;
    }

    // CPU 物理メモリ (WorkingSetSize) の計測
    float cpuMemoryMB = 0.0f;
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        cpuMemoryMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
    }

    // GPU 専用ビデオメモリ (VRAM) の計測 (IDXGIAdapter3 を使用)
    float vramUsageMB = 0.0f;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
        Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
        if (SUCCEEDED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
            Microsoft::WRL::ComPtr<IDXGIAdapter3> dxgiAdapter3;
            if (SUCCEEDED(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgiAdapter3)))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo{};
                if (SUCCEEDED(dxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo))) {
                    vramUsageMB = static_cast<float>(memoryInfo.CurrentUsage) / (1024.0f * 1024.0f);
                }
            }
        }
    }

    // 共有アトミック変数に最新値を格納 (常駐同期スレッドが自動的に回収)
    liveFps_.store(currentFps);
    liveCpu_.store(cpuMemoryMB);
    liveVram_.store(vramUsageMB);

    // 統計ログの記録
    PerfLogEntry entry;
    entry.time = runningTime_;
    entry.fps = currentFps;
    entry.memory = cpuMemoryMB;
    entry.vram = vramUsageMB;
    perfLog_.push_back(entry);

    if (perfLog_.size() > kFpsLogLimit) {
        perfLog_.pop_front();
    }

    // 自動トリガー判定：FPSが閾値を下回った場合
    if (cooldownTimer_ <= 0.0f && currentFps > 0.0f && currentFps < fpsDropThreshold_) {
        std::string detail = std::format("FPS dropped to {:.2f} (Threshold: {:.2f} FPS)", currentFps, fpsDropThreshold_);
        TriggerReport("FPS_DROP", detail);
    }
}

void PerformanceReporter::StartLoadTimer() {
    loadStartTime_ = std::chrono::steady_clock::now();
    isLoading_ = true;
}

void PerformanceReporter::EndLoadTimer(const std::string& loadName, float maxAllowedSeconds) {
    if (!isLoading_) return;
    isLoading_ = false;

    auto endTime = std::chrono::steady_clock::now();
    float loadDuration = std::chrono::duration<float>(endTime - loadStartTime_).count();

    // ロード時間が許容値を超えた場合に自動トリガー
    if (loadDuration > maxAllowedSeconds) {
        std::string detail = std::format("Scene/Area '{}' load took {:.2f} seconds (Max Allowed: {:.2f}s)", loadName, loadDuration, maxAllowedSeconds);
        TriggerReport("LONG_LOAD", detail);
    }
}

void PerformanceReporter::CaptureFrame(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* backBuffer, D3D12_RESOURCE_STATES currentState) {
    if (!isEnabled_ || !device_ || !backBuffer || !cmdList) return;

    D3D12_RESOURCE_DESC srcDesc = backBuffer->GetDesc();

    // 初回実行時、またはコピー元のサイズ・フォーマットが動的に変わった場合、リングバッファ用テクスチャを全再生成
    bool needsRecreate = frameRingBuffer_.empty();
    if (!frameRingBuffer_.empty()) {
        D3D12_RESOURCE_DESC destDesc = frameRingBuffer_[0].gpuTexture->GetDesc();
        if (destDesc.Width != srcDesc.Width || destDesc.Height != srcDesc.Height || destDesc.Format != srcDesc.Format) {
            needsRecreate = true;
        }
    }

    if (needsRecreate) {
        frameRingBuffer_.clear();
        bufferWidth_ = static_cast<UINT>(srcDesc.Width);
        bufferHeight_ = srcDesc.Height;

        for (size_t i = 0; i < maxRingBufferSize_; ++i) {
            CapturedFrame cf;
            cf.timestamp = 0.0f;

            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = bufferWidth_;
            desc.Height = bufferHeight_;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = srcDesc.Format; // コピー元と100%同一のフォーマット
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            HRESULT hr = device_->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&cf.gpuTexture)
            );
            
            if (SUCCEEDED(hr)) {
                frameRingBuffer_.push_back(cf);
            }
        }
        s_writeIndex = 0;
    }

    if (frameRingBuffer_.empty()) return;

    // キャプチャ間隔（10fps）を制御
    if (s_captureTimer >= kCaptureInterval) {
        s_captureTimer = 0.0f;

        // 対象のキャプチャ先リソース
        auto& targetFrame = frameRingBuffer_[s_writeIndex];
        if (targetFrame.gpuTexture) {
            // バリアを張って COPY_SOURCE / COPY_DEST に遷移
            D3D12_RESOURCE_BARRIER barriers[2]{};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = backBuffer;
            barriers[0].Transition.StateBefore = currentState;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource = targetFrame.gpuTexture.Get();
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            cmdList->ResourceBarrier(2, barriers);

            // VRAM間のコピー実行 (超高速、CPU同期待きなし)
            cmdList->CopyResource(targetFrame.gpuTexture.Get(), backBuffer);

            // バリアを戻す
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].Transition.StateAfter = currentState;

            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

            cmdList->ResourceBarrier(2, barriers);

            // タイムスタンプ記録とインデックス進行
            targetFrame.timestamp = runningTime_;
            s_writeIndex = (s_writeIndex + 1) % frameRingBuffer_.size();
        }
    }

    // 遅延ダンプフラグが立っていたら、ここでダンプを実行する
    if (s_shouldDumpNextFrame) {
        s_shouldDumpNextFrame = false;
        DumpReportPackage(s_triggerReason, s_triggerDetail);
    }
}

void PerformanceReporter::TriggerReport(const std::string& reason, const std::string& detail) {
    if (cooldownTimer_ > 0.0f) return;
    if (isDumping_) return; // 既にバックグラウンドでダンプ処理を実行中の場合は無視 (多重スレッドリークを防止)

    // 自動トリガーの場合、セッションあたりの最大回数 (3回) を超えたらトリガーしない (手動トリガー MANUAL は常に許可)
    if (reason != "MANUAL" && triggerCount_ >= 3) {
        return;
    }

    cooldownTimer_ = kCooldownDuration;
    
    if (reason != "MANUAL") {
        triggerCount_++;
    }

    // ゲーム内コンソールへ進捗状況を出力
    Log::Write("[システム] パフォーマンスレポートとMP4動画(直前3秒間)を出力しています（バックグラウンドで非同期保存中）...");

    // フラグを立てて、描画フレームの最後でのダンプ処理実行をスケジュールする
    s_shouldDumpNextFrame = true;
    s_triggerReason = reason;
    s_triggerDetail = detail;
}

void PerformanceReporter::SetMetaData(const std::string& key, const std::string& value) {
    customMetaData_[key] = value;
}

void PerformanceReporter::SavePngFile(const std::wstring& filePath, BYTE* rawRgbaData, UINT width, UINT height) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return;

    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) return;

    hr = stream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) return;

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, NULL, &encoder);
    if (FAILED(hr)) return;

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return;

    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frameEncode;
    hr = encoder->CreateNewFrame(&frameEncode, NULL);
    if (FAILED(hr)) return;

    hr = frameEncode->Initialize(NULL);
    if (FAILED(hr)) return;

    hr = frameEncode->SetSize(width, height);
    if (FAILED(hr)) return;

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    hr = frameEncode->SetPixelFormat(&format);
    if (FAILED(hr)) return;

    // RGBA データの書き込み
    UINT stride = width * 4;
    UINT bufferSize = stride * height;
    hr = frameEncode->WritePixels(height, stride, bufferSize, rawRgbaData);
    if (FAILED(hr)) return;

    hr = frameEncode->Commit();
    if (FAILED(hr)) return;

    hr = encoder->Commit();
    if (FAILED(hr)) return;
}

void PerformanceReporter::DumpReportPackage(const std::string& reason, const std::string& detail) {
    // 多重ダンプを防止 (二重の防衛壁)
    if (isDumping_) {
        Log::Write("[システム] 現在パフォーマンスレポートをバックグラウンドで出力中です。新たなリクエストは無視されます。");
        return;
    }

    // 1. ディレクトリ生成
    std::time_t t = std::time(nullptr);
    std::tm tm_info;
    localtime_s(&tm_info, &t);
    char folderBuffer[64];
    std::strftime(folderBuffer, sizeof(folderBuffer), "%Y%m%d_%H%M%S", &tm_info);
    
    // out/performance_reports/run_YYYYMMDD_HHMMSS/report_YYYYMMDD_HHMMSS
    std::string folderName = "out/performance_reports/" + sessionFolderName_ + "/report_" + std::string(folderBuffer);
    
    std::filesystem::create_directories(folderName);

    // 古い順に並べ替えたテクスチャ配列を作成
    size_t ringSize = frameRingBuffer_.size();
    std::vector<CapturedFrame> orderedFrames;
    orderedFrames.reserve(ringSize);
    for (size_t i = 0; i < ringSize; ++i) {
        size_t idx = (s_writeIndex + i) % ringSize;
        if (frameRingBuffer_[idx].gpuTexture) {
            orderedFrames.push_back(frameRingBuffer_[idx]);
        }
    }

    if (orderedFrames.empty()) return;

    // GPUからのリードバック処理を一括実行
    UINT rowPitch = (bufferWidth_ * 4 + 255) & ~255;
    UINT64 bufferSize = rowPitch * bufferHeight_;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> readbackBuffers(orderedFrames.size());

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
    device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = bufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (size_t i = 0; i < orderedFrames.size(); ++i) {
        auto& frame = orderedFrames[i];
        if (!frame.gpuTexture) continue;

        device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&readbackBuffers[i])
        );

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = frame.gpuTexture.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &barrier);

        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = frame.gpuTexture.Get();
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLocation.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = readbackBuffers[i].Get();
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLocation.PlacedFootprint.Offset = 0;
        dstLocation.PlacedFootprint.Footprint.Width = bufferWidth_;
        dstLocation.PlacedFootprint.Footprint.Height = bufferHeight_;
        dstLocation.PlacedFootprint.Footprint.Depth = 1;
        dstLocation.PlacedFootprint.Footprint.Format = frame.gpuTexture->GetDesc().Format;
        dstLocation.PlacedFootprint.Footprint.RowPitch = rowPitch;

        cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        cmdList->ResourceBarrier(1, &barrier);
    }

    cmdList->Close();
    ID3D12CommandList* lists[] = { cmdList.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);

    // 一括で GPU 完了待機
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    commandQueue_->Signal(fence.Get(), 1);
    HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
    if (fence->GetCompletedValue() < 1) {
        fence->SetEventOnCompletion(1, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
    }
    CloseHandle(eventHandle);

    // CPU メモリへ一括マップコピー
    std::vector<std::vector<BYTE>> rawFrames(orderedFrames.size());
    for (size_t i = 0; i < orderedFrames.size(); ++i) {
        rawFrames[i].resize(bufferWidth_ * bufferHeight_ * 4);
        void* mappedData = nullptr;
        D3D12_RANGE readRange{ 0, rowPitch * bufferHeight_ };
        if (SUCCEEDED(readbackBuffers[i]->Map(0, &readRange, &mappedData))) {
            BYTE* srcBytes = reinterpret_cast<BYTE*>(mappedData);
            for (UINT y = 0; y < bufferHeight_; ++y) {
                std::memcpy(
                    rawFrames[i].data() + (y * bufferWidth_ * 4),
                    srcBytes + (y * rowPitch),
                    bufferWidth_ * 4
                );
            }
            readbackBuffers[i]->Unmap(0, nullptr);
        }
    }

    // スレッドデータの作成とログ引き渡し (スレッド安全にメインスレッドで取得)
    DumpData data;
    data.mp4Path = ConvertToWstring(folderName + "/replay.mp4");
    data.pngPath = ConvertToWstring(folderName + "/screenshot.png");
    data.jsonPath = folderName + "/system_log.json";
    data.promptPath = folderName + "/prompt.md";
    data.folderName = folderName;
    data.rawFrames = std::move(rawFrames);
    if (!data.rawFrames.empty()) {
        data.screenshotRgba = data.rawFrames.back();
    }
    data.perfLog.assign(perfLog_.begin(), perfLog_.end());
    data.logMessages = Log::GetLogMessages(); // システムログを取得して引き渡す
    data.customMetaData = customMetaData_;     // 登録されたメタデータの引き渡し
    data.reason = reason;
    data.detail = detail;
    data.tm_info = tm_info;
    data.width = bufferWidth_;
    data.height = bufferHeight_;

    // 以前のスレッドが生存していれば、join して破棄する
    if (dumpThread_.joinable()) {
        dumpThread_.join();
    }

    isDumping_ = true;
    dumpThread_ = std::thread(ExecuteDumpThread, std::move(data));
}

void PerformanceReporter::ExecuteDumpThread(DumpData data) {
    // 1. MP4ビデオのエンコード出力
    SaveMp4File(data.mp4Path, data.rawFrames, data.width, data.height);

    // 2. 最新の1フレームを screenshot.png として保存
    if (!data.screenshotRgba.empty()) {
        SavePngFile(data.pngPath, data.screenshotRgba.data(), data.width, data.height);
    }

    // 3. system_log.json の出力 (一時ファイルを経由したアトミックな置換)
    std::string tmpJsonPath = data.jsonPath + ".tmp";
    std::ofstream jsonOfs(tmpJsonPath);
    if (jsonOfs.is_open()) {
        jsonOfs << "{\n";
        jsonOfs << "  \"reason\": \"" << data.reason << "\",\n";
        jsonOfs << "  \"detail\": \"" << data.detail << "\",\n";
        char timeBuffer[64];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &data.tm_info);
        jsonOfs << "  \"time_triggered\": \"" << timeBuffer << "\",\n";
        jsonOfs << "  \"resolution\": \"" << data.width << "x" << data.height << "\",\n";
        jsonOfs << "  \"logs\": [\n";

        for (size_t i = 0; i < data.perfLog.size(); ++i) {
            jsonOfs << "    {\n";
            jsonOfs << "      \"time\": " << std::fixed << std::setprecision(3) << data.perfLog[i].time << ",\n";
            jsonOfs << "      \"fps\": " << std::fixed << std::setprecision(2) << data.perfLog[i].fps << ",\n";
            jsonOfs << "      \"memory_mb\": " << std::fixed << std::setprecision(2) << data.perfLog[i].memory << ",\n";
            jsonOfs << "      \"vram_mb\": " << std::fixed << std::setprecision(2) << data.perfLog[i].vram << "\n";
            jsonOfs << "    }" << (i == data.perfLog.size() - 1 ? "" : ",") << "\n";
        }

        jsonOfs << "  ]\n";
        jsonOfs << "}\n";
        jsonOfs.close();

        // 正常に書き込み完了したら、目的のファイル名にアトミック置換
        std::error_code ec;
        std::filesystem::rename(tmpJsonPath, data.jsonPath, ec);
        if (ec) {
            std::filesystem::copy_file(tmpJsonPath, data.jsonPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpJsonPath, ec);
        }
    }

    // ログから低速処理や警告を抽出
    std::vector<std::string> heavyLoads;
    std::vector<std::string> warningsAndErrors;

    for (const auto& logMsg : data.logMessages) {
        if (logMsg.find("低速ロード") != std::string::npos || logMsg.find("ロード完了") != std::string::npos || 
            logMsg.find("コンパイル完了") != std::string::npos) {
            heavyLoads.push_back(logMsg);
        }
        if (logMsg.find("[警告]") != std::string::npos || logMsg.find("error") != std::string::npos || 
            logMsg.find("failed") != std::string::npos || logMsg.find("エラー") != std::string::npos ||
            logMsg.find("ERROR") != std::string::npos || logMsg.find("WARNING") != std::string::npos) {
            if (logMsg.find("パフォーマンスレポートとMP4動画") == std::string::npos) {
                warningsAndErrors.push_back(logMsg);
            }
        }
    }

    auto CleanLogPrefix = [](const std::string& log) -> std::string {
        size_t idx = log.find("├─ ");
        if (idx == std::string::npos) {
            idx = log.find("└─ ");
        }
        if (idx != std::string::npos) {
            return log.substr(idx + 5);
        }

        idx = log.find("[警告]");
        if (idx != std::string::npos) {
            return log.substr(idx);
        }

        size_t gameIdx = log.find("] ");
        if (gameIdx != std::string::npos) {
            size_t secondClose = log.find("] ", gameIdx + 2);
            if (secondClose != std::string::npos) {
                return log.substr(secondClose + 2);
            }
        }
        return log;
    };

    // 4. prompt.md (LLM 解析依頼用プロンプト) の出力
    std::ofstream promptOfs(data.promptPath);
    if (promptOfs.is_open()) {
        promptOfs << "あなたはC++およびDirectX12ゲームエンジンのパフォーマンス最適化の超一流エキスパートエンジニアです。\n";
        promptOfs << "以下の実行時パフォーマンスデータおよび添付されたリプレイ画像をもとに、FPS低下が発生した原因を推測し、考えられるボトルネックの特定と具体的なコードレベルでの修正案（最適化案）を日本語で提示してください。\n\n";
        
        promptOfs << "# 【AI解析依頼】パフォーマンス低下スパイクの調査\n\n";
        promptOfs << "## 1. 発生時の詳細コンテキスト\n";
        promptOfs << "- **検知トリガー理由**: `" << data.reason << "`\n";
        promptOfs << "- **詳細情報**: " << data.detail << "\n";
        char timeBuffer[64];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &data.tm_info);
        promptOfs << "- **発生日時**: " << timeBuffer << "\n";
        promptOfs << "- **画面解像度**: " << data.width << "x" << data.height << "\n";
        
        if (!data.customMetaData.empty()) {
            for (const auto& [key, value] : data.customMetaData) {
                promptOfs << "- **" << key << "**: " << value << "\n";
            }
        }
        promptOfs << "\n";

        promptOfs << "## 2. パフォーマンスログデータ (時系列)\n";
        promptOfs << "| 経過時間 (秒) | FPS | CPUメモリ (MB) | GPU VRAM (MB) |\n";
        promptOfs << "| :--- | :--- | :--- | :--- |\n";
        
        size_t step = std::max<size_t>(1, data.perfLog.size() / 30);
        for (size_t i = 0; i < data.perfLog.size(); i += step) {
            promptOfs << "| " << std::fixed << std::setprecision(2) << data.perfLog[i].time << "s | "
                      << std::fixed << std::setprecision(1) << data.perfLog[i].fps << " | "
                      << std::fixed << std::setprecision(1) << data.perfLog[i].memory << " MB | "
                      << std::fixed << std::setprecision(1) << data.perfLog[i].vram << " MB |\n";
        }
        promptOfs << "\n";

        promptOfs << "## 3. 同梱のリプレイデータについて\n";
        promptOfs << "- スパイク発生の直前3秒間を記録したMP4動画ファイル（`replay.mp4`）および静止画（`screenshot.bmp`）がこのフォルダに保存されています。\n";
        promptOfs << "- ビューワー上で動画として再生・シーク可能です。\n\n";

        promptOfs << "## 4. 直近のログから検出された具体的な処理・警告（ボトルネック of ヒント）\n";
        if (!heavyLoads.empty()) {
            promptOfs << "### 高負荷なロード・コンパイル処理 (0.1秒以上)\n";
            for (const auto& loadLog : heavyLoads) {
                promptOfs << "- " << loadLog << "\n";
            }
            promptOfs << "\n";
        } else {
            promptOfs << "### 高負荷なロード・コンパイル処理\n- ログ上に0.1秒以上かかったロードやコンパイル処理は検出されませんでした。\n\n";
        }

        if (!warningsAndErrors.empty()) {
            promptOfs << "### 警告・エラーログ\n";
            for (const auto& warnLog : warningsAndErrors) {
                promptOfs << "- " << warnLog << "\n";
            }
            promptOfs << "\n";
        } else {
            promptOfs << "### 警告・エラーログ\n- ログ上に警告やエラーは検出されませんでした。\n\n";
        }

        promptOfs << "## 5. あなた（LLM）への調査指示事項\n";
        promptOfs << "以下の仮説とポイントを重点的に検証してください：\n";
        promptOfs << "1. **CPU/GPUボトルネックの判定**: 時系列ログでFPSが落ち込んでいる瞬間、メモリ使用量（CPU/GPU）に急激なスパイクやリークは見られますか？\n";
        promptOfs << "2. **描画負荷との連動**: 添付された画像・動画において、FPSが低下しているフレームに「大量のオブジェクト」「パーティクルの密集」「特定のUI」などが描画されていませんか？\n";
        
        if (!heavyLoads.empty()) {
            promptOfs << "3. **検出された具体的な高負荷アセットの最適化検討**: 実行ログより、特に以下のファイルロード・処理においてストールが発生していることが確認されました。これらのアセット（ファイル）について、どのように軽量化・非同期化すべきか具体的な最適化策を提示してください：\n";
            for (const auto& loadLog : heavyLoads) {
                promptOfs << "   - 「" << CleanLogPrefix(loadLog) << "」\n";
            }
        } else {
            promptOfs << "3. **コード側の懸念箇所（非ロード時FPS低下）**: 今回はアセットロードなどのファイルI/O関連 of ボトルネックは検出されませんでした。したがって、**毎フレームの描画・更新負荷（CPUロジックまたはGPUの描画過多）**が原因である可能性が極めて高いです。リプレイ動画の視覚情報、および以下のカスタム情報をもとに原因を分析してください：\n";
            if (!data.customMetaData.empty()) {
                for (const auto& [key, value] : data.customMetaData) {
                    promptOfs << "   - 「" << key << " = " << value << "」\n";
                }
            }
        }

        if (!warningsAndErrors.empty()) {
            promptOfs << "4. **発生した警告・エラーの要因分析**: 今回の実行中に以下の警告・エラーが検知されています。これらがFPS低下とどう連動しているか、あるいは別の不具合を引き起こしていないか分析してください：\n";
            for (const auto& warnLog : warningsAndErrors) {
                promptOfs << "   - 「" << CleanLogPrefix(warnLog) << "」\n";
            }
        }
    }

    std::cout << "[PerformanceReporter] Async report output success to: " << data.folderName << std::endl;

    // --- 外部ツールへ最新レポートパスを通知 (リアルタイム同期) ---
    bool isServerNotified = false;
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock != INVALID_SOCKET) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8080);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

            DWORD timeout = 100;
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR) {
                // スラッシュ等をエスケープ (URLエンコード相当)
                std::string escapedPath = data.folderName;
                size_t pos = 0;
                while ((pos = escapedPath.find("/", pos)) != std::string::npos) {
                    escapedPath.replace(pos, 1, "%2F");
                    pos += 3;
                }
                pos = 0;
                while ((pos = escapedPath.find("\\", pos)) != std::string::npos) {
                    escapedPath.replace(pos, 1, "%2F");
                    pos += 3;
                }

                std::string req = std::format(
                    "GET /update_path?path={} HTTP/1.1\r\n"
                    "Host: localhost:8080\r\n"
                    "Connection: close\r\n\r\n",
                    escapedPath
                );

                send(sock, req.c_str(), (int)req.size(), 0);
                
                char recvBuf[128];
                recv(sock, recvBuf, sizeof(recvBuf), 0);
                
                isServerNotified = true;
                std::cout << "[PerformanceReporter] Successfully notified live viewer server." << std::endl;
            }
            closesocket(sock);
        }
        WSACleanup();
    }

    // サーバーへ通知できなかった（未起動の）場合のみ、新しくツールをキックする
    if (!isServerNotified) {
        std::cout << "[PerformanceReporter] Viewer server not running. Launching new viewer instance..." << std::endl;
        
        if (std::filesystem::exists("PerformanceViewer.exe")) {
            ShellExecuteA(NULL, "open", "PerformanceViewer.exe", NULL, NULL, SW_SHOW);
        } else if (std::filesystem::exists("Tools/PerformanceViewer/bin/Debug/PerformanceViewer.exe")) {
            ShellExecuteA(NULL, "open", "Tools\\PerformanceViewer\\bin\\Debug\\PerformanceViewer.exe", NULL, NULL, SW_SHOW);
        } else if (std::filesystem::exists("Tools/PerformanceViewer/bin/Release/PerformanceViewer.exe")) {
            ShellExecuteA(NULL, "open", "Tools\\PerformanceViewer\\bin\\Release\\PerformanceViewer.exe", NULL, NULL, SW_SHOW);
        } else {
            std::cout << "[PerformanceReporter] PerformanceViewer.exe was not found. Please build the PerformanceViewer project in Visual Studio." << std::endl;
        }
    }

    isDumping_ = false;
}

std::wstring PerformanceReporter::ConvertToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], sizeNeeded);
    return wstrTo;
}

void PerformanceReporter::SaveMp4File(const std::wstring& filePath, const std::vector<std::vector<BYTE>>& rawFrames, UINT width, UINT height) {
    if (rawFrames.empty()) return;

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return;

    Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter;
    hr = MFCreateSinkWriterFromURL(filePath.c_str(), NULL, NULL, &sinkWriter);
    if (FAILED(hr)) {
        MFShutdown();
        return;
    }

    // Set video video output stream (H.264 MP4)
    Microsoft::WRL::ComPtr<IMFMediaType> mediaTypeOut;
    hr = MFCreateMediaType(&mediaTypeOut);
    mediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    mediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 2000000); // 2Mbps
    MFSetAttributeSize(mediaTypeOut.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(mediaTypeOut.Get(), MF_MT_FRAME_RATE, 15, 1); // 15fps
    MFSetAttributeRatio(mediaTypeOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    mediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    DWORD streamIndex;
    hr = sinkWriter->AddStream(mediaTypeOut.Get(), &streamIndex);
    if (FAILED(hr)) {
        MFShutdown();
        return;
    }

    // Set video input stream (RGB32)
    Microsoft::WRL::ComPtr<IMFMediaType> mediaTypeIn;
    hr = MFCreateMediaType(&mediaTypeIn);
    mediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    MFSetAttributeSize(mediaTypeIn.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(mediaTypeIn.Get(), MF_MT_FRAME_RATE, 15, 1);
    MFSetAttributeRatio(mediaTypeIn.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    mediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    hr = sinkWriter->SetInputMediaType(streamIndex, mediaTypeIn.Get(), NULL);
    if (FAILED(hr)) {
        MFShutdown();
        return;
    }

    hr = sinkWriter->BeginWriting();
    if (FAILED(hr)) {
        MFShutdown();
        return;
    }

    LONGLONG rtStart = 0;
    LONGLONG frameDuration = 10 * 1000 * 1000 / 15; // 15fps duration
    std::vector<BYTE> rgbaBuffer(width * height * 4);

    for (size_t i = 0; i < rawFrames.size(); ++i) {
        const auto& frameData = rawFrames[i];
        if (frameData.empty()) continue;

        // Flip image vertically (Media Foundation RGB32 is bottom-up)
        for (UINT y = 0; y < height; ++y) {
            UINT srcY = height - 1 - y;
            const BYTE* srcRow = frameData.data() + (srcY * width * 4);
            BYTE* dstRow = rgbaBuffer.data() + (y * width * 4);
            
            for (UINT x = 0; x < width; ++x) {
                UINT srcIdx = x * 4;
                UINT dstIdx = x * 4;
                // Convert DX12 RGBA to BGRA
                dstRow[dstIdx + 0] = srcRow[srcIdx + 2]; // B
                dstRow[dstIdx + 1] = srcRow[srcIdx + 1]; // G
                dstRow[dstIdx + 2] = srcRow[srcIdx + 0]; // R
                dstRow[dstIdx + 3] = srcRow[srcIdx + 3]; // A
            }
        }

        // Write media sample
        Microsoft::WRL::ComPtr<IMFSample> sample;
        hr = MFCreateSample(&sample);
        if (SUCCEEDED(hr)) {
            Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
            hr = MFCreateMemoryBuffer(width * height * 4, &buffer);
            if (SUCCEEDED(hr)) {
                BYTE* dataDest = nullptr;
                hr = buffer->Lock(&dataDest, NULL, NULL);
                if (SUCCEEDED(hr)) {
                    std::memcpy(dataDest, rgbaBuffer.data(), width * height * 4);
                    buffer->Unlock();
                    buffer->SetCurrentLength(width * height * 4);
                    sample->AddBuffer(buffer.Get());
                    sample->SetSampleTime(rtStart);
                    sample->SetSampleDuration(frameDuration);
                    sinkWriter->WriteSample(streamIndex, sample.Get());
                }
            }
        }
        rtStart += frameDuration;
    }

    sinkWriter->Finalize();
    MFShutdown();
}
