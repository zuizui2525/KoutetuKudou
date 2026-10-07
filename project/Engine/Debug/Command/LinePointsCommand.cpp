#include "Engine/Debug/Command/LinePointsCommand.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"

LinePointsCommand::LinePointsCommand(const std::string& objectName, const Vector2& beforeStart, const Vector2& beforeEnd, const Vector2& afterStart, const Vector2& afterEnd)
    : objectName_(objectName), beforeStart_(beforeStart), beforeEnd_(beforeEnd), afterStart_(afterStart), afterEnd_(afterEnd) {
}

void LinePointsCommand::Execute() {
}

void LinePointsCommand::Undo() {
    ApplyPoints(beforeStart_, beforeEnd_);
}

void LinePointsCommand::Redo() {
    ApplyPoints(afterStart_, afterEnd_);
}

void LinePointsCommand::ApplyPoints(const Vector2& start, const Vector2& end) {
    const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
    for (auto* obj : objects) {
        if (!obj || obj->GetName() != objectName_) continue;

        if (auto* go = dynamic_cast<GameObject*>(obj)) {
            if (auto* sr = go->GetComponent<SpriteRendererComponent>()) {
                sr->SetLineStart(start);
                sr->SetLineEnd(end);
            }
        } else if (auto* line2d = dynamic_cast<Line2DObject*>(obj)) {
            line2d->SetPoints(start, end);
        }
        break;
    }
}
