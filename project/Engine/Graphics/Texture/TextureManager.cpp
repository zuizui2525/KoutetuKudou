#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <chrono>
#include <format>

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

    auto texture = std::make_unique<Texture>();
    texture->Initialize(device_, commandList_, srvHeap_, descriptorCount_, filePath);
    textures_[name] = std::move(texture);
    descriptorCount_++;

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

void TextureManager::Update() {
    for (auto& tex : textures_) {
        tex.second->Update();
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGpuHandle(const std::string& name) const {
    constexpr const char* kFallbackWhiteKey = "white";

    if (!name.empty()) {
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            return it->second->GetGpuHandle();
        }
    }

    // 指定キーが見つからない、または空の場合は "white" テクスチャに安全フォールバック
    if (name != kFallbackWhiteKey) {
        auto whiteIt = textures_.find(kFallbackWhiteKey);
        if (whiteIt != textures_.end()) {
            return whiteIt->second->GetGpuHandle();
        }
    }

    // 存在しない場合のフォールバック（nullptr的ハンドル）
    D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{};
    nullHandle.ptr = 0;
    return nullHandle;
}

uint32_t TextureManager::AllocateDescriptorIndex() {
    uint32_t allocated = descriptorCount_;
    descriptorCount_++;
    return allocated;
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
