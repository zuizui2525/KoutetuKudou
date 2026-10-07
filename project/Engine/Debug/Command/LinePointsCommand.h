#pragma once
#include "Engine/Debug/Command/IEditorCommand.h"
#include "Engine/Math/MathStructs.h"
#include <string>

/// <summary>
/// Line2Dの始点・終点移動を記録・復元するコマンド
/// </summary>
class LinePointsCommand : public IEditorCommand {
public:
    LinePointsCommand(const std::string& objectName, const Vector2& beforeStart, const Vector2& beforeEnd, const Vector2& afterStart, const Vector2& afterEnd);

    void Execute() override;
    void Undo() override;
    void Redo() override;

    std::string GetName() const override { return "Line Points: " + objectName_; }

private:
    void ApplyPoints(const Vector2& start, const Vector2& end);

private:
    std::string objectName_;
    Vector2 beforeStart_;
    Vector2 beforeEnd_;
    Vector2 afterStart_;
    Vector2 afterEnd_;
};
