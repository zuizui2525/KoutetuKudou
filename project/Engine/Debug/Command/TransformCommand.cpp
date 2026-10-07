#include "Engine/Debug/Command/TransformCommand.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Triangle/Triangle2DObject.h"
#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"

TransformCommand::TransformCommand(const std::string& objectName, const Transform& before, const Transform& after)
    : objectName_(objectName), beforeTransform_(before), afterTransform_(after) {
}

void TransformCommand::Execute() {
    // 初回は既に適用されているため処理不要
}

void TransformCommand::Undo() {
    ApplyTransform(beforeTransform_);
}

void TransformCommand::Redo() {
    ApplyTransform(afterTransform_);
}

void TransformCommand::ApplyTransform(const Transform& tr) {
    const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
    for (auto* obj : objects) {
        if (!obj || obj->GetName() != objectName_) continue;

        if (auto* go = dynamic_cast<GameObject*>(obj)) {
            go->SetPosition(tr.translate);
            go->SetRotate(tr.rotate);
            go->SetScale(tr.scale);
            go->UpdateMatrix();
        } else if (auto* obj3d = dynamic_cast<Object3D*>(obj)) {
            obj3d->SetPosition(tr.translate);
            obj3d->SetRotate(tr.rotate);
            obj3d->SetScale(tr.scale);
        } else if (auto* sprite = dynamic_cast<SpriteObject*>(obj)) {
            sprite->SetPosition(tr.translate);
            sprite->SetRotate(tr.rotate);
            sprite->SetScale(tr.scale);
        } else if (auto* tri = dynamic_cast<Triangle2DObject*>(obj)) {
            tri->SetPosition(tr.translate);
            tri->SetRotate(tr.rotate);
            tri->SetScale(tr.scale);
        } else if (auto* circle = dynamic_cast<Circle2DObject*>(obj)) {
            circle->SetPosition(tr.translate);
            circle->SetRotate(tr.rotate);
            circle->SetScale(tr.scale);
        } else if (auto* ring = dynamic_cast<Ring2DObject*>(obj)) {
            ring->SetPosition(tr.translate);
            ring->SetRotate(tr.rotate);
            ring->SetScale(tr.scale);
        } else if (auto* cam = dynamic_cast<BaseCamera*>(obj)) {
            cam->SetPosition(tr.translate);
            cam->SetRotation(tr.rotate);
        } else if (auto* light = dynamic_cast<DirectionalLightObject*>(obj)) {
            light->SetPosition(tr.translate);
            light->SetRotate(tr.rotate);
        }
        break;
    }
}
