#pragma once
#include <vector>
#include <memory>
#include <string>
#include <typeinfo>

#include "Engine/Component/ScriptComponent.h"

class Scene;

/// <summary>
/// シーン専属の司令塔（Director）クラス
/// ゲームルール、フェーズ管理、スポーン等の機能スクリプトを束ねて統括する
/// （Sceneクラスの神クラス化を防止するための責務分離クラス）
/// </summary>
class SceneDirector {
public:
    explicit SceneDirector(Scene* ownerScene);
    virtual ~SceneDirector();

    // ライフサイクル
    void Initialize();
    void Update();
    void Draw();
    void Draw2D();
    void DrawInspector();

    // スクリプト管理
    void AddScript(std::unique_ptr<ScriptComponent> script);
    void RemoveScriptByIndex(size_t index);
    const std::vector<std::unique_ptr<ScriptComponent>>& GetScripts() const { return scripts_; }

    template <typename T>
    T* GetScript() const {
        for (const auto& script : scripts_) {
            if (auto* casted = dynamic_cast<T*>(script.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    Scene* GetOwnerScene() const { return ownerScene_; }

private:
    Scene* ownerScene_ = nullptr;
    std::vector<std::unique_ptr<ScriptComponent>> scripts_;
    bool isUpdating_ = false;
    std::vector<std::unique_ptr<ScriptComponent>> pendingAddScripts_;
    std::vector<size_t> pendingRemoveIndices_;
};
