#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Stage/MapRendererScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include <cmath>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(EnemyAIScript)

namespace {
    constexpr Vector4 kFlashColor = { 1.0f, 1.0f, 0.2f, 1.0f }; // 通常被弾フラッシュ（黄色）
    constexpr Vector4 kCriticalFlashColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // クリティカル被弾フラッシュ（純白）
    constexpr float kCenterXThreshold = 0.3f; // 正面回り込み完了とみなすX距離誤差 (m)
    constexpr float kEnemyCollisionRadius = 0.8f; // 敵の建物進入防止用衝突半径 (m)
    constexpr int kCriticalMultiplier = 2;    // ヘッドショット時のダメージ倍率
    constexpr float kEpsilon = 0.0001f;

    // トランスフォーマー風アーケードターゲットサイト＆円形HPバー用カラー定数 (RGBA: 0.0f - 1.0f)
    constexpr Vector4 kHpSegmentEmptyColor       = { 35.0f / 255.0f, 40.0f / 255.0f, 50.0f / 255.0f, 0.45f }; // 削れたHP（暗色グレー枠）
    constexpr Vector4 kHpSegmentGreenColor       = { 40.0f / 255.0f, 240.0f / 255.0f, 100.0f / 255.0f, 0.95f }; // HP安全（トランスフォーマー風グリーン）
    constexpr Vector4 kHpSegmentYellowColor      = { 255.0f / 255.0f, 210.0f / 255.0f, 25.0f / 255.0f, 0.95f }; // HP注意（イエロー）
    constexpr Vector4 kHpSegmentRedColor         = { 255.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f, 0.95f };   // HP危険（レッド）
    constexpr Vector4 kBossHpSegmentColor        = { 245.0f / 255.0f, 55.0f / 255.0f, 245.0f / 255.0f, 0.95f }; // ボスHP（マゼンタ）
    constexpr Vector4 kTargetBaseRingColor       = { 25.0f / 255.0f, 30.0f / 255.0f, 40.0f / 255.0f, 0.40f };   // ベース枠リング色
    constexpr Vector4 kCrosshairNormalColor      = { 180.0f / 255.0f, 220.0f / 255.0f, 255.0f / 255.0f, 0.75f };// 通常クロスヘア（シアンホワイト）
    constexpr Vector4 kCrosshairBossColor        = { 255.0f / 255.0f, 215.0f / 255.0f, 80.0f / 255.0f, 0.85f }; // ボスクロスヘア（ゴールド）

    constexpr float kNearClipW = 0.05f;
    constexpr float kHalf = 0.5f;
    constexpr float kScreenWidth = 1280.0f;
    constexpr float kScreenHeight = 720.0f;
    constexpr float kScreenCullMargin = 80.0f;
    constexpr float kYellowHpRatioThreshold = 0.5f;
    constexpr float kRedHpRatioThreshold = 0.2f;

    // 攻撃警告・タイミング収束リング用カラー定数
    constexpr Vector4 kMarkerYellowColor     = { 255.0f / 255.0f, 200.0f / 255.0f, 30.0f / 255.0f, 0.95f }; // 警告チャージ（黄）
    constexpr Vector4 kMarkerRedColor        = { 255.0f / 255.0f, 40.0f / 255.0f, 30.0f / 255.0f, 1.0f };    // 発射直前（危険赤）
    constexpr Vector4 kMarkerWhiteFlash      = { 1.0f, 1.0f, 1.0f, 1.0f };
    const std::string kEffectHitImpact       = "Explosion";
    constexpr int kLosSampleCount            = 8; // 射線遮蔽サンプリング分割数
}

EnemyAIScript::EnemyAIScript()
    : ScriptComponent("EnemyAIScript") {
}

EnemyAIScript::~EnemyAIScript() {
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void EnemyAIScript::EnsureTargetSightVisuals() {
    if (targetBaseRing_) return;

    targetBaseRing_ = std::make_unique<Ring2DObject>();
    targetBaseRing_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(targetBaseRing_.get());

    for (int i = 0; i < kHpSegmentCount; ++i) {
        hpSegments_[i] = std::make_unique<Line2DObject>();
        hpSegments_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(hpSegments_[i].get());
    }

    attackTimingRing_ = std::make_unique<Ring2DObject>();
    attackTimingRing_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(attackTimingRing_.get());

    targetCenterDot_ = std::make_unique<Circle2DObject>();
    targetCenterDot_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(targetCenterDot_.get());

    for (int i = 0; i < 4; ++i) {
        targetCrossLines_[i] = std::make_unique<Line2DObject>();
        targetCrossLines_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(targetCrossLines_[i].get());
        targetCrossLines_[i]->SetThickness(kMarkerThickness);
    }
}

void EnemyAIScript::Draw() {
    if (isDead_ || !owner_ || !owner_->IsVisible()) return;

    // トランスフォーマー風アーケードターゲットサイト＆円形八角形HPバーを描画
    DrawTargetSight();
}

bool EnemyAIScript::HasLineOfSightToPlayer() const {
    if (!owner_ || !targetObject_) return false;

    Vector3 startPos = owner_->GetPosition();
    Vector3 endPos = targetObject_->GetPosition();

    // 敵と自機の間を均等にサンプリングし、途中に建物があるかを判定
    for (int i = 1; i < kLosSampleCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kLosSampleCount);
        Vector3 samplePoint = {
            startPos.x + (endPos.x - startPos.x) * t,
            startPos.y + (endPos.y - startPos.y) * t,
            startPos.z + (endPos.z - startPos.z) * t
        };

        if (MapRendererScript::IsInsideBuilding(samplePoint, 0.5f)) {
            return false; // 建物による射線遮蔽
        }
    }
    return true; // 射線クリア
}

void EnemyAIScript::DrawTargetSight() {
    if (!owner_ || isDead_ || !owner_->IsVisible()) return;

    auto* camMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = camMgr ? camMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    EnsureTargetSightVisuals();

    // 敵の照準中心ワールド座標（胸部・弱点）
    Vector3 enemyPos = owner_->GetPosition();
    float centerYOffset = isBoss_ ? kBossTargetCenterYOffset : kNormalTargetCenterYOffset;
    Vector3 targetWorldPos = { enemyPos.x, enemyPos.y + centerYOffset, enemyPos.z };

    Matrix4x4 viewProj = Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    float w = targetWorldPos.x * viewProj.m[0][3] + targetWorldPos.y * viewProj.m[1][3] + targetWorldPos.z * viewProj.m[2][3] + viewProj.m[3][3];
    if (w <= kNearClipW) return; // カメラ背後はカリング

    float invW = 1.0f / w;
    float ndcX = (targetWorldPos.x * viewProj.m[0][0] + targetWorldPos.y * viewProj.m[1][0] + targetWorldPos.z * viewProj.m[2][0] + viewProj.m[3][0]) * invW;
    float ndcY = (targetWorldPos.x * viewProj.m[0][1] + targetWorldPos.y * viewProj.m[1][1] + targetWorldPos.z * viewProj.m[2][1] + viewProj.m[3][1]) * invW;

    float screenX = (ndcX * kHalf + kHalf) * kScreenWidth;
    float screenY = (-ndcY * kHalf + kHalf) * kScreenHeight;

    if (screenX < -kScreenCullMargin || screenX > kScreenWidth + kScreenCullMargin ||
        screenY < -kScreenCullMargin || screenY > kScreenHeight + kScreenCullMargin) {
        return;
    }

    // 半径とパルスアニメーション
    markerPulseTimer_ += 0.08f;
    float baseRadius = isBoss_ ? kBossTargetRadius : kNormalTargetRadius;
    if (aiState_ == AiState::AttackCharge) {
        // 攻撃チャージ中はエネルギー充填で小刻みに震える
        baseRadius += std::sin(markerPulseTimer_ * 2.0f) * 1.5f;
    }

    // 1. 下地ベースリング（暗色の円形レール）
    targetBaseRing_->SetRadii(baseRadius + 1.0f, (std::max)(1.0f, baseRadius - kMarkerThickness));
    targetBaseRing_->GetMaterialData()->color = kTargetBaseRingColor;
    targetBaseRing_->SetPosition({ screenX - (baseRadius + 1.0f), screenY - (baseRadius + 1.0f), 0.0f });
    targetBaseRing_->Update();
    targetBaseRing_->Draw("white", true);

    // 2. 八角形HPセグメントゲージ（時計回り8分割）
    float hpRatio = (maxHp_ > 0) ? std::clamp(static_cast<float>(hp_) / static_cast<float>(maxHp_), 0.0f, 1.0f) : 0.0f;
    Vector4 activeHpColor = (hpRatio > kYellowHpRatioThreshold) ? kHpSegmentGreenColor :
                            ((hpRatio > kRedHpRatioThreshold) ? kHpSegmentYellowColor : kHpSegmentRedColor);
    if (isBoss_) {
        activeHpColor = kBossHpSegmentColor;
    }
    if (hitFlashTimer_ > 0) {
        activeHpColor = kMarkerWhiteFlash; // 被弾時はHPバーも白く発光
    }

    constexpr float kPi = 3.1415926535f;
    constexpr float kSegmentAngle = (2.0f * kPi) / static_cast<float>(kHpSegmentCount);
    constexpr float kHalfGap = (kSegmentAngle * kHpSegmentGapRatio) * 0.5f;
    // 開始角度は真上（12時方向 = -PI/2）
    constexpr float kStartAngle = -kPi * 0.5f;

    for (int i = 0; i < kHpSegmentCount; ++i) {
        float segStartFrac = static_cast<float>(i) / static_cast<float>(kHpSegmentCount);
        float segEndFrac = static_cast<float>(i + 1) / static_cast<float>(kHpSegmentCount);

        float a0 = kStartAngle + (static_cast<float>(i) * kSegmentAngle) + kHalfGap;
        float a1 = kStartAngle + (static_cast<float>(i + 1) * kSegmentAngle) - kHalfGap;

        Vector2 p0 = { screenX + std::cos(a0) * baseRadius, screenY + std::sin(a0) * baseRadius };
        Vector2 p1 = { screenX + std::cos(a1) * baseRadius, screenY + std::sin(a1) * baseRadius };

        hpSegments_[i]->SetPoints(p0, p1);
        hpSegments_[i]->SetThickness(kHpSegmentThickness);

        if (hpRatio >= segEndFrac) {
            // セグメント完全残存
            hpSegments_[i]->GetMaterialData()->color = activeHpColor;
        } else if (hpRatio > segStartFrac) {
            // 減少途中セグメント（アクティブ色で残存表示）
            hpSegments_[i]->GetMaterialData()->color = activeHpColor;
        } else {
            // 減少済みセグメント（暗色スレートグレー）
            hpSegments_[i]->GetMaterialData()->color = kHpSegmentEmptyColor;
        }

        hpSegments_[i]->Update();
        hpSegments_[i]->Draw("white", true);
    }

    // 3. 照準十字クロスヘア＆中心ドット
    bool isCharging = (aiState_ == AiState::AttackCharge || (aiState_ == AiState::TargetLock && toCenterGauge_ >= 0.5f));
    float attackProgress = (aiState_ == AiState::AttackCharge) ? attackGauge_ : (toCenterGauge_ * 0.4f);
    attackProgress = std::clamp(attackProgress, 0.0f, 1.0f);

    Vector4 crossColor = isBoss_ ? kCrosshairBossColor : kCrosshairNormalColor;
    if (isCharging) {
        if (attackProgress >= 0.8f) {
            bool blink = (static_cast<int>(markerPulseTimer_ * 5.0f) % 2) == 0;
            crossColor = blink ? kMarkerRedColor : kMarkerWhiteFlash;
        } else if (attackProgress >= 0.4f) {
            crossColor = kMarkerYellowColor;
        }
    }

    targetCenterDot_->SetRadius(kCenterDotRadius);
    targetCenterDot_->GetMaterialData()->color = crossColor;
    targetCenterDot_->SetPosition({ screenX - kCenterDotRadius, screenY - kCenterDotRadius, 0.0f });
    targetCenterDot_->Update();
    targetCenterDot_->Draw("white", true);

    float crossGap = baseRadius + kCenterCrosshairGap;
    float crossLen = kCenterCrosshairSize;

    // 上
    targetCrossLines_[0]->SetPoints({ screenX, screenY - crossGap }, { screenX, screenY - crossGap - crossLen });
    targetCrossLines_[0]->GetMaterialData()->color = crossColor;
    targetCrossLines_[0]->Update();
    targetCrossLines_[0]->Draw("white", true);

    // 下
    targetCrossLines_[1]->SetPoints({ screenX, screenY + crossGap }, { screenX, screenY + crossGap + crossLen });
    targetCrossLines_[1]->GetMaterialData()->color = crossColor;
    targetCrossLines_[1]->Update();
    targetCrossLines_[1]->Draw("white", true);

    // 左
    targetCrossLines_[2]->SetPoints({ screenX - crossGap, screenY }, { screenX - crossGap - crossLen, screenY });
    targetCrossLines_[2]->GetMaterialData()->color = crossColor;
    targetCrossLines_[2]->Update();
    targetCrossLines_[2]->Draw("white", true);

    // 右
    targetCrossLines_[3]->SetPoints({ screenX + crossGap, screenY }, { screenX + crossGap + crossLen, screenY });
    targetCrossLines_[3]->GetMaterialData()->color = crossColor;
    targetCrossLines_[3]->Update();
    targetCrossLines_[3]->Draw("white", true);

    // 4. 攻撃予兆・タイミング収束リング（攻撃チャージ中のみ表示）
    if (isCharging) {
        float timingRadius = kTimingMarkerStartRadius - (kTimingMarkerStartRadius - baseRadius) * attackProgress;
        attackTimingRing_->SetRadii(timingRadius, (std::max)(1.0f, timingRadius - kMarkerThickness * 1.2f));
        attackTimingRing_->GetMaterialData()->color = crossColor;
        attackTimingRing_->SetPosition({ screenX - timingRadius, screenY - timingRadius, 0.0f });
        attackTimingRing_->Update();
        attackTimingRing_->Draw("white", true);
    }
}

void EnemyAIScript::Awake() {
    hp_ = isBoss_ ? kDefaultBossHp : kDefaultNormalHp;
    maxHp_ = hp_;
    aiState_ = AiState::Approach;
    toCenterGauge_ = 0.0f;
    attackGauge_ = 0.0f;
    cooldownTimer_ = 0;
    hitFlashTimer_ = 0;
    isDead_ = false;
}

void EnemyAIScript::Start() {
    // メッシュレンダラーから初期カラーを取得
    if (owner_) {
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            originalColor_ = mr->GetColor();
        }
    }
}

void EnemyAIScript::OnUpdate() {
    if (isDead_ || !owner_) {
        return;
    }

    // 被弾フラッシュの更新
    HandleHitFlash();

    // プレイヤーの自動検索（未設定の場合）
    if (!targetObject_) {
        auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && (obj->GetName() == "Player" || obj->GetComponent<PlayerControllerScript>())) {
                    targetObject_ = obj.get();
                    targetUUID_ = obj->GetUUID();
                    break;
                }
            }
        }
    }

    // AIの更新
    UpdateAI();
}

/// <summary>
/// 敵キャラクターの自機追従、視線回転、および5段階のAIステートマシンを更新します。
/// 
/// 【ステート遷移フロー】
/// 1. カリング判定: 自機より後方（kCullDistanceBack）に置き去りにされた場合は即座に消滅。
/// 2. 視線追従: 自機の3Dワールド座標に向けて Yaw・Pitch 回転を同期。
/// 3. ステートマシン:
///    - Approach: 自機後方から等速前進。視界内（kApproachTransitionOffset）に入ると TargetLock へ遷移。
///    - TargetLock: 自機と並走しながらロックオンゲージ（toCenterGauge_）を蓄積。満タンで MoveToCenter へ遷移。
///    - MoveToCenter: プレイヤーのX軸正面へ回り込む。誤差が kCenterXThreshold 以内になると AttackCharge へ遷移。
///    - AttackCharge: 攻撃チャージゲージ（attackGauge_）を蓄積。満タンで UnavoidableAttack へ遷移。
///    - UnavoidableAttack: FireBullet() で自機へ向けて弾丸を発射後、Approach ステートへ復帰。
/// </summary>
void EnemyAIScript::UpdateAI() {
    if (!targetObject_) return;

    // 自機が走行戦闘中でない場合（ルート描画フェーズ中など）はAI行動（接近・攻撃）を停止
    auto* playerScript = targetObject_->GetComponent<PlayerControllerScript>();
    if (!playerScript || !playerScript->IsAutoMoving()) {
        return;
    }

    Vector3 pos = owner_->GetPosition();
    Vector3 playerPos = targetObject_->GetPosition();

    // 1. カリング判定（自機より後方に置き去りにされたら消滅）
    if (playerPos.z - pos.z > kCullDistanceBack) {
        isDead_ = true;
        owner_->SetVisible(false);
        return;
    }

    // 2. プレイヤーの方向を向く（ヨー・ピッチ回転）
    Vector3 toPlayer = { playerPos.x - pos.x, playerPos.y - pos.y, playerPos.z - pos.z };
    float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    float yaw = std::atan2(toPlayer.x, toPlayer.z);
    float pitch = -std::atan2(toPlayer.y, distXZ > kEpsilon ? distXZ : kEpsilon);
    owner_->SetRotate({ pitch, yaw, 0.0f });

    // 3. ステートマシンと移動制御
    // 自機前方の理想位置（停止距離）を追従し、自機に近づきすぎないよう重なりを防止
    float targetZ = playerPos.z + preferredDistanceZ_;
    float minLimitZ = playerPos.z + kMinDistanceZ;

    // 自機との絶対接近限界による重なり防止
    if (pos.z < minLimitZ) {
        pos.z = minLimitZ;
    }

    switch (aiState_) {
    case AiState::Approach: {
        // 自機前方の適正距離へ接近
        if (pos.z < targetZ) {
            pos.z += kApproachSpeedZ;
        } else {
            pos.z -= kDistanceAdjustSpeed;
        }

        if (pos.z >= playerPos.z + kApproachTransitionOffset) {
            aiState_ = AiState::TargetLock;
            toCenterGauge_ = 0.0f;
        }
        break;
    }
    case AiState::TargetLock: {
        // 並走しながら距離をキープし、中央移動チャージ
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        toCenterGauge_ += kToCenterGaugeSpeed;
        if (toCenterGauge_ >= 1.0f) {
            aiState_ = AiState::MoveToCenter;
        }
        break;
    }
    case AiState::MoveToCenter: {
        // 並走距離をキープしつつ、滑らかなLerpで自機のX正面へ回り込む
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        float diffX = playerPos.x - pos.x;
        pos.x += diffX * kMoveLerpFactorX;

        if (std::abs(diffX) <= kCenterXThreshold) {
            aiState_ = AiState::AttackCharge;
            attackGauge_ = 0.0f;
        }
        break;
    }
    case AiState::AttackCharge: {
        // 射線遮蔽チェック（自機との間に建物がある場合はチャージを中断・回り込み優先）
        if (!HasLineOfSightToPlayer()) {
            attackGauge_ = (std::max)(0.0f, attackGauge_ - kAttackGaugeSpeed * 0.5f);
            aiState_ = AiState::MoveToCenter;
            break;
        }

        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        attackGauge_ += kAttackGaugeSpeed;

        // 予備動作 1: 敵本体の高周波震動（エネルギー充填による小刻みな震え）
        float shakeAngle = attackGauge_ * 50.0f;
        float shakeOffset = std::sin(shakeAngle) * (kChargeShakeIntensity * attackGauge_);
        pos.x += shakeOffset;

        // 予備動作 2: 敵本体の発光（オレンジ〜赤白点滅）
        if (owner_ && hitFlashTimer_ == 0) {
            if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
                Vector4 chargeColor = originalColor_;
                if (attackGauge_ >= 0.8f) {
                    bool blink = (static_cast<int>(attackGauge_ * 30.0f) % 2) == 0;
                    chargeColor = blink ? kMarkerWhiteFlash : kMarkerRedColor;
                } else if (attackGauge_ >= 0.4f) {
                    chargeColor = kMarkerYellowColor;
                }
                mr->SetColor(chargeColor);
            }
        }

        if (attackGauge_ >= 1.0f) {
            aiState_ = AiState::UnavoidableAttack;
        }
        break;
    }
    case AiState::UnavoidableAttack: {
        // 弾丸発射
        FireBullet();
        aiState_ = AiState::Cooldown;
        cooldownTimer_ = cooldownDuration_;
        if (owner_ && hitFlashTimer_ == 0) {
            if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
                mr->SetColor(originalColor_);
            }
        }
        break;
    }
    case AiState::Cooldown: {
        // 攻撃後のクールタイム（距離を保って様子見・威嚇）
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        if (cooldownTimer_ > 0) {
            cooldownTimer_--;
            if (cooldownTimer_ == 0) {
                // クールタイム満了で次の攻撃サイクルへ
                aiState_ = AiState::Approach;
                toCenterGauge_ = 0.0f;
                attackGauge_ = 0.0f;
            }
        }
        break;
    }
    case AiState::Stun: {
        // 攻撃阻止スタン中（約0.75秒、大きくのけぞって完全無防備）
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed * 0.5f;

        // のけぞり姿勢（ピッチ角を後ろに倒す）
        Vector3 rot = owner_->GetRotate();
        rot.x = -0.35f; // 仰け反り
        owner_->SetRotate(rot);

        if (stunTimer_ > 0) {
            stunTimer_--;
            if (stunTimer_ == 0) {
                aiState_ = AiState::Approach;
                toCenterGauge_ = 0.0f;
                attackGauge_ = 0.0f;
                if (owner_ && hitFlashTimer_ == 0) {
                    if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
                        mr->SetColor(originalColor_);
                    }
                }
            }
        }
        break;
    }
    }

    // 建物進入防止（建物との衝突判定＆道路・開けた通りへの押し戻し）
    if (!hasPrevSafePos_) {
        prevSafePos_ = pos;
        hasPrevSafePos_ = true;
    }

    Vector3 pushout = MapRendererScript::GetBuildingPushout(pos, kEnemyCollisionRadius);
    pos.x += pushout.x;
    pos.z += pushout.z;

    // それでもなお建物内部に入ってしまう場合は、前回の安全なX座標へ復帰
    if (MapRendererScript::IsInsideBuilding(pos, kEnemyCollisionRadius * 0.5f)) {
        pos.x = prevSafePos_.x;
        Vector3 pushAgain = MapRendererScript::GetBuildingPushout(pos, kEnemyCollisionRadius);
        pos.x += pushAgain.x;
        pos.z += pushAgain.z;
    } else {
        prevSafePos_ = pos;
    }

    owner_->SetPosition(pos);
}

/// <summary>
/// 自身の前方座標から自機座標への方向ベクトルを算出し、敵弾プレハブ（Bullet）を射出します。
/// </summary>
void EnemyAIScript::FireBullet() {
    if (!owner_ || !targetObject_) return;

    Vector3 pos = owner_->GetPosition();
    Vector3 playerPos = targetObject_->GetPosition();

    Vector3 spawnPos = { pos.x, pos.y, pos.z + kBulletSpawnOffsetZ };
    Vector3 diff = { playerPos.x - spawnPos.x, playerPos.y - spawnPos.y, playerPos.z - spawnPos.z };
    float lenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    Vector3 dir = { 0.0f, 0.0f, -1.0f };
    if (lenSq > kEpsilon) {
        float invLen = 1.0f / std::sqrt(lenSq);
        dir = { diff.x * invLen, diff.y * invLen, diff.z * invLen };
    }

    GameObject* bulletObj = ScriptComponent::Instantiate("Bullet", spawnPos);
    if (bulletObj) {
        auto* bulletScript = bulletObj->GetComponent<BulletScript>();
        if (bulletScript) {
            bulletScript->SetDirection(dir);
            bulletScript->SetSpeed(kEnemyBulletSpeed);
            bulletScript->SetDamage(bulletDamage_);
            bulletScript->SetIsPlayerBullet(false); // 敵弾
        }
    }
}

/// <summary>
/// 自機弾からのダメージを適用し、頭部への命中の場合はダメージ倍率を反映します。
/// 被弾フラッシュ演出（黄色）を開始し、HPが0に達した場合は撃破（非表示）状態へ移行します。
/// </summary>
/// <param name="amount">基礎ダメージ量</param>
/// <param name="isCritical">弱点部位判定</param>
void EnemyAIScript::TakeDamage(int amount, bool isCritical) {
    if (isDead_) return;

    int finalDamage = isCritical ? amount * kCriticalMultiplier : amount;
    hp_ -= finalDamage;
    hitFlashTimer_ = kHitFlashDuration;

    if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
        mr->SetColor(isCritical ? kCriticalFlashColor : kFlashColor);
    }

    // 攻撃阻止（カウンター / アタックキャンセル）判定
    // 攻撃チャージ中（AttackCharge）またはロックオン中（TargetLock）に被弾した場合
    if (aiState_ == AiState::AttackCharge || (aiState_ == AiState::TargetLock && toCenterGauge_ > 0.4f)) {
        // チャージを強制中断
        attackGauge_ = 0.0f;
        toCenterGauge_ = 0.0f;
        aiState_ = AiState::Stun;
        stunTimer_ = kStunDuration;

        // カウンター阻止成功の強力なカメラシェイクと被弾火花
        ShakeCamera(0.12f, 0.18f);
        if (owner_) {
            EffectPlayParam cancelParam{};
            cancelParam.position = owner_->GetPosition();
            cancelParam.scale = { 2.2f, 2.2f, 2.2f };
            cancelParam.isLoop = false;
            if (auto* effectMgr = EffectManager::GetInstance()) {
                effectMgr->PlayEffect3D(kEffectHitImpact, cancelParam);
            }
        }
    }

    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
        owner_->SetVisible(false);
    }
}

/// <summary>
/// 被弾時点滅タイマーをカウントダウンし、満了時にメッシュの色を元のマテリアルカラーへ戻します。
/// </summary>
void EnemyAIScript::HandleHitFlash() {
    if (hitFlashTimer_ > 0) {
        hitFlashTimer_--;
        if (hitFlashTimer_ == 0 && owner_) {
            if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
                mr->SetColor(originalColor_);
            }
        }
    }
}

void EnemyAIScript::SetBoss(bool isBoss) {
    isBoss_ = isBoss;
    maxHp_ = isBoss ? kDefaultBossHp : kDefaultNormalHp;
    hp_ = maxHp_;
}

void EnemyAIScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.3f, 0.3f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【敵AIスクリプト設定】");
    ImGui::Separator();

    ImGui::Checkbox("ボス属性##IsBoss", &isBoss_);
    ImGui::DragInt("最大HP##MaxHP", &maxHp_, 1, 1, 500);
    ImGui::ProgressBar(static_cast<float>(hp_) / static_cast<float>(maxHp_), ImVec2(0.0f, 0.0f),
        std::to_string(hp_).append(" / ").append(std::to_string(maxHp_)).c_str());

    const char* stateNames[] = { "Approach", "TargetLock", "MoveToCenter", "AttackCharge", "UnavoidableAttack", "Cooldown", "Stun" };
    int currentStateIndex = static_cast<int>(aiState_);
    ImGui::Text("AIステート: %s", (currentStateIndex >= 0 && currentStateIndex < 7) ? stateNames[currentStateIndex] : "Unknown");
    ImGui::ProgressBar(toCenterGauge_, ImVec2(0.0f, 0.0f), "LockOn Gauge");
    ImGui::ProgressBar(attackGauge_, ImVec2(0.0f, 0.0f), "Attack Gauge");
    if (aiState_ == AiState::Cooldown) {
        float cdProgress = static_cast<float>(cooldownTimer_) / static_cast<float>(cooldownDuration_);
        ImGui::ProgressBar(cdProgress, ImVec2(0.0f, 0.0f), ("Cooldown: " + std::to_string(cooldownTimer_)).c_str());
    }

    ImGui::Separator();
    ImGui::DragFloat("理想維持距離(Z)##PreferredDistZ", &preferredDistanceZ_, 0.5f, 5.0f, 30.0f, "%.1f m");
    ImGui::DragInt("クールタイム(F)##CooldownDuration", &cooldownDuration_, 5, 10, 300);
    ImGui::DragInt("敵弾攻撃力##EnemyDamage", &bulletDamage_, 1, 1, 10);
#endif
}
