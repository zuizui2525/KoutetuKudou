#pragma once
#include "Engine/Debug/Command/IEditorCommand.h"
#include "Engine/Math/MathStructs.h"
#include <string>

/// <summary>
/// オブジェクトの Transform (移動・回転・拡縮) 変更を記録・復元するコマンド
/// </summary>
class TransformCommand : public IEditorCommand {
public:
    TransformCommand(const std::string& objectName, const Transform& before, const Transform& after);

    void Execute() override;
    void Undo() override;
    void Redo() override;

    std::string GetName() const override { return "Transform: " + objectName_; }

private:
    void ApplyTransform(const Transform& tr);

private:
    std::string objectName_;
    Transform beforeTransform_;
    Transform afterTransform_;
};
