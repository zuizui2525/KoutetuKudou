#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <queue>
#include <cstdint>
#include "Engine/Graphics/Texture/Texture.h"

class TextureManager {
public:
    // システム予約ディスクリプタインデックス (マジックナンバー排除)
    static constexpr uint32_t kReservedImGuiIndex = 0;
    static constexpr uint32_t kReservedPostProcessMainIndex = 1;
    static constexpr uint32_t kReservedPostProcessTempIndex = 2;
    static constexpr uint32_t kReservedDepthOutlineIndex = 3;
    static constexpr uint32_t kDynamicStartDescriptorIndex = 10;
    static constexpr uint32_t kMaxSrvDescriptorCount = 2000;

    TextureManager() = default;
    ~TextureManager();

    // 初期化
    void Initialize();

    // テクスチャをロードして登録（同じ名前ならスキップ）
    void LoadTexture(const std::string& name, const std::string& filePath);

    // ファイルパスから直接ロードして登録（パスそのものをキーとして利用、既にロード済みならスキップ）
    std::string LoadTextureByPath(const std::string& filePath);

    // テクスチャが登録済みかどうか
    bool HasTexture(const std::string& name) const;

    // 更新処理
    void Update();

    // GPUハンドル取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(const std::string& name) const;

    // 動的テクスチャ・バッファ等のために空きディスクリプタインデックスを払い出す
    uint32_t AllocateDescriptorIndex();

    // 不要になったディスクリプタインデックスを返却して再利用可能にする
    void FreeDescriptorIndex(uint32_t index);

private:
    ID3D12Device* device_ = nullptr;
    ID3D12GraphicsCommandList* commandList_ = nullptr;
    ID3D12DescriptorHeap* srvHeap_ = nullptr;
    uint32_t descriptorCount_ = kDynamicStartDescriptorIndex; // 予約枠(0〜9)をスキップして10から開始
    std::queue<uint32_t> freeDescriptorIndices_;

    std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
};
