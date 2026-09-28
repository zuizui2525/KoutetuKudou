#include "App/Scene/Game/Stage/Stage.h"
#include <cmath>

Stage::Stage() {
    floor_ = std::make_unique<Floor>();
}

void Stage::Initialize() {
    floor_->Initialize();

    pillars_.clear();
    for (float z = kPillarStartZ; z <= kPillarEndZ; z += kPillarStepZ) {
        float x = (static_cast<int>(z) % kPillarPeriodZ == 0) ? -kPillarXOffset : kPillarXOffset;
        auto pillar = std::make_unique<Pillar>();
        pillar->Initialize({ x, kPillarY, z });
        pillars_.push_back(std::move(pillar));
    }
}

void Stage::Update() {
    floor_->Update();
    for (auto& pillar : pillars_) {
        pillar->Update();
    }
}

void Stage::Draw(bool showWarning, bool isPlayMode, const Vector3& playerPos) {
    if (isPlayMode) {
        floor_->Draw();
    }

    for (auto& pillar : pillars_) {
        if (isPlayMode) {
            float distZ = std::abs(pillar->GetPosition().z - playerPos.z);
            if (distZ > kCullingDistance) {
                continue; // プレイ中はプレイヤーから離れた柱を描画しない
            }
        }
        pillar->Draw(showWarning);
    }
}
