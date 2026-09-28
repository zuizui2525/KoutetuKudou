#include "App/Scene/Game/Light/GameLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"

GameLight::GameLight() = default;
GameLight::~GameLight() = default;

void GameLight::Initialize(LightManager* lightMgr) {
    lightMgr_ = lightMgr;

    dirLight_ = std::make_unique<DirectionalLightObject>();
    dirLight_->Initialize();

    if (lightMgr_) {
        lightMgr_->AddDirectionalLight(dirLight_.get());
    }
}

void GameLight::Update() {
    if (dirLight_) {
        dirLight_->Update();
    }
}
