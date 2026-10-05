#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Debug/IGameObject.h"
#include <algorithm>

namespace {
    // 重複時に付与するナンバリングの初期開始番号
    constexpr int kInitialSuffix = 1;
}

SceneHierarchy* SceneHierarchy::GetInstance() {
    static SceneHierarchy instance;
    return &instance;
}

std::string SceneHierarchy::Register(IGameObject* object, const std::string& suggestedName) {
    // 既に登録済みのオブジェクトインスタンスである場合は二重登録せずリネームのみ行う
    auto it = std::find(objects_.begin(), objects_.end(), object);
    if (it != objects_.end()) {
        return Rename(object, suggestedName);
    }

    std::string uniqueName = ResolveUniqueName(object, suggestedName);
    objects_.push_back(object);
    return uniqueName;
}

void SceneHierarchy::Unregister(IGameObject* object) {
    if (selected_ == object) {
        selected_ = nullptr;
    }
    auto it = std::remove(objects_.begin(), objects_.end(), object);
    if (it != objects_.end()) {
        objects_.erase(it, objects_.end());
    }
}

std::string SceneHierarchy::Rename(IGameObject* object, const std::string& newName) {
    return ResolveUniqueName(object, newName);
}

void SceneHierarchy::Clear() {
    objects_.clear();
    selected_ = nullptr;
}

std::string SceneHierarchy::ResolveUniqueName(IGameObject* self, const std::string& baseName) {
    std::string candidate = baseName;
    int suffix = kInitialSuffix;
    while (true) {
        bool duplicate = false;
        for (auto* obj : objects_) {
            // 自分以外で同じ名前のオブジェクトが既に存在するか確認
            if (obj != self && obj->GetName() == candidate) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            return candidate;
        }
        // 重複している場合は末尾に数値を付与してループ（例: cube -> cube1 -> cube2）
        candidate = baseName + std::to_string(suffix);
        suffix++;
    }
}
