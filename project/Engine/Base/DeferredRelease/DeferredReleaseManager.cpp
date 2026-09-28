#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Base/DxCommon/DxCommon.h"

DeferredReleaseManager* DeferredReleaseManager::GetInstance() {
    static DeferredReleaseManager instance;
    return &instance;
}

void DeferredReleaseManager::Initialize(DxCommon* dxCommon) {
    dxCommon_ = dxCommon;
    pendingResources_.clear();
}

void DeferredReleaseManager::Enqueue(Microsoft::WRL::ComPtr<ID3D12Resource> resource) {
    if (!resource) {
        return; // nullリソースは無視
    }

    // 現在のフェンス値 + 安全マージンフレーム数を解放可能フェンス値として記録
    uint64_t currentFenceValue = dxCommon_ ? dxCommon_->GetFenceValue() : 0;
    uint64_t releaseFenceValue = currentFenceValue + kSafeFrameCount;

    PendingRelease entry;
    entry.resource = std::move(resource);
    entry.releaseFenceValue = releaseFenceValue;

    pendingResources_.push_back(std::move(entry));
}

void DeferredReleaseManager::Flush() {
    if (!dxCommon_ || pendingResources_.empty()) {
        return;
    }

    // GPUが完了したフェンス値を取得
    uint64_t completedValue = dxCommon_->GetFence()->GetCompletedValue();

    // 解放可能なリソースを削除（ComPtrのデストラクタでRelease()が安全に呼ばれる）
    auto it = pendingResources_.begin();
    while (it != pendingResources_.end()) {
        if (completedValue >= it->releaseFenceValue) {
            it = pendingResources_.erase(it);
        } else {
            ++it;
        }
    }
}

void DeferredReleaseManager::ReleaseAll() {
    pendingResources_.clear();
}
