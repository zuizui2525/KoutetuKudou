#include "Engine/Debug/Command/DeleteGameObjectCommand.h"
#include "Engine/Component/GameObject.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Debug/SceneHierarchy.h"

DeleteGameObjectCommand::DeleteGameObjectCommand(std::unique_ptr<GameObject> deletedObject)
    : storedObject_(std::move(deletedObject)) {
    if (storedObject_) {
        rawGameObject_ = storedObject_.get();
        objectName_ = storedObject_->GetName();
    }
}

DeleteGameObjectCommand::~DeleteGameObjectCommand() = default;

void DeleteGameObjectCommand::Execute() {
    // 削除オブジェクトは既にシーンからDetachされているため何もしない
}

void DeleteGameObjectCommand::Undo() {
    auto scene = SceneManager::GetInstance()->GetCurrentScene();
    if (!scene || !storedObject_) return;

    // 削除されていたオブジェクトをシーンへ復元
    rawGameObject_ = storedObject_.get();
    scene->AddGameObject(std::move(storedObject_));
    SceneHierarchy::GetInstance()->SetSelected(rawGameObject_);
}

void DeleteGameObjectCommand::Redo() {
    auto scene = SceneManager::GetInstance()->GetCurrentScene();
    if (!scene || !rawGameObject_) return;

    // 再度シーンから取り除いてコマンド内で保持
    storedObject_ = scene->DetachGameObject(rawGameObject_);
}
