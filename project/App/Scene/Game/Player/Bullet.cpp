#include "App/Scene/Game/Player/Bullet.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"

Bullet::Bullet(const Vector3& position, const Vector3& velocity, const std::string& effectName)
    : velocity_(velocity), effectName_(effectName) {
    cube_ = std::make_unique<CubeObject>();
    cube_->Initialize();
    cube_->SetPosition(position);
    cube_->SetSize(kBulletScale);
    cube_->SetIsVisible(false); // 白いモデル自体は非表示にし、炎エフェクトのみ描画させる
}

void Bullet::Initialize() {
    // コンストラクタで初期化しているため空処理
}

void Bullet::Update() {
    if (!isActive_) {
        return;
    }

    // 移動処理
    cube_->SetPosition(cube_->GetPosition() + velocity_);
    cube_->Update();

    // 炎エフェクトを毎フレーム再生して弾の軌跡を作る
    EffectPlayParam fireParam;
    fireParam.position = cube_->GetPosition();
    EffectManager::GetInstance()->PlayEffect3D(effectName_, fireParam);

    // 寿命タイマーによる消滅判定
    lifeTimer_++;
    if (lifeTimer_ >= kMaxLifeTime) {
        isActive_ = false;
    }

}

void Bullet::Draw() {
    if (!isActive_) {
        return;
    }
    // 描画 (cube_ 自体は SetIsVisible(false) なので実際には描画されない)
    cube_->Draw(kTextureKey, kEnvMapKey);
}
