#pragma once
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

class DxCommon;

/**
 * @brief GPUが使用中のD3D12リソースを安全に遅延解放するためのマネージャ
 *
 * リソースを即座にReleaseせず、GPUが確実に使い終わるまでキューで保持し、
 * フレーム終了時にフェンスを確認して安全なリソースのみ解放する。
 */
class DeferredReleaseManager {
public:
    static DeferredReleaseManager* GetInstance();

    /**
     * @brief 初期化処理（DxCommonのポインタを受け取る）
     * @param dxCommon DxCommonへのポインタ（フェンス情報の取得に使用）
     */
    void Initialize(DxCommon* dxCommon);

    /**
     * @brief リソースを遅延解放キューに追加する
     * @param resource 解放を遅延させたいGPUリソース（ComPtrのムーブで受け取る）
     */
    void Enqueue(Microsoft::WRL::ComPtr<ID3D12Resource> resource);

    /**
     * @brief GPU完了済みのリソースをキューから安全に解放する
     * 毎フレーム DxCommon::EndFrame() から呼び出される
     */
    void Flush();

    /**
     * @brief キュー内の全リソースを即座に解放する（アプリケーション終了時用）
     * FlushGPU() で全GPUコマンドの完了を待ってから呼び出すこと
     */
    void ReleaseAll();

private:
    DeferredReleaseManager() = default;
    ~DeferredReleaseManager() = default;

    // コピー・ムーブ禁止
    DeferredReleaseManager(const DeferredReleaseManager&) = delete;
    DeferredReleaseManager& operator=(const DeferredReleaseManager&) = delete;

    /**
     * @brief 解放待ちリソースのエントリ
     */
    struct PendingRelease {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource; // 保持中のリソース
        uint64_t releaseFenceValue;                       // この値以上のフェンスが完了したら解放可能
    };

    DxCommon* dxCommon_ = nullptr;
    std::vector<PendingRelease> pendingResources_;

    /// ダブルバッファリングを考慮した安全マージン（フレーム数）
    static constexpr uint64_t kSafeFrameCount = 2;
};
