#include "Engine/Debug/Command/CreateGameObjectCommand.h"
#include "Engine/Component/GameObject.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Debug/SceneHierarchy.h"

CreateGameObjectCommand::CreateGameObjectCommand(GameObject* gameObject)
    : rawGameObject_(gameObject) {
    if (gameObject) {
        objectName_ = gameObject->GetName();
    }
}

CreateGameObjectCommand::~CreateGameObjectCommand() = default;

void CreateGameObjectCommand::Execute() {
    // 初回作成は既にシーン上に追加されているため何もしない
}

void CreateGameObjectCommand::Undo() {
    auto scene = SceneManager::GetInstance()->GetCurrentScene();
    if (!scene || !rawGameObject_) return;

    // シーンから一時退避してコマンド自身で保持
    detachedObject_ = scene->DetachGameObject(rawGameObject_);
}

void CreateGameObjectCommand::Redo() {
    auto scene = SceneManager::GetInstance()->GetCurrentScene();
    if (!scene || !detachedObject_) return;

    // コマンドが保持していたオブジェクトをシーンへ復帰
    rawGameObject_ = detachedObject_.get();
    scene->AddGameObject(std::move(detachedObject_));
    SceneHierarchy::GetInstance()->SetSelected(rawGameObject_);
}
