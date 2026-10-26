#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <chrono>
#include <format>
#include <filesystem>

void TextureManager::Initialize() {
    // Engine
    auto engine = EngineResource::GetEngine();
    assert(engine != nullptr);

    device_ = engine->GetDevice();
    commandList_ = engine->GetDxCommon()->GetCommandList();
    srvHeap_ = engine->GetDxCommon()->GetSrvHeap();
}

void TextureManager::LoadTexture(const std::string& name, const std::string& filePath) {
    // すでにロード済みならスキップ
    if (textures_.find(name) != textures_.end()) {
        Log::Write(std::format(L" ├─ 【テクスチャロードスキップ】 キー:「{}」は既にロード済みです。", ConvertString(name)));
        return;
    }

    Log::Write(std::format(L" ├─ 【テクスチャロード開始】 キー:「{}」 | パス:「{}」", ConvertString(name), ConvertString(filePath)));
    auto startTime = std::chrono::steady_clock::now();

    uint32_t descriptorIndex = AllocateDescriptorIndex();
    auto texture = std::make_unique<Texture>();
    texture->Initialize(device_, commandList_, srvHeap_, descriptorIndex, filePath);
    textures_[name] = std::move(texture);

    auto endTime = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(endTime - startTime).count();

    // 0.1秒以上かかった場合は「低速ロード」マークをつける
    static constexpr float kSlowLoadThreshold = 0.1f;
    std::wstring slowLoadWarning = L"";
    if (elapsed >= kSlowLoadThreshold) {
        slowLoadWarning = L"[★低速ロード] ";
    }

    Log::Write(std::format(L" ├─ 【テクスチャロード完了】 {}:「{}」 | 所要時間: {:.4f}秒", 
        slowLoadWarning, ConvertString(filePath), elapsed));
}

std::string TextureManager::LoadTextureByPath(const std::string& filePath) {
    if (filePath.empty()) return "";

    // パスをスラッシュ区切り（generic_string）に正規化してキーとして使用
    std::filesystem::path p(filePath);
    std::string key = p.generic_string();

    if (!HasTexture(key)) {
        LoadTexture(key, key);
    }
    return key;
}

bool TextureManager::HasTexture(const std::string& name) const {
    return textures_.find(name) != textures_.end();
}

void TextureManager::Update() {
    for (auto& tex : textures_) {
        tex.second->Update();
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGpuHandle(const std::string& name) const {
    constexpr const char* kFallbackWhitePath = "resources/Textures/white.png";
    constexpr const char* kLegacyWhiteKey = "white";

    if (!name.empty()) {
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            return it->second->GetGpuHandle();
        }

        // 動的オンデマンドロード (ファイルパス直接指定の場合)
        if (std::filesystem::exists(name)) {
            const_cast<TextureManager*>(this)->LoadTexture(name, name);
            auto loadedIt = textures_.find(name);
            if (loadedIt != textures_.end()) {
                return loadedIt->second->GetGpuHandle();
            }
        }

        // 短縮キーの場合の自動探索解決 (resources/Textures/<name>.png / .dds)
        std::string fallbackPathPng = "resources/Textures/" + name + ".png";
        if (std::filesystem::exists(fallbackPathPng)) {
            const_cast<TextureManager*>(this)->LoadTexture(name, fallbackPathPng);
            const_cast<TextureManager*>(this)->LoadTexture(fallbackPathPng, fallbackPathPng);
            auto loadedIt = textures_.find(name);
            if (loadedIt != textures_.end()) {
                return loadedIt->second->GetGpuHandle();
            }
        }
        std::string fallbackPathDds = "resources/Textures/" + name + ".dds";
        if (std::filesystem::exists(fallbackPathDds)) {
            const_cast<TextureManager*>(this)->LoadTexture(name, fallbackPathDds);
            const_cast<TextureManager*>(this)->LoadTexture(fallbackPathDds, fallbackPathDds);
            auto loadedIt = textures_.find(name);
            if (loadedIt != textures_.end()) {
                return loadedIt->second->GetGpuHandle();
            }
        }
    }

    // 指定キーが見つからない、または空の場合は白テクスチャに安全フォールバック
    if (name != kFallbackWhitePath && name != kLegacyWhiteKey) {
        auto whitePathIt = textures_.find(kFallbackWhitePath);
        if (whitePathIt != textures_.end()) {
            return whitePathIt->second->GetGpuHandle();
        }
        auto legacyWhiteIt = textures_.find(kLegacyWhiteKey);
        if (legacyWhiteIt != textures_.end()) {
            return legacyWhiteIt->second->GetGpuHandle();
        }
    }

    // 存在しない場合のフォールバック（nullptr的ハンドル）
    D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{};
    nullHandle.ptr = 0;
    return nullHandle;
}

uint32_t TextureManager::AllocateDescriptorIndex() {
    if (!freeDescriptorIndices_.empty()) {
        uint32_t recycledIndex = freeDescriptorIndices_.front();
        freeDescriptorIndices_.pop();
        return recycledIndex;
    }
    assert(descriptorCount_ < kMaxSrvDescriptorCount && "SRV DescriptorHeap index out of bounds! Too many descriptors allocated.");
    uint32_t allocated = descriptorCount_;
    descriptorCount_++;
    return allocated;
}

void TextureManager::FreeDescriptorIndex(uint32_t index) {
    if (index >= kDynamicStartDescriptorIndex) {
        freeDescriptorIndices_.push(index);
    }
}

TextureManager::~TextureManager() {
    if (!textures_.empty()) {
        Log::Write(L" ├─ 【テクスチャシステム終了処理開始】 登録されているすべてのテクスチャを解放します。");
        for (auto& pair : textures_) {
            Log::Write(std::format(L" │   ├─ 【テクスチャ解放完了】 キー:「{}」のグラフィックスメモリを解放しました。", ConvertString(pair.first)));
        }
        textures_.clear();
        Log::Write(L" └─ 【テクスチャシステム終了処理完了】 すべてのテクスチャリソースの破棄が完了しました。");
    }
}
