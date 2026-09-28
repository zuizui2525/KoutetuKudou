#include "App/Scene/Game/Stage/Pillar.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/PSO/Manager/PSOManager.h"
#include "Engine/Zuizui.h"

Pillar::Pillar() {
    cube_ = std::make_unique<CubeObject>();
    warningCircle_ = std::make_unique<SquareObject>();
}

void Pillar::Initialize(const Vector3& position) {
    cube_->Initialize();
    cube_->SetPosition(position);
    cube_->SetScale(kPillarScale);
    cube_->SetColor(kPillarColor);

    // 警告サークルの初期化
    // ライティングなし(0)で初期化することで、暗闇でも警告色(オレンジ)がはっきり見えるようにする
    warningCircle_->Initialize(0);
    warningCircle_->SetPosition({ position.x, kWarningHeightOffset, position.z });
    warningCircle_->SetSize(kWarningCircleSize);
    warningCircle_->SetRotate(kWarningCircleRotation);
    warningCircle_->SetColor(kWarningColor);
}

void Pillar::Update() {
    cube_->Update();
    warningCircle_->Update();
}

void Pillar::Draw(bool showWarning) {
    cube_->Draw();
    if (showWarning) {
        warningCircle_->Draw(kWarningTextureKey, "", "Object3D_Alpha");
    }
}
