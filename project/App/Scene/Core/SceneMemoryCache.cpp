#include "App/Scene/Core/SceneMemoryCache.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <fstream>
#include <filesystem>
#include <format>

namespace {
    const std::string kDefaultExtension = ".json";
    const std::string kPathSeparator = "/";
}

SceneMemoryCache* SceneMemoryCache::GetInstance() {
    static SceneMemoryCache instance;
    return &instance;
}

void SceneMemoryCache::SetCache(const std::string& sceneName, const std::string& jsonString) {
    if (sceneName.empty()) return;

    cacheMap_[sceneName] = jsonString;
    dirtyScenes_.insert(sceneName);

    Log::Write(std::format(L"[SceneMemoryCache] シーン「{}」の変更をメモリ一時キャッシュに保存しました（ディスク未保存）。",
        ConvertString(sceneName)));
}

std::string SceneMemoryCache::GetCache(const std::string& sceneName) const {
    auto it = cacheMap_.find(sceneName);
    if (it != cacheMap_.end()) {
        return it->second;
    }
    return "";
}

bool SceneMemoryCache::HasCache(const std::string& sceneName) const {
    return cacheMap_.find(sceneName) != cacheMap_.end();
}

bool SceneMemoryCache::IsDirty(const std::string& sceneName) const {
    return dirtyScenes_.find(sceneName) != dirtyScenes_.end();
}

bool SceneMemoryCache::HasAnyDirty() const {
    return !dirtyScenes_.empty();
}

void SceneMemoryCache::ClearCache(const std::string& sceneName) {
    cacheMap_.erase(sceneName);
    dirtyScenes_.erase(sceneName);
    Log::Write(std::format(L"[SceneMemoryCache] シーン「{}」の一時キャッシュを破棄しました。",
        ConvertString(sceneName)));
}

void SceneMemoryCache::ClearAll() {
    size_t count = dirtyScenes_.size();
    cacheMap_.clear();
    dirtyScenes_.clear();
    Log::Write(std::format(L"[SceneMemoryCache] 全ての一時キャッシュ (計 {} 件) を破棄しました。ディスクファイルは元の状態を維持します。",
        count));
}

bool SceneMemoryCache::SaveAllDirtyToDisk(const std::string& scenesDirectory) {
    if (dirtyScenes_.empty()) {
        Log::Write(L"[SceneMemoryCache] 未保存のシーン変更はありません。");
        return true;
    }

    bool allSuccess = true;
    std::filesystem::path dir(scenesDirectory);
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }

    for (const auto& sceneName : dirtyScenes_) {
        auto it = cacheMap_.find(sceneName);
        if (it == cacheMap_.end()) continue;

        std::string filePath = scenesDirectory + kPathSeparator + sceneName + kDefaultExtension;
        std::ofstream ofs(filePath);
        if (ofs.is_open()) {
            ofs << it->second;
            Log::Write(std::format(L"[SceneMemoryCache] シーン「{}」をディスク「{}」に正式保存しました。",
                ConvertString(sceneName), ConvertString(filePath)));
        } else {
            allSuccess = false;
            Log::Write(std::format(L"[SceneMemoryCache] シーン「{}」のディスク保存に失敗しました。",
                ConvertString(sceneName)));
        }
    }

    dirtyScenes_.clear();
    return allSuccess;
}
