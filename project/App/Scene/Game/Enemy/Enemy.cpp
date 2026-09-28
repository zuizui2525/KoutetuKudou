#include "App/Scene/Game/Enemy/Enemy.h"
#include "App/Scene/Game/Player/Player.h"
#include "Engine/Math/Matrix/Matrix.h"

#include <chrono>

Enemy::Enemy() {
    cube_ = std::make_unique<CubeObject>();
    headCube_ = std::make_unique<CubeObject>();
    bodyCollider_ = std::make_unique<PartCollider>();
    headCollider_ = std::make_unique<PartCollider>();
    bulletEffectName_ = kBulletEffectName;
    
    // 安全な乱数シードの設定 (std::random_deviceのシステム例外回避)
    auto seed = static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    randomEngine_ = std::mt19937(seed);
}

void Enemy::Initialize() {
    cube_->Initialize();
    cube_->SetPosition(kInitialPosition);
    cube_->SetSize(kEnemyScale);
    size_ = kEnemyScale;
    cube_->SetColor(kEnemyColor); // 敵を赤くする

    // 頭部モデルの初期化と親子関係設定
    headCube_->Initialize();
    headCube_->SetParent(cube_.get());
    headCube_->SetPosition(kHeadLocalPos);
    headCube_->SetScale(kHeadLocalScale);
    headCube_->SetColor(kHeadColor);

    // 部位コライダーの初期化と親子関係設定
    bodyCollider_->Initialize(PartCollider::Type::Body, kBodyColliderSize);
    bodyCollider_->SetParent(cube_.get());
    bodyCollider_->SetPosition({ 0.0f, 0.0f, 0.0f });
    bodyCollider_->SetScale({ 1.0f, 1.0f, 1.0f });

    headCollider_->Initialize(PartCollider::Type::Head, kHeadColliderSize);
    headCollider_->SetParent(cube_.get());
    headCollider_->SetPosition(kHeadLocalPos);
    headCollider_->SetScale({ 1.0f, 1.0f, 1.0f });

    ClearBullets();
    InitializeHpBar(); // 体力と体力バーの初期化

    hp_ = isBoss_ ? kBossEnemyHp : kNormalEnemyHp;
    maxHp_ = hp_;

    isSpawnPoint_ = false;
    hitFlashTimer_ = 0;

    aiState_ = AiState::Approach;
    toCenterGauge_ = 0.0f;
    attackGauge_ = 0.0f;
}

void Enemy::Update() {
    if (!cube_ || !headCube_ || !bodyCollider_ || !headCollider_) return;

    if (isSpawnPoint_) {
        cube_->Update();
        headCube_->Update();
        bodyCollider_->Update();
        headCollider_->Update();
        UpdateBullets();
        return;
    }

    if (!targetPlayer_) {
        cube_->Update();
        headCube_->Update();
        bodyCollider_->Update();
        headCollider_->Update();
        UpdateBullets();
        return;
    }

    Vector3 pos = cube_->GetPosition();
    Vector3 playerPos = targetPlayer_->GetPosition();

    // 1. プレイヤーを注視する (向きの設定)
    Vector3 toPlayer = Math::Subtract(playerPos, pos);
    float yaw = std::atan2(toPlayer.x, toPlayer.z);
    float pitch = -std::atan2(toPlayer.y, std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z));
    cube_->SetRotate({ pitch, yaw, 0.0f });

    // 2. AIステートマシンに基づく移動とゲージ更新
    switch (aiState_) {
    case AiState::Approach: {
        // プレイヤーのZ座標より高速接近
        static const float kApproachTransitionOffset = 10.0f;
        
        pos.z += kApproachSpeedZ;

        // プレイヤーのZ位置より前方（視界内）に入ったらTargetLockへ移行
        if (pos.z > playerPos.z + kApproachTransitionOffset) {
            aiState_ = AiState::TargetLock;
            toCenterGauge_ = 0.0f;
        }
        break;
    }
    case AiState::TargetLock: {
        // プレイヤーと並走 (同じZ速度)
        pos.z += Player::GetAutoSpeed();

        // 視界内チャージゲージを蓄積
        toCenterGauge_ += kGaugeIncreaseRate;
        if (toCenterGauge_ >= kToCenterGaugeMax) {
            toCenterGauge_ = kToCenterGaugeMax;
            aiState_ = AiState::MoveToCenter;
        }
        break;
    }
    case AiState::MoveToCenter: {
        // プレイヤーと並走
        pos.z += Player::GetAutoSpeed();

        // 真正面中央 (X=0.0f) へ高速スライド
        float dx = 0.0f - pos.x;
        static const float kCenterMargin = 0.1f;
        if (std::abs(dx) > kCenterMargin) {
            pos.x += (dx > 0.0f ? 1.0f : -1.0f) * kCenterSpeedX;
        } else {
            pos.x = 0.0f;
            aiState_ = AiState::AttackCharge;
            attackGauge_ = 0.0f;
        }
        break;
    }
    case AiState::AttackCharge: {
        // 真正面中央を維持してプレイヤーと並走
        pos.z += Player::GetAutoSpeed();
        pos.x = 0.0f;

        // 攻撃ゲージを蓄積
        attackGauge_ += kGaugeIncreaseRate;
        if (attackGauge_ >= kAttackGaugeMax) {
            attackGauge_ = kAttackGaugeMax;
            aiState_ = AiState::UnavoidableAttack;
        }
        break;
    }
    case AiState::UnavoidableAttack: {
        // 真正面中央を維持してプレイヤーと並走
        pos.z += Player::GetAutoSpeed();
        pos.x = 0.0f;

        // 不可避攻撃（高速・大判定弾）を発射
        Vector3 bulletPos = pos + Vector3{ 0.0f, 0.0f, -1.5f };
        
        // プレイヤーを正確に狙う
        Vector3 bulletDir = Math::Normalize(Math::Subtract(playerPos, bulletPos));
        static const float kUnavoidableBulletSpeed = 0.4f; // 通常(0.15f)の約2.6倍の高速弾
        Vector3 bulletVel = Math::Multiply(kUnavoidableBulletSpeed, bulletDir);

        bullets_.push_back(std::make_unique<Bullet>(bulletPos, bulletVel, kBulletEffectName));

        // 発射後、ゲージをリセットしてプレイヤー背後の左右いずれかへ退避
        aiState_ = AiState::Approach;
        toCenterGauge_ = 0.0f;
        attackGauge_ = 0.0f;

        static const float kRetreatOffsetZ = -15.0f;
        static const float kRetreatOffsetX = 10.0f;
        pos.z = playerPos.z + kRetreatOffsetZ;
        pos.x = (rand() % 2 == 0) ? -kRetreatOffsetX : kRetreatOffsetX;
        break;
    }
    case AiState::StationaryShooting: {
        // 静止砲撃型: 移動せず、定位置で演出専用弾を定期射撃
        shotTimer_++;
        static const int kStationaryShotInterval = 60; // 60フレームごとに射撃
        if (shotTimer_ >= kStationaryShotInterval) {
            shotTimer_ = 0;
            Vector3 bulletPos = pos + Vector3{ 0.0f, 0.0f, -1.0f };
            Vector3 bulletDir = Math::Normalize(Math::Subtract(playerPos, bulletPos));
            static const float kStationaryBulletSpeed = 0.2f;
            Vector3 bulletVel = Math::Multiply(kStationaryBulletSpeed, bulletDir);

            auto bullet = std::make_unique<Bullet>(bulletPos, bulletVel, kBulletEffectName);
            bullet->SetVisualOnly(true); // ★演出用弾（当たり判定なし）
            bullets_.push_back(std::move(bullet));
        }
        break;
    }
    case AiState::SwarmHanging: {
        // 小型並走群れ型: プレイヤーと並走し近傍を漂う
        pos.z += Player::GetAutoSpeed();
        
        static const float kHangOscillationSpeed = 0.05f;
        static const float kHangOscillationAmp = 0.02f;
        hangTimer_ += 1.0f / 60.0f; // 1フレーム1/60秒経過
        pos.x += std::sin(hangTimer_ * 10.0f) * kHangOscillationAmp;

        // 指定並走時間を超えたら攻撃状態へ移行
        if (hangTimer_ >= hangTime_) {
            aiState_ = AiState::SwarmAttacking;
        }
        break;
    }
    case AiState::SwarmAttacking: {
        // 小型並走群れ型: 数秒後、プレイヤーに向かって突撃
        static const float kSwarmAttackSpeed = 0.25f;
        Vector3 dir = Math::Normalize(Math::Subtract(playerPos, pos));
        pos.x += dir.x * kSwarmAttackSpeed;
        pos.y += dir.y * kSwarmAttackSpeed;
        pos.z += dir.z * kSwarmAttackSpeed;
        break;
    }
    }

    // 移動限界制限
    if (pos.x < -kMoveLimitX) pos.x = -kMoveLimitX;
    if (pos.x > kMoveLimitX) pos.x = kMoveLimitX;

    cube_->SetPosition(pos);
    cube_->Update();

    // 子オブジェクトおよびコライダーの更新
    headCube_->Update();
    bodyCollider_->Update();
    headCollider_->Update();

    // 弾の更新
    UpdateBullets();

    // 被弾フラッシュタイマーの更新
    if (hitFlashTimer_ > 0) {
        hitFlashTimer_--;
    }

    // 体力バーの追従更新 (頭部の上に表示するため、少し上にオフセット。ボスは巨大なのでオフセットも高めにする)
    Vector3 hpOffset = { 0.0f, isBoss_ ? 4.5f : 1.8f, 0.0f };
    UpdateHpBar(pos + hpOffset);
}

void Enemy::Draw() {
    if (!cube_ || !headCube_) return;

    if (isSpawnPoint_) {
        cube_->Draw(kTextureKey, kEnvMapKey);
        return;
    }

    if (hitFlashTimer_ > 0) {
        static const Vector4 kFlashColor = { 1.0f, 1.0f, 0.0f, 1.0f };
        cube_->SetColor(kFlashColor);
        headCube_->SetColor(kFlashColor);
    } else {
        cube_->SetColor(kEnemyColor);
        headCube_->SetColor(kHeadColor);
    }

    cube_->Draw(kTextureKey, kEnvMapKey);
    headCube_->Draw(kTextureKey, kEnvMapKey); // 頭部モデルの描画
    DrawBullets();
    DrawHpBar(); // 体力バーの描画
}

void Enemy::SetBoss(bool isBoss) {
    isBoss_ = isBoss;
    if (isBoss_) {
        cube_->SetScale({ 3.0f, 3.0f, 3.0f });
        headCube_->SetScale({ 0.6f, 0.6f, 0.6f });
        
        bodyCollider_->SetScale({ 3.0f, 3.0f, 3.0f });
        headCollider_->SetScale({ 1.8f, 1.8f, 1.8f });
        
        hp_ = kBossEnemyHp;
        maxHp_ = hp_;
    } else {
        cube_->SetScale(kEnemyScale);
        headCube_->SetScale(kHeadLocalScale);
        
        bodyCollider_->SetScale({ 1.0f, 1.0f, 1.0f });
        headCollider_->SetScale({ 1.0f, 1.0f, 1.0f });
        
        hp_ = kNormalEnemyHp;
        maxHp_ = hp_;
    }
}

void Enemy::UpdateHpBar(const Vector3& charPos) {
    if (!hpBarBG_ || !hpBarFill_) return;

    Vector3 bgPos = charPos;
    hpBarBG_->SetPosition(bgPos);
    
    Vector3 barBaseScale = kHpBarScale;
    if (isBoss_) {
        barBaseScale.x *= 2.0f; 
        hpBarBG_->SetSize(barBaseScale);
    } else {
        hpBarBG_->SetSize(barBaseScale);
    }
    hpBarBG_->Update();

    float hpRate = static_cast<float>(hp_) / static_cast<float>(maxHp_);
    
    Vector3 fillScale = barBaseScale;
    fillScale.x *= hpRate;
    hpBarFill_->SetSize(fillScale);

    static constexpr float kHalf = 0.5f;
    Vector3 fillPos = bgPos;
    fillPos.x -= (barBaseScale.x - fillScale.x) * kHalf;
    
    static constexpr float kZOffset = -0.01f; 
    fillPos.z += kZOffset;

    hpBarFill_->SetPosition(fillPos);
    hpBarFill_->Update();

    if (burnTimer_ > 0) {
        if (burnTimer_ == kBurnDuration) {
            Vector3 size = GetSize();
            Vector3 ringPos = cube_->GetPosition();
            ringPos.y -= size.y * kHalf;

            EffectPlayParam ringParam;
            ringParam.position = ringPos;
            ringParam.scale = kRingEffectScale;

            if (isBoss_) {
                ringParam.scale = Math::Multiply(3.0f, kRingEffectScale);
            }

            if (burnEffectName_ == kYellowFireEffectName) {
                ringParam.colorOverride = kYellowRingColor;
            } else if (burnEffectName_ == kPurpleFireEffectName) {
                ringParam.colorOverride = kPurpleRingColor;
            }

            EffectManager::GetInstance()->PlayEffect3D(kRingEffectName, ringParam);
        }

        burnTimer_--;
        for (int i = 0; i < kFireCountPerFrame; ++i) {
            Vector3 size = GetSize();
            float rx = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * size.x;
            float ry = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * size.y;
            float rz = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * size.z;

            Vector3 firePos = cube_->GetPosition() + Vector3{ rx, ry, rz };
            EffectPlayParam burnParam;
            burnParam.position = firePos;
            burnParam.scale = isBoss_ ? Math::Multiply(2.5f, kBurnEffectScale) : kBurnEffectScale;
            EffectManager::GetInstance()->PlayEffect3D(burnEffectName_, burnParam);
        }
    }
}

void Enemy::SetSpawnPoint(bool active) {
    isSpawnPoint_ = active;
    if (isSpawnPoint_) {
        // 出現サークル用に潰したシアンの円盤を設定
        cube_->SetScale({ 3.0f, 0.1f, 3.0f });
        cube_->SetColor({ 0.0f, 1.0f, 0.8f, 0.5f });
        
        // 不要な頭部・コライダーは非アクティブ化(サイズを0にする)
        headCube_->SetScale({ 0.0f, 0.0f, 0.0f });
        bodyCollider_->SetScale({ 0.0f, 0.0f, 0.0f });
        headCollider_->SetScale({ 0.0f, 0.0f, 0.0f });
    } else {
        // 通常の敵に戻す
        cube_->SetScale(kEnemyScale);
        cube_->SetColor(kEnemyColor);
        headCube_->SetScale(kHeadLocalScale);
        headCube_->SetColor(kHeadColor);
        
        bodyCollider_->SetScale({ 1.0f, 1.0f, 1.0f });
        headCollider_->SetScale({ 1.0f, 1.0f, 1.0f });
    }
}

void Enemy::Damage(int amount, const std::string& effectName) {
    // 湧きサークル（エディタ用）の場合はダメージを受けない
    if (isSpawnPoint_) return;

    Shooter::Damage(amount, effectName);
    
    // 黄色い被弾フラッシュを設定
    hitFlashTimer_ = 5;
}

void Enemy::SetSize(const Vector3& size) {
    size_ = size;
    if (!isSpawnPoint_) {
        cube_->SetSize(size);
    }
}
