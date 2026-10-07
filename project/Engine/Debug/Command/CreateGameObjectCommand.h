#pragma once
#include "Engine/Debug/Command/IEditorCommand.h"
#include <memory>
#include <string>

class GameObject;

/// <summary>
/// GameObject の新規作成を記録・Undo / Redo するコマンド
/// </summary>
class CreateGameObjectCommand : public IEditorCommand {
public:
    explicit CreateGameObjectCommand(GameObject* gameObject);
    ~CreateGameObjectCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;

    std::string GetName() const override { return "Create: " + objectName_; }

private:
    std::string objectName_;
    GameObject* rawGameObject_ = nullptr;
    std::unique_ptr<GameObject> detachedObject_;
};
