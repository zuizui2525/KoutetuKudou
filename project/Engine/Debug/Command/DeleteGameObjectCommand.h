#pragma once
#include "Engine/Debug/Command/IEditorCommand.h"
#include <memory>
#include <string>

class GameObject;

/// <summary>
/// GameObject の削除を記録・Undo / Redo するコマンド
/// </summary>
class DeleteGameObjectCommand : public IEditorCommand {
public:
    explicit DeleteGameObjectCommand(std::unique_ptr<GameObject> deletedObject);
    ~DeleteGameObjectCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;

    std::string GetName() const override { return "Delete: " + objectName_; }

private:
    std::string objectName_;
    GameObject* rawGameObject_ = nullptr;
    std::unique_ptr<GameObject> storedObject_;
};
