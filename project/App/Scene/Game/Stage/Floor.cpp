#include "App/Scene/Game/Stage/Floor.h"

Floor::Floor() {
    square_ = std::make_unique<SquareObject>();
}

void Floor::Initialize() {
    square_->Initialize();
    square_->SetPosition(kDefaultPosition);
    square_->SetSize(kDefaultSize);
    square_->SetRotate(kDefaultRotation);
    square_->SetColor(kDefaultColor);
}

void Floor::Update() {
    square_->Update();
}

void Floor::Draw() {
    square_->Draw();
}
