#include "App/Scene/Game/UI/Reticle.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Math/MathStructs.h"

Reticle::Reticle() = default;
Reticle::~Reticle() = default;

void Reticle::Initialize() {
    reticleSprite_ = std::make_unique<SpriteObject>();
    reticleSprite_->Initialize(0); // ライティングなし
    reticleSprite_->SetSize(kReticleSize, kReticleSize);
}

void Reticle::Update() {
    if (!reticleSprite_) return;

    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    float scaleX = static_cast<float>(WindowApp::kClientWidth) / viewSize.x;
    float scaleY = static_cast<float>(WindowApp::kClientHeight) / viewSize.y;
    Vector2 scaledMousePos = { mousePos.x * scaleX, mousePos.y * scaleY };

    reticleSprite_->SetPosition({ scaledMousePos.x - kHalfSize, scaledMousePos.y - kHalfSize, 0.0f });
    reticleSprite_->Update();
}

void Reticle::Draw() {
    if (reticleSprite_) {
        reticleSprite_->Draw(kTextureKey);
    }
}
