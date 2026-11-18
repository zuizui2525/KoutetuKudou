#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleSetting.h"
#include "Engine/Graphics/Objects/3d/Model/ModelObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/3d/FlatRing/FlatRingObject.h"
#include "Engine/Graphics/Objects/3d/Cone/ConeObject.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"
#include <cmath>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(PlayerControllerScript)

namespace {
    constexpr float kEpsilon = 0.0001f;
    constexpr float kPi = 3.1415926535f;
    constexpr float kTwoPi = 6.283185307f;
    constexpr float kDefaultPlayerMuzzleHeight = 1.0f; // カメラ未初期化時のフォールバック高さ
    constexpr float kRayTargetDistance = 50.0f;
    constexpr int kReloadKey = DIK_R;
    constexpr float kDefaultDeltaTime = 1.0f / 60.0f;

    // 死亡演出用定数
    constexpr float kRandOffsetCenter = 0.5f;
    constexpr float kMeltSinkSpeed = 0.015f;
    constexpr int kBubbleInterval = 10;
    constexpr uint32_t kHitSmokeCount = 30;
    constexpr uint32_t kMeltBurstCount = 80;
    constexpr uint32_t kMeltStepCount = 20;
    constexpr float kMinSinkGroundY = 0.05f;
    constexpr float kMeltSmokeMaxProgress = 0.85f;
    constexpr float kMeltColorRedWeight = 0.3f;
    constexpr float kMeltColorBlueWeight = 0.4f;
    constexpr float kMeltColorTargetRed = 0.15f;
    constexpr float kMeltColorTargetBlue = 0.25f;
    constexpr float kMeltColorGreen = 0.02f;
}

PlayerControllerScript::PlayerControllerScript()
    : ScriptComponent("PlayerControllerScript") {
}

PlayerControllerScript::~PlayerControllerScript() {
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
    muzzleFlameOuter_.reset();
    muzzleFlameCore_.reset();
    muzzleCoreDisc_.reset();
    muzzleGpuParticle_.reset();
    gunModel_.reset();
}

void PlayerControllerScript::Awake() {
    hp_ = maxHp_;
    ammo_ = maxAmmo_;
    reloadTimer_ = 0;
    shotIntervalTimer_ = 0;
    invincibilityTimer_ = 0;
    isDead_ = false;
    isAutoMoving_ = false;
    isMelting_ = false;
    meltTimer_ = 0;
    isInitialPosSaved_ = false;
    gunRecoilProgress_ = 0.0f;
    isGunVisible_ = true;
    hasPrevPos_ = false;
    currentVelocity_ = { 0.0f, 0.0f, 0.0f };
    EnsureGunModel();
}

void PlayerControllerScript::Start() {
    if (owner_ && !isOriginalColorSaved_) {
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            originalColor_ = mr->GetColor();
            isOriginalColorSaved_ = true;
        }
    }
    if (owner_ && !isInitialScaleSaved_) {
        owner_->SetScale({ kPlayerBaseScale, kPlayerBaseScale, kPlayerBaseScale });
        initialScale_ = owner_->GetScale();
        isInitialScaleSaved_ = true;
    }

    if (auto* audio = Audio::GetInstance()) {
        if (!isSeShootLoaded_) {
            seShoot_ = audio->LoadSound(kSeShootPath);
            isSeShootLoaded_ = true;
        }
        if (!isSeDryFireLoaded_) {
            seDryFire_ = audio->LoadSound(kSeDryFirePath);
            isSeDryFireLoaded_ = true;
        }
        if (!isSeReloadLoaded_) {
            seReload_ = audio->LoadSound(kSeReloadPath);
            isSeReloadLoaded_ = true;
        }
    }
}

void PlayerControllerScript::OnUpdate() {
    // 被弾演出（画面外周の赤い靄＆赤フラッシュ）の滑らかなフェードアウト更新
    if (damageVignetteAlpha_ > 0.0f) {
        damageVignetteAlpha_ = (std::max)(0.0f, damageVignetteAlpha_ - kDamageVignetteFadeSpeed);
    }
    if (damageFlashAlpha_ > 0.0f) {
        damageFlashAlpha_ = (std::max)(0.0f, damageFlashAlpha_ - kDamageFlashFadeSpeed);
    }

    // 死亡溶解演出の更新
    if (isMelting_) {
        UpdateDeathMeltAnimation();
        return;
    }

    if (isDead_ || !owner_) {
        return;
    }

    // 自機のフレーム移動差分（実速度ベクトル）の算出（弾丸の慣性同期・レティクル完全直進化に使用）
    Vector3 currentPos = owner_->GetPosition();
    if (isAutoMoving_) {
        if (hasPrevPos_) {
            currentVelocity_ = {
                currentPos.x - prevPos_.x,
                currentPos.y - prevPos_.y,
                currentPos.z - prevPos_.z
            };
        } else {
            currentVelocity_ = {
                direction_.x * moveSpeed_,
                direction_.y * moveSpeed_,
                direction_.z * moveSpeed_
            };
            hasPrevPos_ = true;
        }
    } else {
        currentVelocity_ = { 0.0f, 0.0f, 0.0f };
        hasPrevPos_ = false;
    }
    prevPos_ = currentPos;

    // 無敵時間タイマーおよび被弾点滅演出の更新
    if (invincibilityTimer_ > 0) {
        invincibilityTimer_--;
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            if (invincibilityTimer_ == 0) {
                mr->SetColor(originalColor_);
            } else {
                bool isFlash = ((invincibilityTimer_ / kDamageFlashCycle) % 2 == 0);
                Vector4 flashColor = { 1.0f, 0.2f, 0.2f, 0.5f }; // 被弾点滅（半透明赤色）
                mr->SetColor(isFlash ? flashColor : originalColor_);
            }
        }
    }

    // 連射間隔タイマーの更新
    if (shotIntervalTimer_ > 0) {
        shotIntervalTimer_--;
    }

    // リロードタイマーの更新
    if (reloadTimer_ > 0) {
        reloadTimer_--;
        if (reloadTimer_ == 0) {
            ammo_ = maxAmmo_;
        }
    }

    // マズルフラッシュの減衰
    if (muzzleFlashTimer_ > 0) {
        muzzleFlashTimer_--;
    }

    // 入力処理（リロード & 射撃: 走行中のみ実行可能とし、描画中の誤射を防止）
    if (isAutoMoving_) {
        HandleReload();
        HandleShooting();
    }

    // 手持ちGunモデルのTransform更新（カメラ追従、射撃方向同期、首振り自動格納、リコイル演出）
    UpdateGunTransform();

    // 銃口専用GPUパーティクルエミッターのシミュレーション更新
    if (muzzleGpuParticle_) {
        muzzleGpuParticle_->Update(kDefaultDeltaTime);
    }
}

/// <summary>
/// リロードを開始し、メカニカル装填音の再生とリロードタイマーを設定します。
/// </summary>
void PlayerControllerScript::StartReload() {
    if (reloadTimer_ > 0 || ammo_ >= maxAmmo_) return;

    reloadTimer_ = reloadDuration_;
    if (auto* audio = Audio::GetInstance()) {
        if (isSeReloadLoaded_) {
            audio->PlaySoundW(seReload_, kReloadVolume, false);
        }
    }
}

/// <summary>
/// リロードキー（Rキー）の押下を検知し、リロードタイマーを開始します。
/// </summary>
void PlayerControllerScript::HandleReload() {
    // エリアクリア演出中はリロード不可
    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsAreaClearing()) return;
    }

    auto* input = InputResource::GetInput();
    if (!input) return;

    if (reloadTimer_ <= 0 && ammo_ < maxAmmo_) {
        if (input->Trigger(kReloadKey)) {
            StartReload();
        }
    }
}

/// <summary>
/// マウスの左クリック（単発）または長押し（連射）を監視し、
/// 連射インターバルと残弾数（または無限弾フラグ）の条件を満たしている場合に射撃を実行します。
/// 弾切れ時またはリロード中のクリック時は空撃ち演出（「カッ」という撃針クリック音＋微振動）を行います。
/// </summary>
void PlayerControllerScript::HandleShooting() {
    // エリアクリア演出中は射撃不可
    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsAreaClearing()) return;
    }

    auto* input = InputResource::GetInput();
    if (!input) return;

    bool isTrigger = input->MouseTrigger(kShotMouseKey);
    bool isPress = input->MousePress(kShotMouseKey);

    if (isTrigger || isPress) {
        bool canShoot = shotIntervalTimer_ <= 0 && (infiniteAmmo_ || (reloadTimer_ <= 0 && ammo_ > 0));
        if (canShoot) {
            Fire();
        }
        // 弾切れ時またはリロード中の空撃ち演出（単発クリックトリガー時のみ「カッ」と乾いた撃針クリック音を再生）
        else if (isTrigger && !infiniteAmmo_ && (ammo_ <= 0 || reloadTimer_ > 0)) {
            if (auto* audio = Audio::GetInstance()) {
                if (isSeDryFireLoaded_) {
                    audio->PlaySoundW(seDryFire_, kDryFireVolume, false);
                }
            }
            dryFireShakeProgress_ = 1.0f; // 空撃ちの微小ピクッ反動
        }
    }
}

/// <summary>
/// 画面中央のレティクル視線軸（rayDir）に沿って100%まっすぐ直進する弾丸を射出し、
/// 銃口先端ではGPUパーティクル火花と瞬間閃光によるマズルフラッシュを炸裂させます（APEXスタイル）。
/// </summary>
void PlayerControllerScript::Fire() {
    if (!infiniteAmmo_) {
        ammo_--;
        // 30発撃ち切ったら自動でリロード開始！（オートリロード）
        if (ammo_ <= 0) {
            ammo_ = 0;
            StartReload();
        }
    }
    shotIntervalTimer_ = shotInterval_;

    // 1. カメラと画面中央レティクルの視線ベクトル（rayDir）を取得
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;

    Vector3 rayStart = { 0.0f, 0.0f, 0.0f };
    Vector3 rayDir = direction_;

    if (camera) {
        float clientW = static_cast<float>(WindowApp::kClientWidth);
        float clientH = static_cast<float>(WindowApp::kClientHeight);
        rayStart = camera->GetPosition();
        camera->CreateRay(Vector2{ clientW * 0.5f, clientH * 0.5f }, clientW, clientH, rayStart, rayDir);
    }

    // 2. 弾丸のスポーン位置および射出方向の計算
    // 銃が見えている場合は手持ち銃の銃口先端（GetMuzzleWorldPosition）からレティクル目標（GetAimTargetPosition）へ射出！
    Vector3 bulletSpawnPos;
    Vector3 shootDir;

    if (isGunVisible_) {
        bulletSpawnPos = GetMuzzleWorldPosition();
        Vector3 aimTarget = GetAimTargetPosition();
        Vector3 diff = { aimTarget.x - bulletSpawnPos.x, aimTarget.y - bulletSpawnPos.y, aimTarget.z - bulletSpawnPos.z };
        float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (distSq > kEpsilon) {
            float invDist = 1.0f / std::sqrt(distSq);
            shootDir = { diff.x * invDist, diff.y * invDist, diff.z * invDist };
        } else {
            shootDir = rayDir;
        }
    } else {
        bulletSpawnPos = {
            rayStart.x + rayDir.x * kBulletCameraForwardOffset,
            rayStart.y + rayDir.y * kBulletCameraForwardOffset,
            rayStart.z + rayDir.z * kBulletCameraForwardOffset
        };
        shootDir = rayDir;
    }

    // 3. 弾丸プレハブの実体化（VALORANT/APEX風の細長い曳光ビームライン）
    GameObject* bulletObj = ScriptComponent::Instantiate("Bullet", bulletSpawnPos);
    if (bulletObj) {
        auto* bulletScript = bulletObj->GetComponent<BulletScript>();
        if (bulletScript) {
            bulletScript->SetDirection(shootDir); // 銃口からレティクル目標地点へまっすぐ直進！
            bulletScript->SetSpeed(bulletSpeed_);
            bulletScript->SetInheritedVelocity(enableInertia_ ? currentVelocity_ : Vector3{ 0.0f, 0.0f, 0.0f }); // 自機移動慣性同期
            bulletScript->SetVisualScale(bulletVisualScale_);         // 弾丸の見た目スケール（細長い曳光ビーム）
            bulletScript->SetCollisionRadius(bulletCollisionRadius_); // 見た目より広めの快適な当たり判定
            bulletScript->SetTrailColors(bulletSparkStartColor_, bulletSparkEndColor_); // 白熱ゴールド〜赤橙火花トレイル
            bulletScript->SetIsPlayerBullet(true);
        }
        // 弾丸カラーを高輝度ゴールド曳光色に設定
        if (auto* mr = bulletObj->GetComponent<MeshRendererComponent>()) {
            mr->SetColor(kPlayerBulletColor);
        }
    }

    // 射撃反動のカメラシェイク演出
    ShakeCamera(kShootShakeIntensity, kShootShakeDuration);

    // 銃手持ちモデルの反動（キックバック）演出をトリガー
    gunRecoilProgress_ = 1.0f;

    // 4. リアル・ジェット火炎 ＆ 硝煙マズルガス点火（円錐トーチコーン ＆ 前方硝煙スモーク、銃が見えている時のみ）
    if (isGunVisible_) {
        muzzleFlashTimer_ = kMuzzleFlashDuration;
        muzzleFlashRotation_ = static_cast<float>(rand() % 360) * 0.0174533f;

        // 銃口先端ワールド座標の取得
        Vector3 muzzleWorldPos = GetMuzzleWorldPosition();

        // 銃口専用GPUパーティクルによるリアル硝煙（マズルガススモーク）の噴出
        EnsureGunModel();
        if (muzzleGpuParticle_ && muzzleFlashGpuCount_ > 0) {
            Vector3 forward = rayDir;

            // 火炎先端付近から吹き出す前方オフセット位置
            Vector3 smokeSpawnPos = {
                muzzleWorldPos.x + forward.x * muzzleSmokeForwardOffset_,
                muzzleWorldPos.y + forward.y * muzzleSmokeForwardOffset_,
                muzzleWorldPos.z + forward.z * muzzleSmokeForwardOffset_
            };

            // 自機移動の慣性ベクトル（自機走行による煙の置き去りを防止し、銃口の先へ力強く推進）
            Vector3 playerVel = enableInertia_ ? currentVelocity_ : Vector3{ 0.0f, 0.0f, 0.0f };

            GPUParticleSetting s;
            s.name = "MuzzleGasSmoke";
            s.textureKey = "circle";
            s.emitterPosition = smokeSpawnPos;
            constexpr float kLifeMinRatio = 0.55f;
            s.minLifeTime = muzzleSmokeLifeMax_ * kLifeMinRatio;
            s.maxLifeTime = muzzleSmokeLifeMax_;
            s.startScale = { muzzleSmokeScaleStart_, muzzleSmokeScaleStart_, muzzleSmokeScaleStart_ };
            s.endScale = { muzzleSmokeScaleEnd_, muzzleSmokeScaleEnd_, muzzleSmokeScaleEnd_ };
            s.startColor = smokeStartColor_; // 炎光を透過した温かい赤褐色硝煙
            s.endColor = smokeEndColor_;     // 冷えたチャコール黒煙（消滅）
            s.gravity = -0.5f;               // 温かいガスが微かに立ち昇る
            s.killAtY = -999.0f;
            s.isAutoSpawning = false;
            s.spawnRatePerSec = 0.0f;
            s.blendMode = 0; // Normal (半透明合成 - リアルな黒煙)

            // 前方への力強いガス噴出初速 ＋ 横方向モクモク膨張 ＋ 自機移動慣性
            float minSpd = muzzleSmokeSpeed_ * 0.70f;
            float maxSpd = muzzleSmokeSpeed_ * 1.30f;
            s.minVelocity = {
                playerVel.x + forward.x * minSpd - muzzleSmokeSpread_,
                playerVel.y + forward.y * minSpd - muzzleSmokeSpread_,
                playerVel.z + forward.z * minSpd - muzzleSmokeSpread_
            };
            s.maxVelocity = {
                playerVel.x + forward.x * maxSpd + muzzleSmokeSpread_,
                playerVel.y + forward.y * maxSpd + muzzleSmokeSpread_,
                playerVel.z + forward.z * maxSpd + muzzleSmokeSpread_
            };

            muzzleGpuParticle_->ApplySetting(s);
            muzzleGpuParticle_->SetPosition(smokeSpawnPos);
            muzzleGpuParticle_->EmitBurst(muzzleFlashGpuCount_);
        }
    }

    // 射撃効果音 (SE) の再生
    if (auto* audio = Audio::GetInstance()) {
        audio->PlaySoundW(seShoot_, kShootVolume, false);
    }
}

/// <summary>
/// 敵弾等からの被弾によるダメージをHPに減算し、HPが0に達した場合は撃墜（死亡・溶解演出）を開始します。
/// </summary>
/// <param name="amount">ダメージ量</param>
void PlayerControllerScript::TakeDamage(int amount) {
    if (isDead_ || invincibilityTimer_ > 0) return;

    hp_ -= amount;
    invincibilityTimer_ = kDefaultInvincibilityDuration;

    // 被弾演出: 画面外周の赤い靄（ダメージビネット）＆ 瞬間赤フラッシュを点灯！
    damageVignetteAlpha_ = kDamageVignetteInitialAlpha;
    damageFlashAlpha_ = kDamageFlashInitialAlpha;

    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
        isMelting_ = true;
        meltTimer_ = 0;

        if (owner_) {
            if (!isInitialScaleSaved_) {
                initialScale_ = owner_->GetScale();
                isInitialScaleSaved_ = true;
            }
            if (!isInitialPosSaved_) {
                initialPos_ = owner_->GetPosition();
                isInitialPosSaved_ = true;
            }
            // 被弾直後は被弾煙を発生
            if (auto* effectMgr = EffectManager::GetInstance()) {
                effectMgr->PlayGPUEffect(initialPos_, kHitSmokeCount);
            }
        }
    }
}

/// <summary>
/// 敵にやられた際の「とけるような死亡演出」を更新します。
/// カメラが自機正面へ回り込む間は被弾硬直・微震動を行い、正面到達後に地面へドロッと潰れて液状化消滅します。
/// </summary>
void PlayerControllerScript::UpdateDeathMeltAnimation() {
    if (!owner_) return;

    meltTimer_++;

    // フェーズ1: カメラが自機正面へ回り込むまでの待機期間（被弾硬直・微震動）
    if (meltTimer_ < kCameraAlignFrames) {
        float rx = (static_cast<float>(rand()) / RAND_MAX - kRandOffsetCenter) * kDeathShakeIntensity;
        float rz = (static_cast<float>(rand()) / RAND_MAX - kRandOffsetCenter) * kDeathShakeIntensity;
        owner_->SetPosition({ initialPos_.x + rx, initialPos_.y, initialPos_.z + rz });

        // 被弾による赤みフラッシュ
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            bool isFlash = ((meltTimer_ / kDamageFlashCycle) % 2 == 0);
            Vector4 flashColor = { 1.0f, 0.3f, 0.3f, 1.0f };
            mr->SetColor(isFlash ? flashColor : originalColor_);
        }
        return;
    }

    // カメラが正面に到達した瞬間に本格溶解のバーストエフェクトを発生
    if (meltTimer_ == kCameraAlignFrames) {
        owner_->SetPosition(initialPos_);
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(initialPos_, kMeltBurstCount);
        }
    }

    // フェーズ2: 正面アングルでの本格溶解演出
    float meltProgress = static_cast<float>(meltTimer_ - kCameraAlignFrames) /
        static_cast<float>(kDeathMeltDuration - kCameraAlignFrames);
    meltProgress = std::clamp(meltProgress, 0.0f, 1.0f);

    // 1. スケール変形（Y軸がペシャンコに潰れ、XZ軸が液状に広がる）
    float scaleY = initialScale_.y * (1.0f - meltProgress * kMeltVerticalCollapseFactor);
    float scaleXZ = 1.0f + meltProgress * kMeltHorizontalSpreadFactor;
    owner_->SetScale({ initialScale_.x * scaleXZ, scaleY, initialScale_.z * scaleXZ });

    // 2. 重心の地面沈み込み（Y方向の低下）
    Vector3 pos = owner_->GetPosition();
    if (pos.y > kMinSinkGroundY) {
        pos.y -= kMeltSinkSpeed;
        owner_->SetPosition(pos);
    }

    // 3. マテリアル色の変化（黒紫色へ変色しつつフェードアウト）
    if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
        float alpha = (1.0f - meltProgress);
        Vector4 meltColor = {
            originalColor_.x * (1.0f - meltProgress) * kMeltColorRedWeight + kMeltColorTargetRed * meltProgress,
            kMeltColorGreen,
            originalColor_.z * (1.0f - meltProgress) * kMeltColorBlueWeight + kMeltColorTargetBlue * meltProgress,
            alpha
        };
        mr->SetColor(meltColor);
    }

    // 4. 定期的な溶解泡パーティクルの噴出
    if ((meltTimer_ - kCameraAlignFrames) % kBubbleInterval == 0 && meltProgress < kMeltSmokeMaxProgress) {
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(owner_->GetPosition(), kMeltStepCount);
        }
    }

    // 5. 溶解完了時
    if (meltTimer_ >= kDeathMeltDuration) {
        isMelting_ = false;
        owner_->SetVisible(false); // 完全に溶けきって消滅
        owner_->SetPosition(initialPos_);
    }
}

void PlayerControllerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.2f, 0.7f, 1.0f, 1.0f };
    constexpr ImVec4 kActionColor = { 0.4f, 0.9f, 0.5f, 1.0f };
    constexpr float kTestBtnWidth = 200.0f;
    constexpr float kTestBtnHeight = 26.0f;

    ImGui::TextColored(kHeaderColor, "【プレイヤーコントローラー設定】");
    ImGui::Separator();

    ImGui::DragInt("最大HP##MaxHP", &maxHp_, 1, kMinHpLimit, kMaxHpLimit);
    ImGui::ProgressBar(static_cast<float>(hp_) / static_cast<float>(maxHp_), ImVec2(0.0f, 0.0f),
        std::to_string(hp_).append(" / ").append(std::to_string(maxHp_)).c_str());
    if (invincibilityTimer_ > 0) {
        constexpr ImVec4 kInvColor = { 1.0f, 0.8f, 0.2f, 1.0f };
        ImGui::TextColored(kInvColor, "★ 無敵時間中: %d フレーム", invincibilityTimer_);
    }

    ImGui::Checkbox("残弾無限(デバッグ)##InfiniteAmmo", &infiniteAmmo_);
    ImGui::DragInt("最大装弾数##MaxAmmo", &maxAmmo_, 1, kMinAmmoLimit, kMaxAmmoLimit);
    ImGui::Text("残弾数: %d / %d %s", ammo_, maxAmmo_, (reloadTimer_ > 0 ? "[Reloading...]" : ""));

    ImGui::DragInt("リロード時間(F)##ReloadTime", &reloadDuration_, 1, 1, 120);
    ImGui::DragInt("連射間隔(F)##ShotInterval", &shotInterval_, 1, 1, 60);
    ImGui::DragFloat("弾速##BulletSpeed", &bulletSpeed_, 0.05f, 0.1f, 5.0f, "%.2f");
    ImGui::DragFloat3("自機弾スケール##PlayerBulletScale", &bulletVisualScale_.x, 0.02f, 0.1f, 2.0f, "%.2f");
    ImGui::DragFloat("自機弾判定半径##PlayerBulletColRadius", &bulletCollisionRadius_, 0.02f, 0.05f, 1.5f, "%.2f m");
    ImGui::DragFloat("ルート走行速度##MoveSpeed", &moveSpeed_, 0.005f, kMinMoveSpeed, kMaxMoveSpeed, "%.3f");
    ImGui::Checkbox("自機移動慣性同期 (レティクル直進化)##EnableInertia", &enableInertia_);

    ImGui::Separator();
    ImGui::Checkbox("自動走行中##IsAutoMoving", &isAutoMoving_);
    ImGui::Text("状態: %s", isDead_ ? "撃墜(Dead)" : "健在(Alive)");

    ImGui::Spacing();
    ImGui::TextColored(kActionColor, "【射撃テスト】");
    if (ImGui::Button("テスト発射 (左クリック相当)##TestFireBtn", ImVec2(kTestBtnWidth, kTestBtnHeight))) {
        Fire();
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "【手持ち武器 (Gun Viewmodel) 設定】");
    ImGui::Checkbox("銃表示状態##IsGunVisible", &isGunVisible_);
    ImGui::DragFloat3("銃オフセット(右/下/前)##GunOffset", &gunOffset_.x, 0.01f, -2.0f, 2.0f, "%.2f m");
    ImGui::DragFloat3("銃回転角##GunRotation", &gunRotation_.x, 0.02f, -3.14f, 3.14f, "%.2f rad");
    ImGui::DragFloat3("銃スケール##GunScale", &gunScale_.x, 0.01f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat3("銃口オフセット(X/Y/Z)##MuzzleOffset", &gunMuzzleOffset_.x, 0.01f, -2.0f, 3.0f, "%.2f m");
    ImGui::DragFloat2("照準補正(左/上)##AimCorrection", &aimCorrection_.x, 0.01f, -1.0f, 1.0f, "%.2f m");
    ImGui::DragFloat("ゼロイン距離##ZeroInDistance", &zeroInDistance_, 0.5f, 5.0f, 100.0f, "%.1f m");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "【リアル・ジェット火炎 ＆ 硝煙ガス設定】");
    ImGui::DragFloat("火炎コーン長さ##FlameLength", &muzzleFlameLength_, 0.005f, 0.05f, 0.60f, "%.3f m");
    ImGui::DragFloat("火炎コーン半径##FlameRadius", &muzzleFlameRadius_, 0.002f, 0.01f, 0.20f, "%.3f m");
    ImGui::DragFloat("高温コア比率##CoreRatio", &muzzleCoreRatio_, 0.01f, 0.2f, 0.9f, "%.2f");
    ImGui::DragFloat("火炎前方位置##FlashForward", &muzzleFlashForwardOffset_, 0.005f, -0.1f, 0.3f, "%.3f m");
    ImGui::DragFloat("硝煙ガス噴出速度##SmokeSpeed", &muzzleSmokeSpeed_, 0.2f, 0.5f, 30.0f, "%.1f m/s");
    ImGui::DragFloat("硝煙モクモク拡散##SmokeSpread", &muzzleSmokeSpread_, 0.1f, 0.0f, 5.0f, "%.1f m/s");
    ImGui::DragFloat("硝煙前方位置##SmokeForward", &muzzleSmokeForwardOffset_, 0.005f, 0.0f, 0.5f, "%.3f m");
    ImGui::DragFloat("硝煙発生サイズ##SmokeScaleStart", &muzzleSmokeScaleStart_, 0.002f, 0.005f, 0.10f, "%.3f m");
    ImGui::DragFloat("硝煙拡散サイズ##SmokeScaleEnd", &muzzleSmokeScaleEnd_, 0.005f, 0.02f, 0.40f, "%.3f m");
    ImGui::DragFloat("硝煙寿命##SmokeLife", &muzzleSmokeLifeMax_, 0.01f, 0.05f, 0.60f, "%.2f s");
    int gpuFlashCount = static_cast<int>(muzzleFlashGpuCount_);
    constexpr int kMinGpuFlashCount = 0;
    constexpr int kMaxGpuFlashCount = 100;
    if (ImGui::DragInt("硝煙ガス粒子数##MuzzleGpuCount", &gpuFlashCount, 1, kMinGpuFlashCount, kMaxGpuFlashCount)) {
        muzzleFlashGpuCount_ = static_cast<uint32_t>((std::max)(kMinGpuFlashCount, gpuFlashCount));
    }
    ImGui::ColorEdit4("銃マテリアル色##GunColor", &gunColor_.x, ImGuiColorEditFlags_AlphaBar);
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "【火炎・硝煙・弾丸火花 統合カラー設定】");
    ImGui::ColorEdit4("火炎外層色##FlameOuterColor", &flameOuterColor_.x, ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("火炎核色##FlameCoreColor", &flameCoreColor_.x, ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("銃口白熱色##CoreDiscColor", &coreDiscColor_.x, ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("硝煙発生色##SmokeStartColor", &smokeStartColor_.x, ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("硝煙消滅色##SmokeEndColor", &smokeEndColor_.x, ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("弾丸火花発生色##BulletSparkStartColor", &bulletSparkStartColor_.x, ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("弾丸火花消滅色##BulletSparkEndColor", &bulletSparkEndColor_.x, ImGuiColorEditFlags_AlphaBar);

    if (ImGui::Button("銃設定初期化##ResetGunOffset")) {
        gunOffset_ = kGunDefaultOffset;
        gunRotation_ = kGunDefaultRotation;
        gunScale_ = kGunDefaultScale;
        gunColor_ = kGunDefaultColor;
        gunMuzzleOffset_ = kDefaultGunMuzzleOffset;
        aimCorrection_ = kDefaultAimCorrection;
        zeroInDistance_ = kDefaultZeroInDistance;
        muzzleFlameLength_ = kDefaultMuzzleFlameLength;
        muzzleFlameRadius_ = kDefaultMuzzleFlameRadius;
        muzzleCoreRatio_ = kDefaultMuzzleCoreRatio;
        muzzleFlashForwardOffset_ = kDefaultMuzzleFlashForwardOffset;
        muzzleSmokeSpeed_ = kDefaultMuzzleSmokeSpeed;
        muzzleSmokeSpread_ = kDefaultMuzzleSmokeSpread;
        muzzleSmokeForwardOffset_ = kDefaultMuzzleSmokeForwardOffset;
        muzzleSmokeScaleStart_ = kDefaultMuzzleSmokeScaleStart;
        muzzleSmokeScaleEnd_ = kDefaultMuzzleSmokeScaleEnd;
        muzzleSmokeLifeMax_ = kDefaultMuzzleSmokeLifeMax;
        muzzleFlashGpuCount_ = kDefaultMuzzleFlashGpuParticleCount;
        bulletVisualScale_ = kDefaultBulletVisualScale;
        bulletCollisionRadius_ = kDefaultBulletCollisionRadius;
        flameOuterColor_ = kDefaultFlameOuterColor;
        flameCoreColor_ = kDefaultFlameCoreColor;
        coreDiscColor_ = kDefaultCoreDiscColor;
        smokeStartColor_ = kDefaultSmokeStartColor;
        smokeEndColor_ = kDefaultSmokeEndColor;
        bulletSparkStartColor_ = kDefaultBulletSparkStart;
        bulletSparkEndColor_ = kDefaultBulletSparkEnd;
    }
#endif
}

void PlayerControllerScript::EnsureGunModel() {
    if (!gunModel_) {
        gunModel_ = std::make_unique<ModelObject>();
        gunModel_->SetAutoRegisterHierarchy(false);
        gunModel_->Initialize(2); // 陰影・ライティング有効
        gunModel_->SetColor(gunColor_);
        gunModel_->SetScale(gunScale_);
    }

    // 提示画像スタイル 円錐ジェット火炎（3D多層構造）オブジェクト群の生成
    if (!muzzleFlameOuter_) {
        muzzleFlameOuter_ = std::make_unique<ConeObject>();
        muzzleFlameOuter_->SetAutoRegisterHierarchy(false);
        muzzleFlameOuter_->SetRadius(1.0f);
        muzzleFlameOuter_->SetHeight(1.0f);
        muzzleFlameOuter_->Initialize(0); // 非ライティング（鮮烈自発光）
        muzzleFlameOuter_->SetColor(flameOuterColor_);
    }
    if (!muzzleFlameCore_) {
        muzzleFlameCore_ = std::make_unique<ConeObject>();
        muzzleFlameCore_->SetAutoRegisterHierarchy(false);
        muzzleFlameCore_->SetRadius(1.0f);
        muzzleFlameCore_->SetHeight(1.0f);
        muzzleFlameCore_->Initialize(0); // 非ライティング（超高輝度発光）
        muzzleFlameCore_->SetColor(flameCoreColor_);
    }
    if (!muzzleCoreDisc_) {
        constexpr uint32_t kCoreDivide = 16;
        constexpr float kCoreOuter = 1.0f;
        constexpr float kCoreInner = 0.0f;
        muzzleCoreDisc_ = std::make_unique<FlatRingObject>(kCoreDivide, kCoreOuter, kCoreInner);
        muzzleCoreDisc_->SetAutoRegisterHierarchy(false);
        muzzleCoreDisc_->Initialize(0);
        muzzleCoreDisc_->SetColor(coreDiscColor_);
    }

    // 銃口専用GPU硝煙（マズルガス）パーティクルエミッターの初期化
    if (!muzzleGpuParticle_) {
        muzzleGpuParticle_ = std::make_unique<GPUParticleObject>();
        muzzleGpuParticle_->Initialize(kDefaultMuzzleMaxParticles);
    }

    // リロード時マガジン3Dモデル（CubeObject）の初期化
    if (!magazineModel_) {
        magazineModel_ = std::make_unique<CubeObject>();
        magazineModel_->SetAutoRegisterHierarchy(false);
        magazineModel_->Initialize(2); // ライティング有効
        magazineModel_->SetSize(kMagazineSize);
        magazineModel_->SetColor(kMagazineColor);
    }
}

void PlayerControllerScript::UpdateGunTransform() {
    EnsureGunModel();
    if (!gunModel_) return;

    // エリアクリア演出中はFPS手持ち銃およびマガジンを非表示化（三人称カメラの視界に浮遊して残るのを防止）
    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsAreaClearing()) {
            isGunVisible_ = false;
            gunModel_->SetScale({ 0.0f, 0.0f, 0.0f });
            gunModel_->Update();
            if (magazineModel_) {
                magazineModel_->SetScale({ 0.0f, 0.0f, 0.0f });
                magazineModel_->Update();
            }
            return;
        }
    }

    // リコイル回復 (スムーズに0へ収束)
    if (gunRecoilProgress_ > 0.001f) {
        gunRecoilProgress_ += (0.0f - gunRecoilProgress_) * kGunRecoilRecovery;
    } else {
        gunRecoilProgress_ = 0.0f;
    }

    // 空撃ち微振動回復
    if (dryFireShakeProgress_ > 0.001f) {
        dryFireShakeProgress_ += (0.0f - dryFireShakeProgress_) * 0.25f;
    } else {
        dryFireShakeProgress_ = 0.0f;
    }

    // FPS視点用のアクティブカメラを取得
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    Vector3 camPos = camera->GetPosition();
    Vector3 camRot = camera->GetRotation();

    // 銃は常時表示（APEXスタイルの全周交戦のため画面下に隠さない）
    isGunVisible_ = true;

    // カメラ回転行列から右・上・前基底ベクトルを計算
    Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
    Vector3 camRight   = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 camUp      = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
    Vector3 camForward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };

    // リコイルおよび空撃ちによる銃の手前への後退および銃口の跳ね上がり
    float recoilKick = gunRecoilProgress_ * kGunRecoilKickbackZ;
    float recoilPitch = gunRecoilProgress_ * kGunRecoilPitch;
    float dryKick = dryFireShakeProgress_ * kDryFireKickbackZ;
    float dryPitch = dryFireShakeProgress_ * kDryFireKickPitch;

    // リロード演出モーション（マガジン抜き取り・装填・ボルト操作の滑らかな姿勢変化）
    float reloadOffsetY = 0.0f;
    float reloadPitch = 0.0f;
    float reloadRoll = 0.0f;

    if (reloadTimer_ > 0 && reloadDuration_ > 0) {
        float t = 1.0f - static_cast<float>(reloadTimer_) / static_cast<float>(reloadDuration_);
        // サイン波カーブによるマガジン交換時の下傾き
        float curve = std::sin(t * kPi);
        reloadOffsetY = curve * kReloadDownOffsetY;
        reloadPitch   = curve * kReloadTiltPitch;
        reloadRoll    = curve * kReloadTiltRoll;

        // 装填完了時（t=0.55付近）の小さなカチャッという突き上げノック
        if (t >= 0.48f && t <= 0.65f) {
            float tapT = (t - 0.48f) / 0.17f;
            float tapKnock = std::sin(tapT * kPi) * 0.02f;
            reloadOffsetY += tapKnock;
        }
    }

    float totalKick = recoilKick + dryKick;
    float totalOffsetY = gunOffset_.y + reloadOffsetY;

    // カメラローカル座標からワールド座標へ変換
    Vector3 gunWorldPos = {
        camPos.x + camRight.x * gunOffset_.x + camUp.x * totalOffsetY + camForward.x * (gunOffset_.z + totalKick),
        camPos.y + camRight.y * gunOffset_.x + camUp.y * totalOffsetY + camForward.y * (gunOffset_.z + totalKick),
        camPos.z + camRight.z * gunOffset_.x + camUp.z * totalOffsetY + camForward.z * (gunOffset_.z + totalKick)
    };

    // 3. 弾の撃つ方向（レティクル視線レイ rayDir）と銃の向きを完全一致化
    float clientW = static_cast<float>(WindowApp::kClientWidth);
    float clientH = static_cast<float>(WindowApp::kClientHeight);
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    Vector2 scaledMousePos = mousePos;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
        scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
    }

    Vector3 rayStart = camPos;
    Vector3 rayDir = direction_;
    camera->CreateRay(Vector2{ clientW * 0.5f, clientH * 0.5f }, clientW, clientH, rayStart, rayDir);

    float hLen = std::sqrt(rayDir.x * rayDir.x + rayDir.z * rayDir.z);
    float bulletYaw = std::atan2(rayDir.x, rayDir.z);
    float bulletPitch = -std::atan2(rayDir.y, hLen > kEpsilon ? hLen : kEpsilon);

    // 銃の回転を弾丸の射出方向（bulletPitch, bulletYaw）およびリコイル・リロード姿勢と同期
    Vector3 gunWorldRot = {
        bulletPitch + gunRotation_.x + recoilPitch + dryPitch + reloadPitch,
        bulletYaw + gunRotation_.y,
        camRot.z + gunRotation_.z + reloadRoll
    };

    gunModel_->SetPosition(gunWorldPos);
    gunModel_->SetRotate(gunWorldRot);
    gunModel_->SetScale(gunScale_);
    gunModel_->SetColor(gunColor_);
    gunModel_->Update();

    // 3Dマガジン交換アニメーションの更新
    if (magazineModel_) {
        if (reloadTimer_ > 0 && reloadDuration_ > 0 && isGunVisible_) {
            float t = 1.0f - static_cast<float>(reloadTimer_) / static_cast<float>(reloadDuration_);

            // 銃の姿勢に応じた基底ベクトル
            Matrix4x4 gunRotM = Math::MakeRotateMatrix(gunWorldRot.x, gunWorldRot.y, gunWorldRot.z);
            Vector3 gunR = { gunRotM.m[0][0], gunRotM.m[0][1], gunRotM.m[0][2] };
            Vector3 gunU = { gunRotM.m[1][0], gunRotM.m[1][1], gunRotM.m[1][2] };
            Vector3 gunF = { gunRotM.m[2][0], gunRotM.m[2][1], gunRotM.m[2][2] };

            Vector3 baseMagPos = {
                gunWorldPos.x + gunR.x * kMagRelativeOffset.x + gunU.x * kMagRelativeOffset.y + gunF.x * kMagRelativeOffset.z,
                gunWorldPos.y + gunR.y * kMagRelativeOffset.x + gunU.y * kMagRelativeOffset.y + gunF.y * kMagRelativeOffset.z,
                gunWorldPos.z + gunR.z * kMagRelativeOffset.x + gunU.z * kMagRelativeOffset.y + gunF.z * kMagRelativeOffset.z
            };

            // 前半 (0.00 〜 0.38): 旧マガジンが銃底から外れて落下！
            if (t < 0.38f) {
                float dropT = t / 0.38f;
                float dropDist = dropT * dropT * kMagDropDist;
                Vector3 magPos = {
                    baseMagPos.x - gunU.x * dropDist,
                    baseMagPos.y - gunU.y * dropDist,
                    baseMagPos.z - gunU.z * dropDist
                };
                magazineModel_->SetPosition(magPos);
                magazineModel_->SetRotate(gunWorldRot);
                magazineModel_->SetScale({ 1.0f, 1.0f, 1.0f });
                magazineModel_->SetColor(kMagazineColor);
                magazineModel_->Update();
            }
            // 中盤〜後半 (0.42 〜 0.68): 新マガジンが下から上がってきて銃底へ差し込まれる！
            else if (t >= 0.42f && t < 0.68f) {
                float insertT = (t - 0.42f) / 0.26f;
                float easeInsert = insertT * insertT;
                float remainDist = (1.0f - easeInsert) * kMagInsertStartDist;
                Vector3 magPos = {
                    baseMagPos.x - gunU.x * remainDist,
                    baseMagPos.y - gunU.y * remainDist,
                    baseMagPos.z - gunU.z * remainDist
                };
                magazineModel_->SetPosition(magPos);
                magazineModel_->SetRotate(gunWorldRot);
                magazineModel_->SetScale({ 1.0f, 1.0f, 1.0f });
                magazineModel_->SetColor(kMagazineColor);
                magazineModel_->Update();
            }
            // 0.68以降: 完全に銃内部に収まったため非表示
            else {
                magazineModel_->SetScale({ 0.0f, 0.0f, 0.0f });
                magazineModel_->Update();
            }
        } else {
            magazineModel_->SetScale({ 0.0f, 0.0f, 0.0f });
            magazineModel_->Update();
        }
    }

    // VALORANT風マズルフラッシュのTransform更新（銃が表示されている時のみ）
    if (muzzleFlashTimer_ > 0 && isGunVisible_) {
        Vector3 muzzleWorldPos = GetMuzzleWorldPosition();

        // 銃の向いている前方方向
        Matrix4x4 gunRotMat = Math::MakeRotateMatrix(gunWorldRot.x, gunWorldRot.y, gunWorldRot.z);
        Vector3 gunForward = { gunRotMat.m[2][0], gunRotMat.m[2][1], gunRotMat.m[2][2] };

        Vector3 flashPos = {
            muzzleWorldPos.x + gunForward.x * muzzleFlashForwardOffset_,
            muzzleWorldPos.y + gunForward.y * muzzleFlashForwardOffset_,
            muzzleWorldPos.z + gunForward.z * muzzleFlashForwardOffset_
        };

        float progress = static_cast<float>(muzzleFlashTimer_) / static_cast<float>(kMuzzleFlashDuration);
        constexpr float kHalfPi = 1.5707963f;

        // 1. アウター円錐火炎コーン（鮮烈な炎オレンジ、前方に向かって尖る円錐）
        float outerLen = muzzleFlameLength_ * progress;
        float outerRad = muzzleFlameRadius_ * progress;
        Vector3 outerCenter = {
            flashPos.x + gunForward.x * (outerLen * 0.5f),
            flashPos.y + gunForward.y * (outerLen * 0.5f),
            flashPos.z + gunForward.z * (outerLen * 0.5f)
        };
        if (muzzleFlameOuter_) {
            muzzleFlameOuter_->SetPosition(outerCenter);
            muzzleFlameOuter_->SetScale({ outerRad, outerLen, outerRad });
            muzzleFlameOuter_->SetRotate({ gunWorldRot.x + kHalfPi, gunWorldRot.y, gunWorldRot.z + muzzleFlashRotation_ });
            muzzleFlameOuter_->SetColor(flameOuterColor_);
            muzzleFlameOuter_->Update();
        }

        // 2. インナー高温コアコーン（白〜黄色に輝く超高温核）
        float coreLen = outerLen * muzzleCoreRatio_;
        float coreRad = outerRad * muzzleCoreRatio_;
        Vector3 coreCenter = {
            flashPos.x + gunForward.x * (coreLen * 0.5f),
            flashPos.y + gunForward.y * (coreLen * 0.5f),
            flashPos.z + gunForward.z * (coreLen * 0.5f)
        };
        if (muzzleFlameCore_) {
            muzzleFlameCore_->SetPosition(coreCenter);
            muzzleFlameCore_->SetScale({ coreRad, coreLen, coreRad });
            muzzleFlameCore_->SetRotate({ gunWorldRot.x + kHalfPi, gunWorldRot.y, gunWorldRot.z + muzzleFlashRotation_ });
            muzzleFlameCore_->SetColor(flameCoreColor_);
            muzzleFlameCore_->Update();
        }

        // 3. 銃口白熱発光ディスク（銃口リムの白熱リング）
        if (muzzleCoreDisc_) {
            muzzleCoreDisc_->SetPosition(flashPos);
            muzzleCoreDisc_->SetScale({ outerRad * 1.15f, outerRad * 1.15f, 1.0f });
            muzzleCoreDisc_->SetRotate({ gunWorldRot.x, gunWorldRot.y, gunWorldRot.z + muzzleFlashRotation_ });
            muzzleCoreDisc_->SetColor(coreDiscColor_);
            muzzleCoreDisc_->Update();
        }
    }
}

Vector3 PlayerControllerScript::GetMuzzleWorldPosition() const {
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;
    if (!camera) {
        if (owner_) {
            Vector3 pos = owner_->GetPosition();
            return { pos.x, pos.y + kDefaultPlayerMuzzleHeight, pos.z };
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    Vector3 camPos = camera->GetPosition();
    Vector3 camRot = camera->GetRotation();

    Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
    Vector3 camRight   = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 camUp      = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
    Vector3 camForward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };

    float recoilKick = gunRecoilProgress_ * kGunRecoilKickbackZ;
    float recoilPitch = gunRecoilProgress_ * kGunRecoilPitch;

    Vector3 gunWorldPos = {
        camPos.x + camRight.x * gunOffset_.x + camUp.x * gunOffset_.y + camForward.x * (gunOffset_.z + recoilKick),
        camPos.y + camRight.y * gunOffset_.x + camUp.y * gunOffset_.y + camForward.y * (gunOffset_.z + recoilKick),
        camPos.z + camRight.z * gunOffset_.x + camUp.z * gunOffset_.y + camForward.z * (gunOffset_.z + recoilKick)
    };

    float clientW = static_cast<float>(WindowApp::kClientWidth);
    float clientH = static_cast<float>(WindowApp::kClientHeight);
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    Vector2 scaledMousePos = mousePos;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
        scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
    }

    Vector3 rayStart = camPos;
    Vector3 rayDir = direction_;
    camera->CreateRay(Vector2{ clientW * 0.5f, clientH * 0.5f }, clientW, clientH, rayStart, rayDir);

    float hLen = std::sqrt(rayDir.x * rayDir.x + rayDir.z * rayDir.z);
    float bulletYaw = std::atan2(rayDir.x, rayDir.z);
    float bulletPitch = -std::atan2(rayDir.y, hLen > kEpsilon ? hLen : kEpsilon);

    Vector3 gunWorldRot = {
        bulletPitch + gunRotation_.x + recoilPitch,
        bulletYaw + gunRotation_.y,
        camRot.z + gunRotation_.z
    };

    // 銃モデルの完全なワールドアフィン変換行列（Scale * Rotate * Translate）を構築
    Matrix4x4 gunWorldMatrix = Math::MakeAffineMatrix(gunScale_, gunWorldRot, gunWorldPos);

    // gun.obj の銃口先端ローカル座標をワールド座標へ射影変換
    return Math::Transform(gunWorldMatrix, gunMuzzleOffset_);
}

Vector3 PlayerControllerScript::GetAimTargetPosition() const {
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;
    if (!camera) {
        if (owner_) {
            Vector3 pos = owner_->GetPosition();
            return { pos.x + direction_.x * zeroInDistance_, pos.y + kDefaultPlayerMuzzleHeight, pos.z + direction_.z * zeroInDistance_ };
        }
        return { 0.0f, 0.0f, zeroInDistance_ };
    }

    Vector3 camPos = camera->GetPosition();
    Vector3 camRot = camera->GetRotation();
    Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
    Vector3 camRight = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 camUp    = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    float clientW = static_cast<float>(WindowApp::kClientWidth);
    float clientH = static_cast<float>(WindowApp::kClientHeight);
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    Vector2 scaledMousePos = mousePos;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
        scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
    }

    Vector3 rayStart = camPos;
    Vector3 rayDir = direction_;
    camera->CreateRay(Vector2{ clientW * 0.5f, clientH * 0.5f }, clientW, clientH, rayStart, rayDir);

    // 照準レイ前方 zeroInDistance_ をエイム目標地点の基準とする
    Vector3 rawTargetPos = {
        rayStart.x + rayDir.x * zeroInDistance_,
        rayStart.y + rayDir.y * zeroInDistance_,
        rayStart.z + rayDir.z * zeroInDistance_
    };

    // カメラ基準のオフセット補正（デフォルト 0 でレティクル中心軸と完全一致）
    return {
        rawTargetPos.x + camRight.x * aimCorrection_.x + camUp.x * aimCorrection_.y,
        rawTargetPos.y + camRight.y * aimCorrection_.x + camUp.y * aimCorrection_.y,
        rawTargetPos.z + camRight.z * aimCorrection_.x + camUp.z * aimCorrection_.y
    };
}

void PlayerControllerScript::Draw() {
    if (isDead_ || isMelting_) return;

    // 走行戦闘中（isAutoMoving_ == true）の一人称視点時かつ銃が表示状態の時のみ描画
    if (!isAutoMoving_ || !isGunVisible_) return;

    EnsureGunModel();
    if (gunModel_) {
        gunModel_->Draw(kGunModelKey, "white");
    }

    // リロード中の3Dマガジンモデル描画
    if (magazineModel_ && reloadTimer_ > 0 && isGunVisible_) {
        magazineModel_->Draw("white");
    }

    // 提示画像スタイル 3D円錐ジェット火炎描画（射撃瞬間のみ点灯）
    if (muzzleFlashTimer_ > 0) {
        if (muzzleFlameOuter_) muzzleFlameOuter_->Draw("white");
        if (muzzleFlameCore_) muzzleFlameCore_->Draw("white");
        if (muzzleCoreDisc_) muzzleCoreDisc_->Draw("white");
    }

    // 銃口専用GPU硝煙（マズルガス）パーティクルの描画
    if (muzzleGpuParticle_) {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                auto* cmdList = dxCommon->GetCommandList();
                muzzleGpuParticle_->DispatchCompute(cmdList);
                muzzleGpuParticle_->Draw(cmdList, "circle", ParticleBlendMode::Normal);
            }
        }
    }
}

void PlayerControllerScript::EnsureDamageVisuals() {
    if (!damageFlashSprite_) {
        damageFlashSprite_ = std::make_unique<SpriteObject>();
        damageFlashSprite_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(damageFlashSprite_.get());
    }
    if (!damageVignetteSprite_) {
        damageVignetteSprite_ = std::make_unique<SpriteObject>();
        damageVignetteSprite_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(damageVignetteSprite_.get());
    }
}

void PlayerControllerScript::Draw2D() {
    if (damageFlashAlpha_ <= 0.005f && damageVignetteAlpha_ <= 0.005f) {
        return;
    }

    EnsureDamageVisuals();

    constexpr float kScreenWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kScreenHeight = static_cast<float>(WindowApp::kClientHeight);

    // 1. 被弾瞬間の全画面赤フラッシュ（被弾インパクトの強調）
    if (damageFlashSprite_ && damageFlashAlpha_ > 0.005f) {
        damageFlashSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        damageFlashSprite_->SetSize(kScreenWidth, kScreenHeight);
        damageFlashSprite_->GetMaterialData()->color = {
            kDamageFlashColor.x, kDamageFlashColor.y, kDamageFlashColor.z, damageFlashAlpha_
        };
        damageFlashSprite_->Update();
        damageFlashSprite_->Draw("white", true);
    }

    // 2. 画面外周の赤い靄（ダメージビネット・ブラッディスクリーン）
    if (damageVignetteSprite_ && damageVignetteAlpha_ > 0.005f) {
        damageVignetteSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        damageVignetteSprite_->SetSize(kScreenWidth, kScreenHeight);
        damageVignetteSprite_->GetMaterialData()->color = {
            kDamageVignetteColor.x, kDamageVignetteColor.y, kDamageVignetteColor.z, damageVignetteAlpha_
        };
        damageVignetteSprite_->Update();
        damageVignetteSprite_->Draw("damageVignette", true);
    }
}
