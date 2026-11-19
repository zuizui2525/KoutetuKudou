#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Scripts/Stage/MapRendererScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/3d/FlatRing/FlatRingObject.h"
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
    constexpr Vector4 kFlashColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 通常被弾フラッシュ（純白発光）
    constexpr Vector4 kCriticalFlashColor = { 1.0f, 0.25f, 0.25f, 1.0f }; // クリティカル被弾フラッシュ（鮮烈レッド）
    constexpr float kCenterXThreshold = 0.3f; // 正面回り込み完了とみなすX距離誤差 (m)
    constexpr float kEnemyCollisionRadius = 0.8f; // 敵の建物進入防止用衝突半径 (m)
    constexpr int kCriticalMultiplier = 2;    // ヘッドショット時のダメージ倍率
    constexpr float kEpsilon = 0.0001f;
    constexpr float kPi = 3.1415926535f;
    constexpr float kTwoPi = 6.283185307f;

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
    // アクティブ敵リストから自身を除外（敵同士の重なり判定用）
    auto it = std::find(sActiveEnemies_.begin(), sActiveEnemies_.end(), this);
    if (it != sActiveEnemies_.end()) {
        sActiveEnemies_.erase(it);
    }

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

    // Fortnite風 画面中央放射状目盛りサークル＆脅威インジケーター描画オブジェクトの初期化
    for (int i = 0; i < kRadialTickCount; ++i) {
        radialTicks_[i] = std::make_unique<Line2DObject>();
        radialTicks_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(radialTicks_[i].get());
    }

    threatRay_ = std::make_unique<Line2DObject>();
    threatRay_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(threatRay_.get());

    for (int i = 0; i < 2; ++i) {
        threatArrowWings_[i] = std::make_unique<Line2DObject>();
        threatArrowWings_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(threatArrowWings_[i].get());
    }

    for (int i = 0; i < kThreatArcSegmentCount; ++i) {
        threatArcBars_[i] = std::make_unique<Line2DObject>();
        threatArcBars_[i]->Initialize();
        SceneHierarchy::GetInstance()->Unregister(threatArcBars_[i].get());
    }
}

void EnemyAIScript::EnsureChargeRingVisuals() {
    if (!groundChargeRing_) {
        constexpr uint32_t kRingDivide = 32;
        groundChargeRing_ = std::make_unique<FlatRingObject>(kRingDivide, k3DChargeRingBaseOuterRadius, k3DChargeRingBaseInnerRadius);
        groundChargeRing_->SetAutoRegisterHierarchy(false);
        groundChargeRing_->Initialize(0); // 自己発光マテリアル（非ライティング）
    }
    if (!bodyChargeRing_) {
        constexpr uint32_t kRingDivide = 32;
        bodyChargeRing_ = std::make_unique<FlatRingObject>(kRingDivide, k3DChargeRingBaseOuterRadius, k3DChargeRingBaseInnerRadius);
        bodyChargeRing_->SetAutoRegisterHierarchy(false);
        bodyChargeRing_->Initialize(0); // 自己発光マテリアル（非ライティング）
    }
}

/// <summary>
/// 地上溜め（AttackCharge）から空中浮遊（PounceStrike）を経て攻撃発射に至るまでの統合攻撃進行度（0.0 〜 1.0）を取得します。
/// </summary>
float EnemyAIScript::GetOverallAttackProgress() const {
    if (aiState_ == AiState::AttackCharge) {
        // 地上チャージ中: 0.0 〜 kChargePhaseRatio (0.20)
        return std::clamp(attackGauge_, 0.0f, 1.0f) * kChargePhaseRatio;
    } else if (aiState_ == AiState::PounceStrike) {
        // 空中浮遊中: kChargePhaseRatio (0.20) 〜 1.00 (約5秒かけてゆっくり収縮)
        float t = 1.0f;
        if (kAirborneVulnerableDuration > 0) {
            t = 1.0f - static_cast<float>(strikeVulnerableTimer_) / static_cast<float>(kAirborneVulnerableDuration);
            t = std::clamp(t, 0.0f, 1.0f);
        }
        return kChargePhaseRatio + t * (1.0f - kChargePhaseRatio);
    }
    return 0.0f;
}

void EnemyAIScript::DrawChargeRings() {
    bool isAttacking = (aiState_ == AiState::AttackCharge || aiState_ == AiState::PounceStrike);
    if (!isAttacking || isDead_ || !owner_) return;

    EnsureChargeRingVisuals();

    float progress = GetOverallAttackProgress();
    // スケールを k3DChargeRingStartScale (2.8m) から k3DChargeRingEndScale (0.0m: 敵中心へ完全消失) へ収縮
    float currentScale = k3DChargeRingStartScale - (k3DChargeRingStartScale - k3DChargeRingEndScale) * progress;
    currentScale = (std::max)(0.0f, currentScale);

    // チャージ終盤（0.85以上）は白熱高速点滅
    Vector4 ringColor = k3DChargeRingColor;
    if (progress >= 0.85f) {
        bool blink = (static_cast<int>(progress * 50.0f) % 2) == 0;
        ringColor = blink ? k3DChargeRingBlinkColor : k3DChargeRingColor;
    }

    Vector3 enemyPos = owner_->GetPosition();

    // 1. 3D足元地面警告サークル（地上チャージ中は足元地面に敷いて可視化・半透明アルファブレンド）
    if (groundChargeRing_ && currentScale > 0.02f) {
        groundChargeRing_->SetPosition({ enemyPos.x, k3DChargeRingGroundY, enemyPos.z });
        constexpr float kHalfPi = 1.5707963f;
        groundChargeRing_->SetRotate({ kHalfPi, 0.0f, 0.0f });
        groundChargeRing_->SetScale({ currentScale, currentScale, 1.0f });
        groundChargeRing_->SetColor(ringColor);
        groundChargeRing_->Update();
        groundChargeRing_->Draw("white", "", "Object3D_Alpha");
    }

    // 2. 3D敵胴体正面ビルボード警告サークル（敵の真ん中のちょいカメラ側手前に配置し、半透明で収束！）
    if (bodyChargeRing_ && currentScale > 0.02f) {
        auto* camMgr = CameraResource::GetCameraManager();
        BaseCamera* camera = camMgr ? camMgr->GetActiveCamera() : nullptr;
        if (camera) {
            Vector3 camRot = camera->GetRotation();
            Vector3 camPos = camera->GetPosition();

            // 敵の中心（真ん中）座標
            float centerY = enemyPos.y + (isBoss_ ? kBossTargetCenterYOffset : kNormalTargetCenterYOffset);
            Vector3 enemyCenter = { enemyPos.x, centerY, enemyPos.z };

            // 敵中心からカメラ（自機）方向への単位ベクトルを算出
            Vector3 toCam = { camPos.x - enemyCenter.x, camPos.y - enemyCenter.y, camPos.z - enemyCenter.z };
            float distToCam = std::sqrt(toCam.x * toCam.x + toCam.y * toCam.y + toCam.z * toCam.z);

            Vector3 ringPos = enemyCenter;
            if (distToCam > kEpsilon) {
                // 敵中心からカメラ側へちょい手前にオフセット（敵モデルに埋もれず「ここを撃て」と明確化）
                ringPos.x += (toCam.x / distToCam) * k3DChargeRingForwardOffset;
                ringPos.y += (toCam.y / distToCam) * k3DChargeRingForwardOffset;
                ringPos.z += (toCam.z / distToCam) * k3DChargeRingForwardOffset;
            }

            bodyChargeRing_->SetPosition(ringPos);
            bodyChargeRing_->SetRotate(camRot);
            bodyChargeRing_->SetScale({ currentScale, currentScale, 1.0f });
            bodyChargeRing_->SetColor(ringColor);
            bodyChargeRing_->Update();
            bodyChargeRing_->Draw("white", "", "Object3D_Alpha");
        }
    }
}

/// <summary>
/// 3D描画パス: 3D空間上の攻撃予兆サークル（収縮する地面警告リング）を描画します。
/// </summary>
void EnemyAIScript::Draw() {
    if (isDead_ || !owner_ || !owner_->IsVisible()) return;

    // 3D空間攻撃予兆サークル（収縮する警告リング）を描画
    DrawChargeRings();
}

/// <summary>
/// 2D描画パス: トランスフォーマー風アーケードターゲットサイトおよび脅威インジケーターを描画します。
/// </summary>
void EnemyAIScript::Draw2D() {
    if (isDead_ || !owner_ || !owner_->IsVisible()) return;

    // レベルアップ3択選択中はターゲットサイト・脅威レーダー・幽霊ジャミングを非表示にしてUI重なりを防止
    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsLevelUpChoosing()) {
            return;
        }
    }

    // 幽霊化（AiState::Ghost）中はターゲットサイト・HPバーを消し、照準を塞ぐ視界ジャミング演出を描画
    if (IsGhost()) {
        DrawGhostJamming2D();
        return;
    }

    // トランスフォーマー風アーケードターゲットサイト＆円形八角形HPバーを描画（2Dパス）
    DrawTargetSight();

    // Fortnite風 画面中央脅威インジケーターを描画（2Dパス）
    DrawThreatIndicator();
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

bool EnemyAIScript::IsNearBuilding(const Vector3& worldPos, float radius) const {
    Vector3 sample = { worldPos.x, worldPos.y + 0.5f, worldPos.z };
    return MapRendererScript::IsInsideBuilding(sample, radius);
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
    } else if (aiState_ == AiState::PounceStrike) {
        // 空中滞空・隙露出中は巨大化した敵モデルに合わせてサイトリングもダイナミック拡大！
        baseRadius *= kAirborneBaseScaleMultiplier;
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
    bool isAttackingSequence = (aiState_ == AiState::AttackCharge || aiState_ == AiState::PounceStrike);
    float attackProgress = GetOverallAttackProgress();

    Vector4 crossColor = isBoss_ ? kCrosshairBossColor : kCrosshairNormalColor;
    if (isAttackingSequence) {
        if (attackProgress >= 0.85f) {
            bool blink = (static_cast<int>(markerPulseTimer_ * 6.0f) % 2) == 0;
            crossColor = blink ? kMarkerRedColor : kMarkerWhiteFlash;
        } else if (attackProgress >= 0.40f) {
            crossColor = kMarkerRedColor;
        } else {
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

    // 4. 攻撃予兆・タイミング収束リング（地上溜め〜空中浮遊中を通して収縮！）
    if (isAttackingSequence) {
        float timingRadius = kTimingMarkerStartRadius - (kTimingMarkerStartRadius - baseRadius) * attackProgress;
        if (timingRadius > 1.0f) {
            attackTimingRing_->SetRadii(timingRadius, (std::max)(1.0f, timingRadius - kMarkerThickness * 1.2f));
            attackTimingRing_->GetMaterialData()->color = crossColor;
            attackTimingRing_->SetPosition({ screenX - timingRadius, screenY - timingRadius, 0.0f });
            attackTimingRing_->Update();
            attackTimingRing_->Draw("white", true);
        }
    }
}

/// <summary>
/// Fortnite風 画面中央脅威＆サウンドビジュアライザーを描画します。
/// 敵がいない/溜めていない通常時でも薄いベース円が常時表示され、
/// 敵が並走・接近している時は走行音を可視化する波形ラインがホワンホワンと伸縮、
/// 攻撃溜め・空中切り込み時は赤い鋭角警告ラインがピピピッと伸びて方角を強調表示します。
/// </summary>
void EnemyAIScript::DrawThreatIndicator() {
    if (!owner_ || isDead_ || !owner_->IsVisible() || !targetObject_) return;

    auto* camMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = camMgr ? camMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    EnsureTargetSightVisuals();

    // 1. 画面中央座標
    float centerX = kScreenWidth * 0.5f;
    float centerY = kScreenHeight * 0.5f;

    // 2. 自機（カメラ）と敵の水平相対角度（方位角）の高精度算出
    Vector3 enemyPos = owner_->GetPosition();
    Vector3 cameraPos = camera->GetPosition();

    const Matrix4x4& vm = camera->GetViewMatrix();
    // カメラから敵へのワールド相対ベクトル
    Vector3 toEnemy = { enemyPos.x - cameraPos.x, enemyPos.y - cameraPos.y, enemyPos.z - cameraPos.z };
    float enemyDist = std::sqrt(toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y + toEnemy.z * toEnemy.z);

    // ビュー行列の基底を用いてカメラローカル座標系へ直接変換（100%狂いのない高精度方位角）
    float viewX = toEnemy.x * vm.m[0][0] + toEnemy.y * vm.m[1][0] + toEnemy.z * vm.m[2][0];
    float viewY = toEnemy.x * vm.m[0][1] + toEnemy.y * vm.m[1][1] + toEnemy.z * vm.m[2][1];
    float viewZ = toEnemy.x * vm.m[0][2] + toEnemy.y * vm.m[1][2] + toEnemy.z * vm.m[2][2];

    // カメラ前方（viewZ）と左右（viewX）によるスクリーン上の回転角（真上: 0, 右: +PI/2, 左: -PI/2, 後方: PI）
    float screenAngle = std::atan2(viewX, viewZ);

    // 3. 画面中央の放射状目盛りサークル（サンバースト型HUD・実線円なし・薄く常時表示）
    // 提示イラスト通り、敵がいる方角の目盛りが音波イコライザーのように波打って外側に長く伸びる！
    constexpr Vector4 kTickBaseColor = { 0.80f, 0.90f, 1.0f, kRadialTickAlpha };
    constexpr float kSoundAngleRange = 0.62f; // 音波反応する扇角（約35度）

    for (int i = 0; i < kRadialTickCount; ++i) {
        float tickAngle = (kTwoPi / static_cast<float>(kRadialTickCount)) * static_cast<float>(i);
        float tickDirX = std::sin(tickAngle);
        float tickDirY = -std::cos(tickAngle);

        float rIn = kThreatBaseRadius - kRadialTickInnerLen;
        float rOut = kThreatBaseRadius + kRadialTickOuterLen;
        Vector4 tickColor = kTickBaseColor;
        float tickThickness = 2.0f;

        // 敵の走行接近音による目盛り伸長（通常時・イラスト左上の波形表現）
        if (aiState_ == AiState::Aiming && enemyDist <= kSoundPerceptionRange) {
            float angleDiff = std::atan2(std::sin(tickAngle - screenAngle), std::cos(tickAngle - screenAngle));
            float absDiff = std::abs(angleDiff);

            if (absDiff < kSoundAngleRange) {
                float proximity = 1.0f - (absDiff / kSoundAngleRange);
                float distWeight = 1.0f - std::clamp(enemyDist / kSoundPerceptionRange, 0.0f, 1.0f);
                float wavePulse = std::abs(std::sin(markerPulseTimer_ * 5.5f + tickAngle * 2.0f));
                float extraLen = (kRadialTickPulseScale * proximity * distWeight) * (0.6f + 0.4f * wavePulse);

                rOut += extraLen;
                tickThickness = 2.0f + 1.5f * proximity;
                tickColor = { 0.35f, 0.85f, 1.0f, kRadialTickAlpha + 0.45f * proximity * distWeight };
            }
        }

        Vector2 p0 = { centerX + tickDirX * rIn, centerY + tickDirY * rIn };
        Vector2 p1 = { centerX + tickDirX * rOut, centerY + tickDirY * rOut };

        radialTicks_[i]->SetPoints(p0, p1);
        radialTicks_[i]->SetThickness(tickThickness);
        radialTicks_[i]->GetMaterialData()->color = tickColor;
        radialTicks_[i]->Update();
        radialTicks_[i]->Draw("white", true);
    }

    // 4. 攻撃チャージ中（AttackCharge）または空中切り込み中（PounceStrike）: 鮮烈な赤い警告ライン（イラスト右側）
    if (aiState_ == AiState::AttackCharge || aiState_ == AiState::PounceStrike) {
        float dirX = std::sin(screenAngle);
        float dirY = -std::cos(screenAngle);

        bool blink = (static_cast<int>(markerPulseTimer_ * 6.0f) % 2) == 0;
        Vector4 rayColor = blink ? kMarkerRedColor : kMarkerWhiteFlash;
        if (aiState_ == AiState::PounceStrike) {
            rayColor = kMarkerRedColor; // 空中隙露出中は最大警戒（鮮烈な赤）
        }

        // 4-1. 警告円周上の赤い警戒アーク（円弧ライン: 「この方角一帯が危険」と直感認識）
        constexpr float kArcSpan = 0.50f; // 約28度の弧
        float halfSpan = kArcSpan * 0.5f;
        float stepAngle = kArcSpan / static_cast<float>(kThreatArcSegmentCount);
        for (int i = 0; i < kThreatArcSegmentCount; ++i) {
            float a0 = screenAngle - halfSpan + stepAngle * static_cast<float>(i);
            float a1 = a0 + stepAngle;
            Vector2 arcP0 = { centerX + std::sin(a0) * kThreatBaseRadius, centerY - std::cos(a0) * kThreatBaseRadius };
            Vector2 arcP1 = { centerX + std::sin(a1) * kThreatBaseRadius, centerY - std::cos(a1) * kThreatBaseRadius };
            threatArcBars_[i]->SetPoints(arcP0, arcP1);
            threatArcBars_[i]->SetThickness(kThreatArcThickness);
            threatArcBars_[i]->GetMaterialData()->color = rayColor;
            threatArcBars_[i]->Update();
            threatArcBars_[i]->Draw("white", true);
        }

        // 4-2. 敵方角へ伸びる肉太な主警告線（イラスト右側の赤い線）
        float r0 = kThreatBaseRadius + kThreatRayInnerGap;
        float r1 = r0 + kThreatRayLength;
        Vector2 p0 = { centerX + dirX * r0, centerY + dirY * r0 };
        Vector2 p1 = { centerX + dirX * r1, centerY + dirY * r1 };

        threatRay_->SetPoints(p0, p1);
        threatRay_->SetThickness(kThreatRayThickness);
        threatRay_->GetMaterialData()->color = rayColor;
        threatRay_->Update();
        threatRay_->Draw("white", true);

        // 4-3. 先端のくさび型矢印両翼（アロー羽: 進行方向を力強く指し示す）
        float leftAngle = screenAngle + kThreatArrowWingAngle;
        float rightAngle = screenAngle - kThreatArrowWingAngle;
        Vector2 wingLeft = { p1.x + std::sin(leftAngle) * kThreatArrowWingLength, p1.y - std::cos(leftAngle) * kThreatArrowWingLength };
        Vector2 wingRight = { p1.x + std::sin(rightAngle) * kThreatArrowWingLength, p1.y - std::cos(rightAngle) * kThreatArrowWingLength };

        threatArrowWings_[0]->SetPoints(p1, wingLeft);
        threatArrowWings_[0]->SetThickness(kThreatRayThickness * 0.85f);
        threatArrowWings_[0]->GetMaterialData()->color = rayColor;
        threatArrowWings_[0]->Update();
        threatArrowWings_[0]->Draw("white", true);

        threatArrowWings_[1]->SetPoints(p1, wingRight);
        threatArrowWings_[1]->SetThickness(kThreatRayThickness * 0.85f);
        threatArrowWings_[1]->GetMaterialData()->color = rayColor;
        threatArrowWings_[1]->Update();
        threatArrowWings_[1]->Draw("white", true);
    }
}

/// <summary>
/// 生存している他のエネミーとの距離を判定し、衝突半径（kEnemySeparationDistance）未満であれば
/// 互いに反対方向へ押し離す分離斥力（Boids Separation / Soft Pushout）を適用して敵同士の重なりを解消します。
/// </summary>
void EnemyAIScript::ApplyEnemySeparation(Vector3& pos) {
    float sepDist = IsGhost() ? kGhostSeparationDistance : kEnemySeparationDistance;
    float minSepSq = sepDist * sepDist;

    for (auto* other : sActiveEnemies_) {
        if (!other || other == this || other->isDead_ || !other->owner_ || !other->owner_->IsVisible()) {
            continue;
        }
        // 生存敵と幽霊は互いに干渉しない（同状態同士のみ分離）
        if (other->IsGhost() != this->IsGhost()) {
            continue;
        }

        Vector3 otherPos = other->owner_->GetPosition();
        float dx = pos.x - otherPos.x;
        float dz = pos.z - otherPos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq < minSepSq && distSq > kEpsilon) {
            float dist = std::sqrt(distSq);
            float overlap = (sepDist - dist) * kEnemySeparationPushForce;
            float nx = dx / dist;
            float nz = dz / dist;
            pos.x += nx * overlap;
            pos.z += nz * overlap;
        }
    }
}

void EnemyAIScript::StartVault(float targetSideSign) {
    if (!owner_ || !targetObject_) return;
    isVaulting_ = true;
    vaultTimer_ = kVaultDuration;
    vaultStartX_ = owner_->GetPosition().x;
    vaultTargetX_ = targetObject_->GetPosition().x + targetSideSign * kSideDistanceX;
    sideSign_ = targetSideSign;
}

void EnemyAIScript::Awake() {
    hp_ = isBoss_ ? kDefaultBossHp : kDefaultNormalHp;
    maxHp_ = hp_;
    aiState_ = AiState::Aiming;
    attackGauge_ = 0.0f;
    cooldownTimer_ = 0;
    hitFlashTimer_ = 0;
    hitSquashTimer_ = 0;
    stunTimer_ = 0;
    isDead_ = false;
    flyByInitialized_ = false;
    parallelCruiseTimer_ = 0;
    strikeVulnerableTimer_ = 0;
    attackZoneIndex_ = (sNextZoneIndex_++) % 3;
    sideSign_ = 1.0f;

    // アクティブ敵リストへ登録（重複防止）
    if (std::find(sActiveEnemies_.begin(), sActiveEnemies_.end(), this) == sActiveEnemies_.end()) {
        sActiveEnemies_.push_back(this);
    }
}

void EnemyAIScript::Start() {
    // メッシュレンダラーから初期カラーを取得
    if (owner_) {
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            originalColor_ = mr->GetColor();
        }

        // マップ縮尺に美しく調和する適正スケール（0.68m）を設定
        owner_->SetScale({ kEnemyBaseScale, kEnemyBaseScale, kEnemyBaseScale });
        originalScale_ = owner_->GetScale();
        isOriginalScaleSaved_ = true;

        // 初期X座標から右並走(+1)か左並走(-1)かを決定
        sideSign_ = (owner_->GetPosition().x >= 0.0f) ? 1.0f : -1.0f;
    }
}

void EnemyAIScript::OnUpdate() {
    if (isDead_ || !owner_) {
        return;
    }

    // レベルアップ3択選択中は敵AI・攻撃チャージ・幽霊寿命タイマーを完全一時停止
    if (auto* phaseCtrl = GamePhaseControllerScript::GetInstance()) {
        if (phaseCtrl->IsLevelUpChoosing()) {
            return;
        }
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

    if (!isOriginalScaleSaved_ && owner_) {
        owner_->SetScale({ kEnemyBaseScale, kEnemyBaseScale, kEnemyBaseScale });
        originalScale_ = owner_->GetScale();
        isOriginalScaleSaved_ = true;
    }

    // 自機が走行戦闘中でない場合（ルート描画フェーズ中など）はAI行動（接近・攻撃）を停止
    auto* playerScript = targetObject_->GetComponent<PlayerControllerScript>();
    if (!playerScript || !playerScript->IsAutoMoving()) {
        return;
    }

    // 幽霊化（AiState::Ghost）している場合は専用のカメラ前方浮遊追従を実行
    if (IsGhost()) {
        UpdateGhostState();
        return;
    }

    Vector3 pos = owner_->GetPosition();
    Vector3 playerPos = targetObject_->GetPosition();

    // 1. カリング判定（自機より後方 kCullDistanceBack に置き去りにされたら消滅）
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

    // 自機との直線距離（移動制御および射線判定で共用）
    float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);

    // 3. アーキタイプ別移動制御
    if (enemyType_ == EnemyType::ParallelChaser) {
        // 【横並走＆アーケード風飛びかかり強襲型】普段は自機の横を並走し、攻撃時は目前へダイブ（壁自動回避対応）
        // 1. 壁検知と頭上飛び越え（Overhead Vault Leap）判定
        Vector3 currentSidePos = { playerPos.x + sideSign_ * kSideDistanceX, playerPos.y, playerPos.z + kParallelLeadZ };
        if (IsNearBuilding(currentSidePos, kWallCheckMargin)) {
            // 現在の側に壁がある場合、反対側が開いているかをチェック
            Vector3 oppositeSidePos = { playerPos.x - sideSign_ * kSideDistanceX, playerPos.y, playerPos.z + kParallelLeadZ };
            if (!IsNearBuilding(oppositeSidePos, kWallCheckMargin) && !isVaulting_) {
                // 地上をすり抜けるのではなく、プレイヤーの頭上を放物線で飛び越えて逆サイドへ！
                StartVault(-sideSign_);
            }
        }

        // 2. 各ステートにおける位置追従
        Vector3 prevPos = pos;
        if (aiState_ == AiState::Aiming) {
            if (isVaulting_) {
                // 【頭上飛び越えオーバーヘッドジャンプ】自機を地上ですり抜けず、頭上高くダイナミックに跳躍！
                vaultTimer_--;
                float progress = 1.0f - static_cast<float>(vaultTimer_) / static_cast<float>(kVaultDuration);
                // X軸: スムーズなS字カーブ（SmoothStep）
                float smoothT = progress * progress * (3.0f - 2.0f * progress);
                pos.x = vaultStartX_ + (vaultTargetX_ - vaultStartX_) * smoothT;
                // Y軸: プレイヤーの頭上を越える美しいサイン放物線上昇
                pos.y = kEnemyGroundYOffset + std::sin(progress * kPi) * kVaultApexHeight;
                // Z軸: 自機と同期して並走
                pos.z += (playerPos.z + kParallelLeadZ - pos.z) * kParallelLerpSpeed;

                // 飛び越え中のアクロバティックなロールバンク傾斜
                float vaultRoll = -sideSign_ * std::sin(progress * kPi) * 0.45f;
                owner_->SetRotate({ pitch, yaw, vaultRoll });

                if (vaultTimer_ <= 0) {
                    isVaulting_ = false;
                }
            } else {
                // 【横並走フェーズ】自機の真横〜わずか前方（安全な側）を並走
                // 自機より8m以上後方に取り残された場合は、ビルの角を避けるため道路中央寄りへシフト
                float sideOffset = (playerPos.z - pos.z > kCatchUpDistanceThreshold) ? (sideSign_ * 1.5f) : (sideSign_ * kSideDistanceX);
                Vector3 targetPos = { playerPos.x + sideOffset, kEnemyGroundYOffset, playerPos.z + kParallelLeadZ };

                pos.x += (targetPos.x - pos.x) * kParallelLerpSpeed;
                pos.y += (targetPos.y - pos.y) * kParallelLerpSpeed; // 地上高度維持
                pos.z += (targetPos.z - pos.z) * kParallelLerpSpeed;

                parallelCruiseTimer_++;
                // 一定時間並走後、射線が通っていれば側方溜め（AttackCharge）へ移行
                if (parallelCruiseTimer_ >= kDefaultCruiseDuration && distToPlayer <= kAmbushPerceptionRange && HasLineOfSightToPlayer()) {
                    aiState_ = AiState::AttackCharge;
                    attackGauge_ = 0.0f;
                    parallelCruiseTimer_ = 0;
                }
            }
        } else if (aiState_ == AiState::AttackCharge) {
            // 【前方中距離チャージフェーズ】自機の前方斜め（Z = +15m, X = ±5.2m）へスッと先回り展開して堂々とチャージ！
            // プレイヤーの視界が開けた正面斜めで溜めるため、「溜めている！狙い撃て！」と最高にわかりやすくなる！
            float sideOffset = sideSign_ * kChargeSideOffsetX;
            Vector3 targetPos = { playerPos.x + sideOffset, kEnemyGroundYOffset, playerPos.z + kChargeLeadZ };

            pos.x += (targetPos.x - pos.x) * kParallelLerpSpeed;
            pos.y += (targetPos.y - pos.y) * kParallelLerpSpeed;
            pos.z += (targetPos.z - pos.z) * kParallelLerpSpeed;
        } else if (aiState_ == AiState::PounceStrike) {
            // 【扇状3アタックゾーン空中跳躍＆隙露出フェーズ】
            // 狙う場所の固定化を防ぐため、敵ごとに「斜め前」「正面上空」「その場横ジャンプ」の3ゾーンへダイナミック分散！
            float offsetX = kZone1OffsetX;
            float offsetY = kZone1OffsetY;
            float offsetZ = kZone1OffsetZ;

            if (attackZoneIndex_ == 1) {
                // Zone 2: 道路中央上空（正面高高度・ビルの間）
                offsetX = kZone2OffsetX;
                offsetY = kZone2OffsetY;
                offsetZ = kZone2OffsetZ;
            } else if (attackZoneIndex_ == 2) {
                // Zone 3: 側方ジャンプ横オフセット (m)（視界内に収まる横幅）
                offsetX = kZone3OffsetX;
                offsetY = kZone3OffsetY;
                offsetZ = kZone3OffsetZ;
            }

            Vector3 targetPos = { playerPos.x + sideSign_ * offsetX, kEnemyGroundYOffset + offsetY, playerPos.z + offsetZ };
            if (IsNearBuilding(targetPos, kWallCheckMargin)) {
                targetPos.x = playerPos.x + sideSign_ * 1.5f; // 壁が近ければ道路内側へシフト
            }

            // X, Y, Z を空中へダイレクト跳躍
            pos.x += (targetPos.x - pos.x) * kAirborneLeapSpeed;
            pos.y += (targetPos.y - pos.y) * kAirborneLeapSpeed;
            pos.z += (targetPos.z - pos.z) * kAirborneLeapSpeed;

            // 「動かなくなるではない」生き生きとした滞空：ブースター浮遊微振動（ホバリング）
            float hoverY = std::sin(markerPulseTimer_ * kAirborneHoverFrequency) * kAirborneHoverAmplitude;
            pos.y += hoverY;

            // ゾーンに応じた空中姿勢制御
            if (attackZoneIndex_ == 1) {
                // 正面上空: まっすぐ見下ろすピッチ傾斜
                owner_->SetRotate({ pitch + kAirbornePitchTilt, yaw, 0.0f });
            } else {
                // 斜め前 / 側方: 前傾ピッチ ＆ バンクロール傾斜
                owner_->SetRotate({ pitch + kAirbornePitchTilt, yaw, -sideSign_ * kAirborneRollTilt });
            }
        } else if (aiState_ == AiState::Cooldown) {
            // 【着地・外側離脱フェーズ】地上へ戻りながら外側へ大きく開いて離脱し、滑らかに並走位置へ復帰
            Vector3 targetPos = { playerPos.x + sideSign_ * kRetreatSideDistanceX, kEnemyGroundYOffset, playerPos.z + kParallelLeadZ };
            pos.x += (targetPos.x - pos.x) * kRetreatLerpSpeed;
            pos.y += (targetPos.y - pos.y) * kRetreatLerpSpeed; // 地上へ復帰
            pos.z += (targetPos.z - pos.z) * kRetreatLerpSpeed;
        }

        // 3. 移動速度の厳格キャップ（瞬間移動・ワープの完全防止）
        Vector3 moveDelta = { pos.x - prevPos.x, pos.y - prevPos.y, pos.z - prevPos.z };
        float moveDist = std::sqrt(moveDelta.x * moveDelta.x + moveDelta.y * moveDelta.y + moveDelta.z * moveDelta.z);
        if (moveDist > kMaxEnemyFrameSpeed && moveDist > kEpsilon) {
            float factor = kMaxEnemyFrameSpeed / moveDist;
            pos.x = prevPos.x + moveDelta.x * factor;
            pos.y = prevPos.y + moveDelta.y * factor;
            pos.z = prevPos.z + moveDelta.z * factor;
        }

        // 4. 敵同士の重なり防止（Boids Separation / Soft Pushout）
        ApplyEnemySeparation(pos);

        // 5. 壁からの安全マージン押し戻し（万が一の壁めり込み完全防止）
        Vector3 pushout = MapRendererScript::GetBuildingPushout(pos, kEnemyBodyRadius);
        pos.x += pushout.x;
        pos.z += pushout.z;
    } else if (enemyType_ == EnemyType::FlyBy) {
        // 【突撃・すれ違い型】一定速度ですれ違い直進
        if (!flyByInitialized_) {
            flyByDir_ = { 0.0f, 0.0f, -kFlyBySpeed };
            flyByInitialized_ = true;
        }
        pos.x += flyByDir_.x;
        pos.y += flyByDir_.y;
        pos.z += flyByDir_.z;
    } else if (enemyType_ == EnemyType::RushSurround) {
        // 【拘束ラッシュ360度包囲型 (RushSurround)】
        // 湧いた方位（前方・側方・背後）を保ったまま自機周囲を緩やかに旋回包囲し、その場の上空へ跳躍して攻撃！
        Vector3 prevPos = pos;

        // 待機・チャージ中は包囲円上をゆっくりストレイフ旋回（壁にぶつかりそうなら旋回方向を反転）
        if (aiState_ == AiState::Aiming || aiState_ == AiState::AttackCharge || aiState_ == AiState::Cooldown) {
            float nextAngle = surroundAngle_ + kRushSurroundStrafeSpeed * surroundStrafeDir_;
            Vector3 probePos = {
                playerPos.x + std::sin(nextAngle) * kRushSurroundIdealDist,
                kEnemyGroundYOffset,
                playerPos.z + std::cos(nextAngle) * kRushSurroundIdealDist
            };
            if (IsNearBuilding(probePos, kWallCheckMargin)) {
                surroundStrafeDir_ = -surroundStrafeDir_;
            } else {
                surroundAngle_ = nextAngle;
            }
        }

        float targetY = kEnemyGroundYOffset;
        float lerpSpeed = kRushSurroundLerpSpeed;
        if (aiState_ == AiState::PounceStrike) {
            targetY = kEnemyGroundYOffset + kRushSurroundAirborneHeight;
            lerpSpeed = kAirborneLeapSpeed;
        }

        Vector3 targetPos = {
            playerPos.x + std::sin(surroundAngle_) * kRushSurroundIdealDist,
            targetY,
            playerPos.z + std::cos(surroundAngle_) * kRushSurroundIdealDist
        };

        pos.x += (targetPos.x - pos.x) * lerpSpeed;
        pos.y += (targetPos.y - pos.y) * lerpSpeed;
        pos.z += (targetPos.z - pos.z) * lerpSpeed;

        if (aiState_ == AiState::PounceStrike) {
            float hoverY = std::sin(markerPulseTimer_ * kAirborneHoverFrequency) * kAirborneHoverAmplitude;
            pos.y += hoverY;
            owner_->SetRotate({ pitch + kAirbornePitchTilt, yaw, 0.0f });
        }

        // 移動速度キャップ（ワープ防止）
        Vector3 moveDelta = { pos.x - prevPos.x, pos.y - prevPos.y, pos.z - prevPos.z };
        float moveDist = std::sqrt(moveDelta.x * moveDelta.x + moveDelta.y * moveDelta.y + moveDelta.z * moveDelta.z);
        if (moveDist > kMaxEnemyFrameSpeed && moveDist > kEpsilon) {
            float factor = kMaxEnemyFrameSpeed / moveDist;
            pos.x = prevPos.x + moveDelta.x * factor;
            pos.y = prevPos.y + moveDelta.y * factor;
            pos.z = prevPos.z + moveDelta.z * factor;
        }

        // 敵同士の重なり防止＆壁からの押し戻し
        ApplyEnemySeparation(pos);
        Vector3 pushout = MapRendererScript::GetBuildingPushout(pos, kEnemyBodyRadius);
        pos.x += pushout.x;
        pos.z += pushout.z;
    } else {
        // 【定点・待ち伏せ型 (Ambush)】
        // トラップ円の周囲や物陰に陣取って迎撃。プレイヤーのルートやビルとの衝突・けいれんは物理的にゼロ！
    }

    // 4. ステートマシン（Aiming → AttackCharge → PounceStrike → UnavoidableAttack → Cooldown）
    switch (aiState_) {
    case AiState::Aiming: {
        // RushSurround型は出現直後のわずかな猶予後に速やかにチャージ開始
        if (enemyType_ == EnemyType::RushSurround) {
            rushAimDelayTimer_++;
            if (rushAimDelayTimer_ >= kRushSurroundAimDelayFrames && distToPlayer <= kAmbushPerceptionRange && HasLineOfSightToPlayer()) {
                aiState_ = AiState::AttackCharge;
                attackGauge_ = 0.0f;
            }
        }
        // Ambush / FlyBy 型用の感知移行（ParallelChaser型は並走タイマー制御）
        else if (enemyType_ != EnemyType::ParallelChaser) {
            if (distToPlayer <= kAmbushPerceptionRange && HasLineOfSightToPlayer()) {
                aiState_ = AiState::AttackCharge;
                attackGauge_ = 0.0f;
            }
        }
        break;
    }
    case AiState::AttackCharge: {
        // 射線遮蔽チェック（途中で壁に遮られた場合はチャージを中断・Aimingへ復帰）
        if (!HasLineOfSightToPlayer()) {
            attackGauge_ = (std::max)(0.0f, attackGauge_ - kAttackGaugeSpeed * 0.5f);
            if (attackGauge_ <= 0.0f) {
                aiState_ = AiState::Aiming;
            }
            break;
        }

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

        // チャージ完了！溜めた位置からダイレクトに空中へ跳躍し、空中で隙を晒す！
        if (attackGauge_ >= 1.0f) {
            aiState_ = AiState::PounceStrike;
            strikeVulnerableTimer_ = kAirborneVulnerableDuration;
        }
        break;
    }
    case AiState::PounceStrike: {
        // 空中滞空・隙の露出（プレイヤーが意識して狙い撃てる決定的なカウンターウィンドウ）
        // 1. 敵モデルのダイナミック拡大＆脈動強調（1.35倍〜1.50倍）
        if (owner_) {
            float pulse = 0.5f + 0.5f * std::sin(markerPulseTimer_ * kAirborneScalePulseFrequency);
            float scaleMultiplier = kAirborneBaseScaleMultiplier + pulse * kAirbornePulseScaleDelta;
            owner_->SetScale({
                originalScale_.x * scaleMultiplier,
                originalScale_.y * scaleMultiplier,
                originalScale_.z * scaleMultiplier
            });
        }

        // 2. 攻撃臨界の鮮烈な赤色高速点滅
        if (owner_ && hitFlashTimer_ == 0) {
            if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
                bool blink = (strikeVulnerableTimer_ % 4) < 2;
                mr->SetColor(blink ? kMarkerWhiteFlash : kMarkerRedColor);
            }
        }

        if (strikeVulnerableTimer_ > 0) {
            strikeVulnerableTimer_--;
            if (strikeVulnerableTimer_ == 0) {
                // 隙の時間が切れたら、目前空中から弾丸発射！
                aiState_ = AiState::UnavoidableAttack;
            }
        }
        break;
    }
    case AiState::UnavoidableAttack: {
        // モデルスケールを基本サイズへ復帰
        if (owner_) {
            owner_->SetScale(originalScale_);
        }

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
        // 離脱中もスケールは確実に等倍維持
        if (owner_) {
            owner_->SetScale(originalScale_);
        }

        if (cooldownTimer_ > 0) {
            cooldownTimer_--;
            if (cooldownTimer_ == 0) {
                aiState_ = AiState::Aiming;
                attackGauge_ = 0.0f;
            }
        }
        break;
    }
    case AiState::Stun: {
        // スタン時はスケールを即座に等倍へ復帰
        if (owner_) {
            owner_->SetScale(originalScale_);
        }

        // のけぞり姿勢（ピッチ角を後ろに倒す）
        Vector3 rot = owner_->GetRotate();
        rot.x = -0.35f; // 仰け反り
        owner_->SetRotate(rot);

        if (stunTimer_ > 0) {
            stunTimer_--;
            if (stunTimer_ == 0) {
                aiState_ = AiState::Aiming;
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
    case AiState::Ghost: {
        // 撃破後の幽霊状態（Step 1-4 で実装）
        break;
    }
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
    if (isDead_ || IsGhost()) return;

    isVaulting_ = false; // 飛び越え中に被弾した場合も即座に解除

    int finalDamage = isCritical ? amount * kCriticalMultiplier : amount;
    hp_ -= finalDamage;
    hitFlashTimer_ = kHitFlashDuration;

    if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
        mr->SetColor(isCritical ? kCriticalFlashColor : kFlashColor);
    }

    // 1. 被弾ノックバック演出（自機から敵へのベクトル方向へ衝撃押し戻し）
    if (owner_ && targetObject_) {
        Vector3 ePos = owner_->GetPosition();
        Vector3 pPos = targetObject_->GetPosition();
        Vector3 pushDir = { ePos.x - pPos.x, 0.0f, ePos.z - pPos.z };
        float pLenSq = pushDir.x * pushDir.x + pushDir.z * pushDir.z;
        if (pLenSq > kEpsilon) {
            float invLen = 1.0f / std::sqrt(pLenSq);
            ePos.x += pushDir.x * invLen * kHitKnockbackDist;
            ePos.z += pushDir.z * invLen * kHitKnockbackDist;
            owner_->SetPosition(ePos);
        }
    }

    // 2. 被弾縮みバウンド変形（Squash: 弾丸がめり込んだ重厚な手応え）
    hitSquashTimer_ = kDefaultHitSquashDuration;
    if (owner_) {
        owner_->SetScale({
            originalScale_.x * kHitSquashScale.x,
            originalScale_.y * kHitSquashScale.y,
            originalScale_.z * kHitSquashScale.z
        });
    }

    // 攻撃阻止（カウンター / アタックキャンセル）判定
    // 攻撃チャージ中（AttackCharge）または空中切り込み隙（PounceStrike）に被弾した場合
    if (aiState_ == AiState::AttackCharge || aiState_ == AiState::PounceStrike) {
        // チャージ・切り込みを強制中断
        attackGauge_ = 0.0f;
        strikeVulnerableTimer_ = 0;
        aiState_ = AiState::Stun;
        stunTimer_ = kStunDuration;

        // モデルスケールを即座に基本サイズへ復帰
        if (owner_) {
            owner_->SetScale(originalScale_);
        }

        // カウンター阻止成功のカメラシェイクとコンパクトな阻止エフェクト（周囲の敵を隠さないサイズ）
        constexpr float kCancelShakeIntensity = 0.12f;
        constexpr float kCancelShakeDuration = 0.18f;
        constexpr float kCancelImpactScale = 0.65f;
        ShakeCamera(kCancelShakeIntensity, kCancelShakeDuration);
        if (owner_) {
            EffectPlayParam cancelParam{};
            cancelParam.position = owner_->GetPosition();
            cancelParam.scale = { kCancelImpactScale, kCancelImpactScale, kCancelImpactScale };
            cancelParam.isLoop = false;
            if (auto* effectMgr = EffectManager::GetInstance()) {
                effectMgr->PlayEffect3D(kEffectHitImpact, cancelParam);
            }
        }
    }

    if (hp_ <= 0) {
        hp_ = 0;
        if (isBoss_) {
            // ボス敵は撃破と同時に消滅
            isDead_ = true;
            if (owner_) {
                owner_->SetScale(originalScale_);
                owner_->SetVisible(false);
            }
        } else {
            // 通常敵・ラッシュ敵は撃破後に幽霊化（AiState::Ghost）してカメラ前方へ取り憑く！
            EnterGhostState();
        }
    }
}

/// <summary>
/// 被弾時点滅タイマーをカウントダウンし、満了時にメッシュの色を元のマテリアルカラーへ戻します。
/// </summary>
void EnemyAIScript::HandleHitFlash() {
    if (IsGhost()) return;

    if (hitFlashTimer_ > 0) {
        hitFlashTimer_--;
        if (hitFlashTimer_ == 0 && owner_) {
            if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
                mr->SetColor(originalColor_);
            }
        }
    }

    // 被弾縮みバウンドの復元（満了時に元の等倍スケールへスムーズに復帰）
    if (hitSquashTimer_ > 0) {
        hitSquashTimer_--;
        if (hitSquashTimer_ == 0 && owner_ && aiState_ != AiState::PounceStrike) {
            owner_->SetScale(originalScale_);
        }
    }
}

/// <summary>
/// 敵撃破時に呼び出され、半透明ゴースト状態（AiState::Ghost）へ遷移させます。
/// </summary>
void EnemyAIScript::EnterGhostState() {
    aiState_ = AiState::Ghost;
    attackGauge_ = 0.0f;
    strikeVulnerableTimer_ = 0;
    hitFlashTimer_ = 0;
    hitSquashTimer_ = 0;
    isVaulting_ = false;

    ghostLifeTimer_ = kGhostLifeDuration;
    ghostAnimTimer_ = 0.0f;
    ghostSlotIndex_ = (sNextGhostSlot_++) % 5;
    constexpr float kSlotPhaseStep = 1.256637f; // 2*PI / 5
    ghostPhaseOffset_ = static_cast<float>(ghostSlotIndex_) * kSlotPhaseStep;
    vacuumProgress_ = 0.0f;
    isBeingVacuumed_ = false;
    vacuumActiveFrames_ = 0;

    if (owner_) {
        // 幽霊化瞬間の魂離脱スパーク
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(owner_->GetPosition(), kGhostSpawnSparkCount);
        }

        // 半透明シアンパープル発光マテリアル（CubeObject::Draw が自動で Object3D_Alpha へ切替）
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            mr->SetLightingMode(0);
            mr->SetColor(kGhostColorCyan);
        }

        float ghostScale = kEnemyBaseScale * kGhostBaseScaleRatio;
        owner_->SetScale({ ghostScale, ghostScale, ghostScale });
    }
}

bool EnemyAIScript::ApplyVacuumPull(const Vector3& muzzlePos, float chargeAmount) {
    if (!IsGhost() || !owner_) return false;

    constexpr int kActiveHoldFrames = 2;
    vacuumActiveFrames_ = kActiveHoldFrames;
    isBeingVacuumed_ = true;
    vacuumTargetMuzzlePos_ = muzzlePos;
    vacuumProgress_ = std::clamp(vacuumProgress_ + chargeAmount, 0.0f, 1.0f);

    // 吸引中は一定フレーム間隔で霊魂から収束スパークを発生
    if (static_cast<int>(ghostAnimTimer_) % kGhostVacuumSparkInterval == 0) {
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(owner_->GetPosition(), kGhostVacuumStreamSparkCount);
        }
    }

    // 吸引ゲージ満タンで銃口へ吸い込まれ成仏・完全消滅
    if (vacuumProgress_ >= 1.0f) {
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(muzzlePos, kGhostExorciseBurstCount);
        }
        isDead_ = true;
        owner_->SetVisible(false);
        return true;
    }

    return false;
}

/// <summary>
/// 幽霊状態（AiState::Ghost）中のカメラ前方8の字浮遊追従、右クリック吸引時の銃口スパイラル引き寄せ、および霊魂ゆらぎアニメーションを更新します。
/// </summary>
void EnemyAIScript::UpdateGhostState() {
    if (!owner_) return;

    // 吸引中は自然消滅タイマーの進行を止め、吸引解除時はゲージを緩やかに減衰
    if (vacuumActiveFrames_ > 0) {
        vacuumActiveFrames_--;
        isBeingVacuumed_ = true;
    } else {
        isBeingVacuumed_ = false;
        if (vacuumProgress_ > 0.0f) {
            vacuumProgress_ = (std::max)(0.0f, vacuumProgress_ - kGhostVacuumDecaySpeed);
        }
        ghostLifeTimer_--;
        if (ghostLifeTimer_ <= 0) {
            // 自然成仏して消滅
            isDead_ = true;
            owner_->SetVisible(false);
            return;
        }
    }

    ghostAnimTimer_ += 1.0f;

    auto* camMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = camMgr ? camMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    Vector3 camPos = camera->GetPosition();
    Vector3 camRot = camera->GetRotation();
    Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
    Vector3 camRight   = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 camUp      = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
    Vector3 camForward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };

    // 8の字リサージュ曲線でカメラ前方の視界をゆらゆら横切るターゲット座標
    float swayX = std::sin(ghostAnimTimer_ * kGhostSwayFreqX + ghostPhaseOffset_) * kGhostSwayAmpX;
    float swayY = std::sin(ghostAnimTimer_ * kGhostSwayFreqY + ghostPhaseOffset_) * kGhostSwayAmpY;
    float forwardDist = kGhostHauntDist + static_cast<float>(ghostSlotIndex_ % 3) * kGhostHauntDistStep;

    Vector3 hauntPos = {
        camPos.x + camForward.x * forwardDist + camRight.x * swayX + camUp.x * swayY,
        camPos.y + camForward.y * forwardDist + camRight.y * swayX + camUp.y * swayY,
        camPos.z + camForward.z * forwardDist + camRight.z * swayX + camUp.z * swayY
    };

    Vector3 pos = owner_->GetPosition();
    if (isBeingVacuumed_ || vacuumProgress_ > 0.01f) {
        // 吸引ゲージに応じて本来の浮遊位置から銃口（vacuumTargetMuzzlePos_）へ渦巻き状に引き寄せる
        float swirlAngle = ghostAnimTimer_ * kGhostVacuumSwirlSpeed + ghostPhaseOffset_;
        float swirlR = kGhostVacuumSwirlRadius * (1.0f - vacuumProgress_);
        Vector3 pullTarget = {
            vacuumTargetMuzzlePos_.x + (camRight.x * std::cos(swirlAngle) + camUp.x * std::sin(swirlAngle)) * swirlR,
            vacuumTargetMuzzlePos_.y + (camRight.y * std::cos(swirlAngle) + camUp.y * std::sin(swirlAngle)) * swirlR,
            vacuumTargetMuzzlePos_.z + (camRight.z * std::cos(swirlAngle) + camUp.z * std::sin(swirlAngle)) * swirlR
        };
        Vector3 targetPos = {
            hauntPos.x + (pullTarget.x - hauntPos.x) * vacuumProgress_,
            hauntPos.y + (pullTarget.y - hauntPos.y) * vacuumProgress_,
            hauntPos.z + (pullTarget.z - hauntPos.z) * vacuumProgress_
        };
        float pullLerp = kGhostVacuumPullLerpBase + (kGhostVacuumPullLerpMax - kGhostVacuumPullLerpBase) * vacuumProgress_;
        pos.x += (targetPos.x - pos.x) * pullLerp;
        pos.y += (targetPos.y - pos.y) * pullLerp;
        pos.z += (targetPos.z - pos.z) * pullLerp;
    } else {
        pos.x += (hauntPos.x - pos.x) * kGhostFollowLerpSpeed;
        pos.y += (hauntPos.y - pos.y) * kGhostFollowLerpSpeed;
        pos.z += (hauntPos.z - pos.z) * kGhostFollowLerpSpeed;
        // 他の幽霊と完全に1点へ重ならないよう緩やかに分離
        ApplyEnemySeparation(pos);
    }
    owner_->SetPosition(pos);

    // 不気味な自転＆吸引時の高速スピン・縮小変形
    Vector3 rot = owner_->GetRotate();
    float spinBoost = isBeingVacuumed_ ? (kGhostVacuumSpinBoost * (0.4f + vacuumProgress_)) : 0.0f;
    rot.y += kGhostSpinSpeed + spinBoost;
    rot.z = std::sin(ghostAnimTimer_ * kGhostSwayFreqX + ghostPhaseOffset_) * 0.35f;
    owner_->SetRotate(rot);

    float shrinkFactor = 1.0f - vacuumProgress_ * kGhostVacuumShrinkRatio;
    float baseGhostScale = kEnemyBaseScale * kGhostBaseScaleRatio * shrinkFactor;
    float stretch = std::sin(ghostAnimTimer_ * kGhostSwayFreqY + ghostPhaseOffset_) * kGhostWispStretchAmp;
    owner_->SetScale({
        baseGhostScale * (1.0f - stretch * 0.5f),
        baseGhostScale * (1.0f + stretch),
        baseGhostScale * (1.0f - stretch * 0.5f)
    });

    // シアン⇔パープル間の霊魂カラー明滅（吸引中は高輝度エレクトリックシアンホワイトへ発光）
    if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
        constexpr float kColorWaveSpeed = 0.08f;
        float blend = 0.5f + 0.5f * std::sin(ghostAnimTimer_ * kColorWaveSpeed + ghostPhaseOffset_);
        Vector4 ghostColor = {
            kGhostColorCyan.x + (kGhostColorPurple.x - kGhostColorCyan.x) * blend,
            kGhostColorCyan.y + (kGhostColorPurple.y - kGhostColorCyan.y) * blend,
            kGhostColorCyan.z + (kGhostColorPurple.z - kGhostColorCyan.z) * blend,
            kGhostColorCyan.w + (kGhostColorPurple.w - kGhostColorCyan.w) * blend
        };
        if (vacuumProgress_ > 0.01f) {
            constexpr Vector4 kVacuumGlowColor = { 0.65f, 1.0f, 1.0f, 0.78f };
            ghostColor.x += (kVacuumGlowColor.x - ghostColor.x) * vacuumProgress_;
            ghostColor.y += (kVacuumGlowColor.y - ghostColor.y) * vacuumProgress_;
            ghostColor.z += (kVacuumGlowColor.z - ghostColor.z) * vacuumProgress_;
            ghostColor.w += (kVacuumGlowColor.w - ghostColor.w) * vacuumProgress_;
        }
        if (ghostLifeTimer_ < kGhostFadeOutFrames) {
            float fadeRatio = static_cast<float>(ghostLifeTimer_) / static_cast<float>(kGhostFadeOutFrames);
            ghostColor.w *= fadeRatio;
        }
        mr->SetColor(ghostColor);
    }
}

void EnemyAIScript::EnsureGhostJammingVisuals() {
    if (!ghostJamDisc_) {
        ghostJamDisc_ = std::make_unique<Circle2DObject>();
        ghostJamDisc_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(ghostJamDisc_.get());
    }
    if (!ghostJamRing_) {
        ghostJamRing_ = std::make_unique<Ring2DObject>();
        ghostJamRing_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(ghostJamRing_.get());
    }
    for (int i = 0; i < kGhostGlitchLineCount; ++i) {
        if (!ghostGlitchLines_[i]) {
            ghostGlitchLines_[i] = std::make_unique<Line2DObject>();
            ghostGlitchLines_[i]->Initialize();
            SceneHierarchy::GetInstance()->Unregister(ghostGlitchLines_[i].get());
        }
    }
    if (!vacuumGaugeRing_) {
        vacuumGaugeRing_ = std::make_unique<Ring2DObject>();
        vacuumGaugeRing_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(vacuumGaugeRing_.get());
    }
    for (int i = 0; i < kVacuumStreamLineCount; ++i) {
        if (!vacuumStreamLines_[i]) {
            vacuumStreamLines_[i] = std::make_unique<Line2DObject>();
            vacuumStreamLines_[i]->Initialize();
            SceneHierarchy::GetInstance()->Unregister(vacuumStreamLines_[i].get());
        }
    }
}

/// <summary>
/// カメラ前方に取り憑いた幽霊による2D視界ジャミング（霊障シルエット＆グリッチノイズ走査線）を描画します。
/// </summary>
void EnemyAIScript::DrawGhostJamming2D() {
    if (!owner_ || !IsGhost()) return;

    auto* camMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = camMgr ? camMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    Vector3 ghostPos = owner_->GetPosition();
    Vector3 camPos = camera->GetPosition();
    Vector3 diff = { ghostPos.x - camPos.x, ghostPos.y - camPos.y, ghostPos.z - camPos.z };
    float distToCam = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

    // カメラ前方に取り憑いている距離（kGhostJamActivationDist 以内）のときのみ視界ジャミング発動
    if (distToCam > kGhostJamActivationDist) return;

    Matrix4x4 viewProj = Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    float w = ghostPos.x * viewProj.m[0][3] + ghostPos.y * viewProj.m[1][3] + ghostPos.z * viewProj.m[2][3] + viewProj.m[3][3];
    if (w <= kNearClipW) return;

    float invW = 1.0f / w;
    float ndcX = (ghostPos.x * viewProj.m[0][0] + ghostPos.y * viewProj.m[1][0] + ghostPos.z * viewProj.m[2][0] + viewProj.m[3][0]) * invW;
    float ndcY = (ghostPos.x * viewProj.m[0][1] + ghostPos.y * viewProj.m[1][1] + ghostPos.z * viewProj.m[2][1] + viewProj.m[3][1]) * invW;

    float screenX = (ndcX * kHalf + kHalf) * kScreenWidth;
    float screenY = (-ndcY * kHalf + kHalf) * kScreenHeight;

    if (screenX < -kScreenCullMargin || screenX > kScreenWidth + kScreenCullMargin ||
        screenY < -kScreenCullMargin || screenY > kScreenHeight + kScreenCullMargin) {
        return;
    }

    EnsureGhostJammingVisuals();

    // 距離と寿命に応じたジャミング強度 (0.0 〜 1.0、吸引進行に伴いジャミング濃度を軽減)
    float proximityFactor = std::clamp(1.0f - (distToCam - kGhostHauntDist) / (kGhostJamActivationDist - kGhostHauntDist), 0.25f, 1.0f);
    float fadeFactor = (ghostLifeTimer_ < kGhostFadeOutFrames)
        ? (static_cast<float>(ghostLifeTimer_) / static_cast<float>(kGhostFadeOutFrames))
        : 1.0f;
    float vacuumClearRatio = 1.0f - vacuumProgress_ * 0.72f;
    float intensity = proximityFactor * fadeFactor * vacuumClearRatio;

    constexpr float kPulseFreq = 0.14f;
    constexpr float kPulseRadiusAmp = 8.0f;
    constexpr float kDiscBaseAlpha = 0.34f;
    constexpr float kRingBaseAlpha = 0.58f;
    constexpr float kGlitchAlpha = 0.68f;

    float pulse = std::sin(ghostAnimTimer_ * kPulseFreq + ghostPhaseOffset_);
    float jamRadius = (kGhostJamDiscRadius + pulse * kPulseRadiusAmp) * (1.0f - vacuumProgress_ * 0.55f);

    // 1. 照準・視界をボヤッと遮る霊障ダークパープルシルエット円盤
    if (ghostJamDisc_) {
        ghostJamDisc_->SetRadius(jamRadius);
        ghostJamDisc_->GetMaterialData()->color = { 0.14f, 0.04f, 0.24f, kDiscBaseAlpha * intensity };
        ghostJamDisc_->SetPosition({ screenX - jamRadius, screenY - jamRadius, 0.0f });
        ghostJamDisc_->Update();
        ghostJamDisc_->Draw("white", true);
    }

    // 2. 不気味に脈動する霊障オーラ外周リング
    if (ghostJamRing_) {
        ghostJamRing_->SetRadii(jamRadius, (std::max)(1.0f, jamRadius - kGhostJamRingThickness));
        ghostJamRing_->GetMaterialData()->color = { 0.45f, 0.90f, 1.0f, kRingBaseAlpha * intensity };
        ghostJamRing_->SetPosition({ screenX - jamRadius, screenY - jamRadius, 0.0f });
        ghostJamRing_->Update();
        ghostJamRing_->Draw("white", true);
    }

    // 3. 照準周辺をチカチカと走る霊障グリッチノイズ走査線
    int glitchTick = static_cast<int>(ghostAnimTimer_ * 0.5f) + ghostSlotIndex_ * 7;
    for (int i = 0; i < kGhostGlitchLineCount; ++i) {
        if (!ghostGlitchLines_[i]) continue;

        // 擬似ハッシュでフレームごとに不規則な水平グリッチ線を生成
        int seed = (glitchTick * 31 + i * 17) & 0xFF;
        float normA = static_cast<float>(seed) / 255.0f;
        float normB = static_cast<float>((seed * 73) & 0xFF) / 255.0f;

        float offsetY = (normA - 0.5f) * 2.0f * kGhostGlitchSpreadY;
        float halfLen = (0.35f + 0.65f * normB) * kGhostGlitchHalfWidth;
        float centerShiftX = (normB - 0.5f) * 32.0f;

        Vector2 p0 = { screenX + centerShiftX - halfLen, screenY + offsetY };
        Vector2 p1 = { screenX + centerShiftX + halfLen, screenY + offsetY };

        bool isCyanLine = ((seed + i) % 2) == 0;
        Vector4 lineColor = isCyanLine
            ? Vector4{ 0.35f, 0.98f, 1.0f, kGlitchAlpha * intensity }
            : Vector4{ 0.88f, 0.32f, 1.0f, kGlitchAlpha * intensity };

        ghostGlitchLines_[i]->SetPoints(p0, p1);
        ghostGlitchLines_[i]->SetThickness(kGhostGlitchThickness);
        ghostGlitchLines_[i]->GetMaterialData()->color = lineColor;
        ghostGlitchLines_[i]->Update();
        ghostGlitchLines_[i]->Draw("white", true);
    }

    // 4. Step 1-5: 右クリック吸引中の2D収束ゲージリング＆銃口への霊魂ストリームライン描画
    if (vacuumProgress_ > 0.01f) {
        // 幽霊の中心へ向かってキュッと収縮する高輝度シアン吸引ゲージリング
        if (vacuumGaugeRing_) {
            float gaugeR = kVacuumGaugeRingStartRadius + (kVacuumGaugeRingEndRadius - kVacuumGaugeRingStartRadius) * vacuumProgress_;
            float gaugeInnerR = (std::max)(1.0f, gaugeR - kVacuumGaugeRingThickness);
            vacuumGaugeRing_->SetRadii(gaugeR, gaugeInnerR);
            vacuumGaugeRing_->GetMaterialData()->color = { 0.40f, 1.0f, 1.0f, 0.92f };
            vacuumGaugeRing_->SetPosition({ screenX - gaugeR, screenY - gaugeR, 0.0f });
            vacuumGaugeRing_->Update();
            vacuumGaugeRing_->Draw("white", true);
        }

        // 銃口ワールド座標をスクリーンへ投影（画面外やニアクリップ時は画面右下銃口基準位置へフォールバック）
        constexpr float kDefaultMuzzleScreenX = 880.0f;
        constexpr float kDefaultMuzzleScreenY = 580.0f;
        float muzzleScreenX = kDefaultMuzzleScreenX;
        float muzzleScreenY = kDefaultMuzzleScreenY;

        float mw = vacuumTargetMuzzlePos_.x * viewProj.m[0][3] + vacuumTargetMuzzlePos_.y * viewProj.m[1][3] +
                   vacuumTargetMuzzlePos_.z * viewProj.m[2][3] + viewProj.m[3][3];
        if (mw > kNearClipW) {
            float invMw = 1.0f / mw;
            float mNdcX = (vacuumTargetMuzzlePos_.x * viewProj.m[0][0] + vacuumTargetMuzzlePos_.y * viewProj.m[1][0] +
                           vacuumTargetMuzzlePos_.z * viewProj.m[2][0] + viewProj.m[3][0]) * invMw;
            float mNdcY = (vacuumTargetMuzzlePos_.x * viewProj.m[0][1] + vacuumTargetMuzzlePos_.y * viewProj.m[1][1] +
                           vacuumTargetMuzzlePos_.z * viewProj.m[2][1] + viewProj.m[3][1]) * invMw;
            muzzleScreenX = (mNdcX * kHalf + kHalf) * kScreenWidth;
            muzzleScreenY = (-mNdcY * kHalf + kHalf) * kScreenHeight;
        }

        constexpr float kStreamSpreadRadius = 26.0f;
        constexpr float kStreamAngleStep = 2.0943951f; // 2*PI / 3
        for (int i = 0; i < kVacuumStreamLineCount; ++i) {
            if (!vacuumStreamLines_[i]) continue;

            float angle = ghostAnimTimer_ * kGhostVacuumSwirlSpeed + static_cast<float>(i) * kStreamAngleStep;
            float spread = kStreamSpreadRadius * (1.0f - vacuumProgress_ * 0.7f);
            Vector2 startPt = {
                screenX + std::cos(angle) * spread,
                screenY + std::sin(angle) * spread
            };
            Vector2 endPt = { muzzleScreenX, muzzleScreenY };

            float streamAlpha = (0.45f + 0.50f * vacuumProgress_);
            Vector4 streamColor = (i % 2 == 0)
                ? Vector4{ 0.35f, 0.98f, 1.0f, streamAlpha }
                : Vector4{ 0.85f, 0.96f, 1.0f, streamAlpha };

            vacuumStreamLines_[i]->SetPoints(startPt, endPt);
            vacuumStreamLines_[i]->SetThickness(kVacuumStreamThickness);
            vacuumStreamLines_[i]->GetMaterialData()->color = streamColor;
            vacuumStreamLines_[i]->Update();
            vacuumStreamLines_[i]->Draw("white", true);
        }
    }
}

void EnemyAIScript::SetBoss(bool isBoss) {
    isBoss_ = isBoss;
    maxHp_ = isBoss ? kDefaultBossHp : kDefaultNormalHp;
    hp_ = maxHp_;
}

void EnemyAIScript::SetCustomMaxHp(int hp) {
    constexpr int kMinHp = 1;
    maxHp_ = (std::max)(kMinHp, hp);
    hp_ = maxHp_;
}

void EnemyAIScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.3f, 0.3f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【敵AIスクリプト設定】");
    ImGui::Separator();

    const char* typeNames[] = {
        "横並走＆前方強襲 (ParallelChaser)",
        "定点待ち伏せ (Ambush)",
        "突撃すれ違い (FlyBy)",
        "拘束ラッシュ包囲 (RushSurround)"
    };
    int currentTypeIndex = static_cast<int>(enemyType_);
    if (ImGui::Combo("行動タイプ##EnemyType", &currentTypeIndex, typeNames, 4)) {
        enemyType_ = static_cast<EnemyType>(currentTypeIndex);
    }

    ImGui::Checkbox("ボス属性##IsBoss", &isBoss_);
    ImGui::DragInt("最大HP##MaxHP", &maxHp_, 1, 1, 500);
    ImGui::ProgressBar(static_cast<float>(hp_) / static_cast<float>(maxHp_), ImVec2(0.0f, 0.0f),
        std::to_string(hp_).append(" / ").append(std::to_string(maxHp_)).c_str());

    const char* stateNames[] = { "Aiming", "AttackCharge", "PounceStrike", "UnavoidableAttack", "Cooldown", "Stun", "Ghost" };
    int currentStateIndex = static_cast<int>(aiState_);
    ImGui::Text("AIステート: %s", (currentStateIndex >= 0 && currentStateIndex < 7) ? stateNames[currentStateIndex] : "Unknown");

    const char* zoneNames[] = { "Zone 1 (斜め前方)", "Zone 2 (正面上空)", "Zone 3 (側方ハイジャンプ)" };
    ImGui::Combo("跳躍ゾーン##AttackZone", &attackZoneIndex_, zoneNames, 3);
    ImGui::ProgressBar(attackGauge_, ImVec2(0.0f, 0.0f), "Attack Gauge");
    if (aiState_ == AiState::Cooldown) {
        float cdProgress = static_cast<float>(cooldownTimer_) / static_cast<float>(cooldownDuration_);
        ImGui::ProgressBar(cdProgress, ImVec2(0.0f, 0.0f), ("Cooldown: " + std::to_string(cooldownTimer_)).c_str());
    }

    ImGui::Separator();
    ImGui::DragInt("クールタイム(F)##CooldownDuration", &cooldownDuration_, 5, 10, 300);
    ImGui::DragInt("敵弾攻撃力##EnemyDamage", &bulletDamage_, 1, 1, 10);
#endif
}
