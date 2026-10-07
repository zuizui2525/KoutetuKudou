#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Engine/Graphics/Texture/Texture.h"

class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager();

    // 初期化
    void Initialize();

    // テクスチャをロードして登録（同じ名前ならスキップ）
    void LoadTexture(const std::string& name, const std::string& filePath);

    // 更新処理
    void Update();

    // GPUハンドル取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(const std::string& name) const;

    // 動的テクスチャ等のために空きディスクリプタインデックスを払い出す
    uint32_t AllocateDescriptorIndex();

private:
    ID3D12Device* device_ = nullptr;
    ID3D12GraphicsCommandList* commandList_ = nullptr;
    ID3D12DescriptorHeap* srvHeap_ = nullptr;
    uint32_t descriptorCount_ = 1; // descriptor heapの先頭は別用途（0）なので1から開始

    std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
};
