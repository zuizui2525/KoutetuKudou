#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <queue>
#include <cstdint>
#include "Engine/Graphics/Texture/Texture.h"

class TextureManager {
public:
    // --- SRVディスクリプタヒープ領域設定 (マジックナンバー排除) ---
    // 1. システム予約領域 (スロット 0 〜 9)
    static constexpr uint32_t kReservedPostProcessMainIndex = 1;  // ポストプロセス メインRT
    static constexpr uint32_t kReservedPostProcessTempIndex = 2;  // ポストプロセス 中間RT
    static constexpr uint32_t kReservedDepthOutlineIndex   = 3;  // 深度バッファ SRV
    static constexpr uint32_t kReservedParticleStudioIndex = 4;  // GPUパーティクルスタジオ プレビューRT
    static constexpr uint32_t kSystemReservedCount         = 10; // 予約枠の総数 (0〜9)

    // 2. 動的アロケーション領域 (スロット 10 〜 1899)
    // テクスチャ、動的テキスト、スキニング行列、パーティクル等で動的に確保・再利用
    static constexpr uint32_t kDynamicStartDescriptorIndex = kSystemReservedCount;
    static constexpr uint32_t kMaxTextureDescriptorCount   = 1890; // 最大動的スロット数 (10 + 1890 = 1900)

    // 3. ImGui専用領域 (スロット 1900 〜 1963)
    // 永続的に生存するImGuiフォント/アイコンテクスチャを完全分離
    static constexpr uint32_t kImGuiSrvStartIndex = 1900;
    static constexpr uint32_t kImGuiSrvCount      = 64;   // ビットマスク(uint64_t)で管理可能な64スロット

    // 4. ヒープ全体の最大スロット数 (DxCommonでの生成サイズと完全一致)
    static constexpr uint32_t kTotalSrvDescriptorCount = 2200;

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
