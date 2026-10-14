#include "Engine/Graphics/Objects/3d/Model/ModelManager.h"
#include "Engine/Graphics/Objects/3d/Model/ModelLoader.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <chrono>
#include <format>
#include <filesystem>
 
void ModelManager::Initialize() {
    // Engine
    auto engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    device_ = engine->GetDevice();
 
    // TexMgr
    auto texMgr = TextureResource::GetTextureManager();
    assert(texMgr != nullptr);
    texMgr_ = texMgr;
}
 
void ModelManager::LoadModel(const std::string& name, const std::string& filename) {
    if (models_.find(name) != models_.end()) return;
 
    auto startTime = std::chrono::steady_clock::now();
 
    auto data = std::make_shared<ModelData>(ModelLoader::LoadObjFile(filename));
 
    // 頂点リソース作成
    data->vertexResource = DxUtils::CreateBufferResource(device_, sizeof(VertexData) * data->vertices.size());
    data->vbv.BufferLocation = data->vertexResource->GetGPUVirtualAddress();
    data->vbv.SizeInBytes = sizeof(VertexData) * (UINT)data->vertices.size();
    data->vbv.StrideInBytes = sizeof(VertexData);
 
    // データ転送
    VertexData* vertexData = nullptr;
    data->vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    memcpy(vertexData, data->vertices.data(), sizeof(VertexData) * data->vertices.size());
    data->vertexResource->Unmap(0, nullptr);
 
    models_[name] = std::move(data);

    // テクスチャ未設定または存在しない場合はデフォルト白テクスチャにフォールバック
    constexpr const char* kDefaultWhiteTexturePath = "resources/Textures/white.png";
    std::string texPath = models_[name]->material.textureFilePath;
    if (texPath.empty() || !std::filesystem::exists(texPath)) {
        texPath = kDefaultWhiteTexturePath;
    }
    texMgr_->LoadTexture(name, texPath);
 
    auto endTime = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(endTime - startTime).count();
 
    // 0.1秒以上かかった場合は「低速ロード」マークをつける
    static constexpr float kSlowLoadThreshold = 0.1f;
    std::wstring slowLoadWarning = L"";
    if (elapsed >= kSlowLoadThreshold) {
        slowLoadWarning = L"[★低速ロード] ";
    }
 
    Log::Write(std::format(L" ├─ 【モデルロード完了】 {}キー:「{}」 | パス:「{}」 | 頂点数: {} | 所要時間: {:.4f}秒", 
        slowLoadWarning, ConvertString(name), ConvertString(filename), models_[name]->vertices.size(), elapsed));
}

std::string ModelManager::LoadModelByPath(const std::string& filename) {
    if (filename.empty()) return "";

    std::filesystem::path p(filename);
    std::string key = p.generic_string();

    if (!HasModel(key)) {
        LoadModel(key, key);
    }
    return key;
}

bool ModelManager::HasModel(const std::string& name) const {
    return models_.find(name) != models_.end();
}

std::shared_ptr<ModelData> ModelManager::GetModelData(const std::string& name) const {
    if (name.empty()) return nullptr;

    auto it = models_.find(name);
    if (it != models_.end()) return it->second;

    // パス直接指定の場合の動的オンデマンドロード
    if (std::filesystem::exists(name)) {
        const_cast<ModelManager*>(this)->LoadModel(name, name);
        auto loadedIt = models_.find(name);
        if (loadedIt != models_.end()) return loadedIt->second;
    }

    // 短縮名の場合の自動解決 (resources/obj/<name>/<name>.obj)
    std::string fallbackPathObj = "resources/obj/" + name + "/" + name + ".obj";
    if (std::filesystem::exists(fallbackPathObj)) {
        const_cast<ModelManager*>(this)->LoadModel(name, fallbackPathObj);
        const_cast<ModelManager*>(this)->LoadModel(fallbackPathObj, fallbackPathObj);
        auto loadedIt = models_.find(name);
        if (loadedIt != models_.end()) return loadedIt->second;
    }

    return nullptr;
}

ModelManager::~ModelManager() {
    Clear();
}

void ModelManager::Clear() {
    if (!models_.empty()) {
        Log::Write(L" ├─ 【モデルリソースクリア開始】 登録されているすべてのモデルデータを破棄します。");
        for (auto& pair : models_) {
            Log::Write(std::format(L" │   ├─ 【モデル解放完了】 キー:「{}」の頂点・インデックス・マテリアルバッファを解放しました。", ConvertString(pair.first)));
        }
        models_.clear();
        Log::Write(L" └─ 【モデルリソースクリア完了】 すべてのモデルデータの解放が完了しました。");
    }
}
