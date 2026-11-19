#include "Scripts/Enemy/EnemySpawnPointScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/PrefabManager.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Engine/Graphics/Objects/3d/FlatRing/FlatRingObject.h"
#include "Engine/Graphics/Objects/3d/Cylinder/CylinderObject.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include <cmath>
#include <numbers>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(EnemySpawnPointScript)

namespace {
    // ゾーンビジュアル用定数 (マジックナンバー排除)
    constexpr uint32_t kRingDivideCount = 32;
    constexpr uint32_t kCylinderSubdivisions = 32;
    constexpr float kDiscInnerRadiusRatio = 0.0f;        // 地面塗りつぶし円盤の内径比率 (面)
    constexpr float kRingInnerRadiusRatio = 0.88f;       // 地面外周リングの内径比率
    constexpr float kRingZBias = 0.005f;                 // ディスクとリングのZ-Fighting防止バイアス
    constexpr float kRotationSpeed = 0.015f;             // シリンダー自転速度
    constexpr float kSingleEnemyVisualRadius = 3.2f;     // 単体出現時の可視化半径
    constexpr float kVisualRadiusPadding = 1.2f;         // 複数出現時の余白半径
    constexpr float kUnitBaseRadius = 1.0f;              // 初期生成時の基準単位半径 (m)
    constexpr float kUnitBaseHeight = 1.0f;              // 初期生成時の基準単位高さ (m)
    constexpr float kTextElevationOffset = 0.8f;         // シリンダー上空の文字表示オフセット (m)
    constexpr float kDefaultTextFontSize = 20.0f;        // 敵情報表示文字サイズ
    constexpr float kScreenWidth = 1280.0f;              // 基準スクリーン幅
    constexpr float kScreenHeight = 720.0f;              // 基準スクリーン高さ
    constexpr float kNearClipW = 0.001f;                 // クリップ判定閾値

    // 敵エリア「外向き拡散＆湧き上がり」アニメーション定数（拘束トラップの内向き吸引と真逆の直感表現）
    const std::string kAlphaPsoKey = "Object3D_Alpha";
    const std::string kWhiteTexKey = "white";
    constexpr float kWaveInnerRadiusRatio = 0.88f;       // 外向き拡散ソナーリングの内径比率
    constexpr float kRisingInnerRadiusRatio = 0.90f;     // 湧き上がり浮上リングの内径比率
    constexpr float kWaveZBias = 0.015f;                 // 拡散波紋のZ-Fighting防止バイアス
    constexpr float kEnemyWaveBaseSpeed = 0.014f;        // 外向き拡散ソナーの基本速度 (/frame)
    constexpr float kEnemyWaveCountSpeedBoost = 0.003f;  // 敵数に応じた波紋テンポ加算係数
    constexpr float kEnemyBossWaveSpeedBonus = 0.008f;   // ボス時の波紋テンポ加算
    constexpr float kEnemyWaveStartScaleRatio = 0.15f;   // 外向き拡散リングの開始スケール比率
    constexpr float kEnemyRisingBaseHeight = 2.0f;       // 湧き上がり浮上リングの基本到達高度 (m)
    constexpr float kEnemyRisingCountHeightStep = 0.45f; // 敵数1体ごとの上昇高度加算 (m)
    constexpr float kEnemyBossRisingHeight = 4.5f;       // ボス時の湧き上がり到達高度 (m)
    constexpr float kEnemyRisingStartScale = 0.72f;      // 浮上開始時のリングスケール比率
    constexpr float kEnemyRisingEndScale = 1.05f;        // 浮上到達時のリングスケール比率
    constexpr float kWaveAlphaPeak = 0.78f;              // 波紋の最大アルファ値
    constexpr float kRisingAlphaPeak = 0.72f;            // 浮上リングの最大アルファ値
    constexpr float kHalfCycleOffset = 0.5f;             // 拡散と浮上の位相差 (0.5周期ずらしで途切れない動き)
    constexpr float kCylinderSoftAlphaRatio = 0.45f;     // 警戒シリンダーの控えめな半透明係数
    constexpr float kWaveCycleMax = 1.0f;

    // カラーパレット (敵種類別: エディタ時 / ゲーム中視認性向上半透明)
    // 1. 通常敵 (Enemy): 鮮烈オレンジ
    constexpr Vector4 kNormalRingEditorColor = { 1.0f, 0.45f, 0.1f, 0.85f };
    constexpr Vector4 kNormalCylinderEditorColor = { 1.0f, 0.45f, 0.1f, 0.28f };
    constexpr Vector4 kNormalDiscEditorColor = { 1.0f, 0.45f, 0.1f, 0.45f };
    constexpr Vector4 kNormalRingPlayColor = { 1.0f, 0.45f, 0.1f, 0.85f };
    constexpr Vector4 kNormalDiscPlayColor = { 1.0f, 0.45f, 0.1f, 0.35f };
    constexpr Vector4 kNormalTextColor = { 1.0f, 0.90f, 0.4f, 1.0f };

    // 2. 俊足・突撃敵 (FastEnemy / Rush): エレクトリックシアン
    constexpr Vector4 kFastRingEditorColor = { 0.15f, 0.75f, 1.0f, 0.85f };
    constexpr Vector4 kFastCylinderEditorColor = { 0.15f, 0.75f, 1.0f, 0.28f };
    constexpr Vector4 kFastDiscEditorColor = { 0.15f, 0.75f, 1.0f, 0.45f };
    constexpr Vector4 kFastRingPlayColor = { 0.15f, 0.75f, 1.0f, 0.85f };
    constexpr Vector4 kFastDiscPlayColor = { 0.15f, 0.75f, 1.0f, 0.35f };
    constexpr Vector4 kFastTextColor = { 0.4f, 0.9f, 1.0f, 1.0f };

    // 3. 遠距離・射撃敵 (Shooter / Range): エメラルドグリーン
    constexpr Vector4 kShooterRingEditorColor = { 0.15f, 0.95f, 0.45f, 0.85f };
    constexpr Vector4 kShooterCylinderEditorColor = { 0.15f, 0.95f, 0.45f, 0.28f };
    constexpr Vector4 kShooterDiscEditorColor = { 0.15f, 0.95f, 0.45f, 0.45f };
    constexpr Vector4 kShooterRingPlayColor = { 0.15f, 0.95f, 0.45f, 0.85f };
    constexpr Vector4 kShooterDiscPlayColor = { 0.15f, 0.95f, 0.45f, 0.35f };
    constexpr Vector4 kShooterTextColor = { 0.4f, 1.0f, 0.7f, 1.0f };

    // 4. 大型・重装敵 (HeavyEnemy / Tank): クリムゾンレッド
    constexpr Vector4 kHeavyRingEditorColor = { 0.95f, 0.15f, 0.20f, 0.85f };
    constexpr Vector4 kHeavyCylinderEditorColor = { 0.95f, 0.15f, 0.20f, 0.28f };
    constexpr Vector4 kHeavyDiscEditorColor = { 0.95f, 0.15f, 0.20f, 0.45f };
    constexpr Vector4 kHeavyRingPlayColor = { 0.95f, 0.15f, 0.20f, 0.85f };
    constexpr Vector4 kHeavyDiscPlayColor = { 0.95f, 0.15f, 0.20f, 0.35f };
    constexpr Vector4 kHeavyTextColor = { 1.0f, 0.45f, 0.45f, 1.0f };

    // 5. ボス敵 (BossEnemy): ネオンパープル / マゼンタ
    constexpr Vector4 kBossRingEditorColor = { 0.88f, 0.10f, 0.95f, 0.90f };
    constexpr Vector4 kBossCylinderEditorColor = { 0.82f, 0.05f, 0.90f, 0.35f };
    constexpr Vector4 kBossDiscEditorColor = { 0.88f, 0.10f, 0.95f, 0.50f };
    constexpr Vector4 kBossRingPlayColor = { 0.88f, 0.10f, 0.95f, 0.90f };
    constexpr Vector4 kBossDiscPlayColor = { 0.88f, 0.10f, 0.95f, 0.40f };
    constexpr Vector4 kBossTextColor = { 1.0f, 0.35f, 0.95f, 1.0f };
}

EnemySpawnPointScript::HazardZoneColorTheme EnemySpawnPointScript::GetVisualColorTheme() const {
    if (isBoss_ || prefabName_ == kBossEnemyPrefab) {
        return { kBossRingEditorColor, kBossCylinderEditorColor, kBossDiscEditorColor, kBossRingPlayColor, kBossDiscPlayColor, kBossTextColor };
    }
    if (prefabName_.find("Fast") != std::string::npos || prefabName_.find("Rush") != std::string::npos) {
        return { kFastRingEditorColor, kFastCylinderEditorColor, kFastDiscEditorColor, kFastRingPlayColor, kFastDiscPlayColor, kFastTextColor };
    }
    if (prefabName_.find("Shoot") != std::string::npos || prefabName_.find("Range") != std::string::npos) {
        return { kShooterRingEditorColor, kShooterCylinderEditorColor, kShooterDiscEditorColor, kShooterRingPlayColor, kShooterDiscPlayColor, kShooterTextColor };
    }
    if (prefabName_.find("Heavy") != std::string::npos || prefabName_.find("Tank") != std::string::npos) {
        return { kHeavyRingEditorColor, kHeavyCylinderEditorColor, kHeavyDiscEditorColor, kHeavyRingPlayColor, kHeavyDiscPlayColor, kHeavyTextColor };
    }
    return { kNormalRingEditorColor, kNormalCylinderEditorColor, kNormalDiscEditorColor, kNormalRingPlayColor, kNormalDiscPlayColor, kNormalTextColor };
}

std::string EnemySpawnPointScript::GetSpawnCountLevelText() const {
    if (isBoss_ || prefabName_ == kBossEnemyPrefab) {
        return "[★BOSS]";
    }

    std::string levelStr;
    if (count_ <= kFewCountThreshold) {
        levelStr = "少";
    } else if (count_ <= kMediumCountThreshold) {
        levelStr = "普";
    } else {
        levelStr = "多";
    }

    // デフォルトEnemyプレハブの場合はシンプルに "敵 [普]"、特殊敵の場合は "プレハブ名 [普]"
    if (prefabName_ == kDefaultEnemyPrefab) {
        return "敵 [" + levelStr + "]";
    }
    return prefabName_ + " [" + levelStr + "]";
}

EnemySpawnPointScript::EnemySpawnPointScript()
    : ScriptComponent("EnemySpawnPointScript") {
}

EnemySpawnPointScript::~EnemySpawnPointScript() {
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
    groundDisc_.reset();
    groundRing_.reset();
    expandWaveRing_.reset();
    risingAuraRing_.reset();
    warningCylinder_.reset();
}

void EnemySpawnPointScript::Awake() {
    hasTriggered_ = false;
    triggerFadeAlpha_ = kFullVisibleAlpha;

    // Transformの初期Z座標からトリガーZの初期値を自動計算（未設定の場合）
    if (owner_ && autoSyncTriggerZWithPosition_ && triggerZ_ == 0.0f) {
        float currentZ = owner_->GetPosition().z;
        triggerZ_ = currentZ - kDefaultTriggerDistanceAhead;
        if (triggerZ_ < 0.0f) triggerZ_ = 0.0f;
    }

    EnsureVisuals();
}

void EnemySpawnPointScript::Start() {
    EnsureVisuals();
}

void EnemySpawnPointScript::OnUpdate() {
    // シリンダー自転アニメーション
    visualRotationAngle_ += kRotationSpeed;

    // 敵数・ボス属性に応じた外向き拡散ソナー＆湧き上がりアニメーション位相更新
    float waveSpeed = kEnemyWaveBaseSpeed + static_cast<float>(count_) * kEnemyWaveCountSpeedBoost;
    if (isBoss_) {
        waveSpeed += kEnemyBossWaveSpeedBonus;
    }
    waveAnimPhase_ += waveSpeed;
    if (waveAnimPhase_ >= kWaveCycleMax) {
        waveAnimPhase_ -= kWaveCycleMax;
    }

    // ギミック作動後は即座に消さず、透明度を徐々に下げて滑らかにフェードアウトさせる
    if (hasTriggered_ && triggerFadeAlpha_ > 0.0f) {
        triggerFadeAlpha_ = (std::max)(0.0f, triggerFadeAlpha_ - kTriggerFadeOutSpeed);
    }
}

void EnemySpawnPointScript::EnsureVisuals() {
    // 1. 地面塗りつぶし円盤（内径0.0fの完全なディスクメッシュ）
    if (!groundDisc_) {
        groundDisc_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kUnitBaseRadius * kDiscInnerRadiusRatio);
        groundDisc_->SetAutoRegisterHierarchy(false);
        groundDisc_->Initialize(0); // 非ライティング（鮮明発光）
        groundDisc_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }); // 地面(XZ)に寝かせる
    }

    // 2. 地面外周リング（単位半径1.0mで1度だけメッシュ生成し、以降はSetScaleで高速拡縮）
    if (!groundRing_) {
        groundRing_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kUnitBaseRadius * kRingInnerRadiusRatio);
        groundRing_->SetAutoRegisterHierarchy(false);
        groundRing_->Initialize(0); // 非ライティング（鮮明発光）
        groundRing_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }); // 地面(XZ)に寝かせる
    }

    // 2-b. 外向き拡散ソナーリング（中心→外周へ広がる「敵出現」の直感表現）
    if (!expandWaveRing_) {
        expandWaveRing_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kUnitBaseRadius * kWaveInnerRadiusRatio);
        expandWaveRing_->SetAutoRegisterHierarchy(false);
        expandWaveRing_->Initialize(0);
        expandWaveRing_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
    }

    // 2-c. 湧き上がり浮上リング（地面→上空へ湧き出る立体モーション）
    if (!risingAuraRing_) {
        risingAuraRing_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kUnitBaseRadius * kRisingInnerRadiusRatio);
        risingAuraRing_->SetAutoRegisterHierarchy(false);
        risingAuraRing_->Initialize(0);
        risingAuraRing_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
    }

    // 3. 立ち上がる警戒円柱（半透明オーラ柱）
    if (!warningCylinder_) {
        warningCylinder_ = std::make_unique<CylinderObject>();
        warningCylinder_->SetAutoRegisterHierarchy(false);
        warningCylinder_->SetRadius(kUnitBaseRadius);
        warningCylinder_->SetHeight(kUnitBaseHeight);
        warningCylinder_->SetSubdivision(kCylinderSubdivisions);
        warningCylinder_->Initialize(0); // 非ライティング（鮮明半透明）
    }

    // 3. 敵情報表示用 Text2D コンポーネントのバインド（未所持なら追加）
    if (owner_ && !text2D_) {
        text2D_ = owner_->GetComponent<TextRenderer2DComponent>();
        if (!text2D_) {
            text2D_ = owner_->AddComponent<TextRenderer2DComponent>();
        }
        if (text2D_) {
            text2D_->SetFontSize(kDefaultTextFontSize);
            text2D_->SetOutlineEnabled(true);
            text2D_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 0.95f });
            text2D_->SetOutlineWidth(2.5f);
            text2D_->SetScreenSpace(true); // 3D親のワールドZによるファークリップや位置ずれを防止
        }
    }
}

float EnemySpawnPointScript::GetEffectiveRadius() const {
    // 単体・複数を問わず、エディタで設定されたスポーン・感知円の半径を常に反映
    return spawnRadius_;
}

void EnemySpawnPointScript::UpdateVisuals(bool isGamePlay) {
    if (!owner_) return;
    EnsureVisuals();

    Vector3 ownerPos = owner_->GetPosition();
    constexpr float kHalfPi = std::numbers::pi_v<float> * 0.5f;

    // 出現数と分散半径に基づく可視化外径の取得 (判定半径と完全同期)
    float effectiveRadius = GetEffectiveRadius();
    HazardZoneColorTheme theme = GetVisualColorTheme();
    float fadeMul = (isGamePlay && hasTriggered_) ? triggerFadeAlpha_ : kFullVisibleAlpha;

    // 1. 地面塗りつぶし円盤の更新（半透明ベース面）
    if (groundDisc_) {
        groundDisc_->SetPosition({ ownerPos.x, kGroundRingElevation, ownerPos.z });
        groundDisc_->SetScale({ effectiveRadius, effectiveRadius, 1.0f });
        groundDisc_->SetRotate({ kHalfPi, 0.0f, 0.0f });

        Vector4 discColor = isGamePlay ? theme.discPlay : theme.discEditor;
        discColor.w *= 0.65f * fadeMul;
        groundDisc_->SetColor(discColor);
        groundDisc_->Update();
    }

    // 2. 地面外周リングの更新
    if (groundRing_) {
        groundRing_->SetPosition({ ownerPos.x, kGroundRingElevation + kRingZBias, ownerPos.z });
        groundRing_->SetScale({ effectiveRadius, effectiveRadius, 1.0f });
        groundRing_->SetRotate({ kHalfPi, 0.0f, 0.0f });

        Vector4 ringColor = isGamePlay ? theme.ringPlay : theme.ringEditor;
        ringColor.w *= fadeMul;
        groundRing_->SetColor(ringColor);
        groundRing_->Update();
    }

    // 3. 外向き拡散ソナーリング（中心→外周へ広がる「敵が湧き出す」モーション）
    if (expandWaveRing_) {
        float waveScale = effectiveRadius * (kEnemyWaveStartScaleRatio + (1.0f - kEnemyWaveStartScaleRatio) * waveAnimPhase_);
        float waveAlpha = (1.0f - waveAnimPhase_) * kWaveAlphaPeak * fadeMul;
        Vector4 waveColor = { theme.ringEditor.x, theme.ringEditor.y, theme.ringEditor.z, waveAlpha };

        expandWaveRing_->SetPosition({ ownerPos.x, kGroundRingElevation + kWaveZBias, ownerPos.z });
        expandWaveRing_->SetScale({ waveScale, waveScale, 1.0f });
        expandWaveRing_->SetRotate({ kHalfPi, 0.0f, 0.0f });
        expandWaveRing_->SetColor(waveColor);
        expandWaveRing_->Update();
    }

    // 4. 湧き上がり浮上リング（地面→上空へ湧き出る立体モーション・敵数/ボスで到達高度が変化）
    if (risingAuraRing_) {
        float risePhase = std::fmod(waveAnimPhase_ + kHalfCycleOffset, kWaveCycleMax);
        float maxRiseHeight = isBoss_
            ? kEnemyBossRisingHeight
            : (kEnemyRisingBaseHeight + static_cast<float>(count_) * kEnemyRisingCountHeightStep);
        float riseY = kGroundRingElevation + risePhase * maxRiseHeight;
        float riseScale = effectiveRadius * (kEnemyRisingStartScale + (kEnemyRisingEndScale - kEnemyRisingStartScale) * risePhase);
        // 出現直後と消滅直前を滑らかにフェード
        float riseAlpha = std::sin(risePhase * std::numbers::pi_v<float>) * kRisingAlphaPeak * fadeMul;
        Vector4 riseColor = { theme.textColor.x, theme.textColor.y, theme.textColor.z, riseAlpha };

        risingAuraRing_->SetPosition({ ownerPos.x, riseY, ownerPos.z });
        risingAuraRing_->SetScale({ riseScale, riseScale, 1.0f });
        risingAuraRing_->SetRotate({ kHalfPi, 0.0f, 0.0f });
        risingAuraRing_->SetColor(riseColor);
        risingAuraRing_->Update();
    }

    // 5. 控えめな半透明オーラ円柱（DrawRoute/待機時のみ・敵数に応じた高さ）
    if (!isGamePlay && warningCylinder_) {
        float softHeight = isBoss_
            ? kEnemyBossRisingHeight
            : (kEnemyRisingBaseHeight + static_cast<float>(count_) * kEnemyRisingCountHeightStep * 0.6f);
        float halfHeight = softHeight * 0.5f;
        Vector4 cylColor = theme.cylinderEditor;
        cylColor.w *= kCylinderSoftAlphaRatio;

        warningCylinder_->SetPosition({ ownerPos.x, ownerPos.y + halfHeight + kGroundRingElevation, ownerPos.z });
        warningCylinder_->SetScale({ effectiveRadius, softHeight, effectiveRadius });
        warningCylinder_->SetRotate({ 0.0f, visualRotationAngle_, 0.0f });
        warningCylinder_->SetColor(cylColor);
        warningCylinder_->Update();
    }
}

void EnemySpawnPointScript::DrawVisuals(bool isGamePlay) {
    if (!showHazardZone_) return;

    // ゲーム進行中で、すでに敵が出現しフェードアウトが完了した場合は表示を終了
    if (isGamePlay && hasTriggered_ && triggerFadeAlpha_ <= kMinVisibleAlpha) return;

    // すべて Object3D_Alpha による半透明ホログラム描画へ統一
    if (groundDisc_) {
        groundDisc_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }
    if (groundRing_) {
        groundRing_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }
    if (expandWaveRing_) {
        expandWaveRing_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }
    if (risingAuraRing_) {
        risingAuraRing_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }
    if (!isGamePlay && warningCylinder_) {
        warningCylinder_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }
}

void EnemySpawnPointScript::UpdateText2D(bool isGamePlay) {
    (void)isGamePlay;
    if (!text2D_) return;

    // 3Dマップ上では文字表示を撤廃し、色・形・アニメーション（外向き拡散＆湧き上がり）で直感識別する設計へ統一
    text2D_->SetActive(false);
}

void EnemySpawnPointScript::Draw() {
    // 現在の進行エリア（currentArea）以外のスポーン情報は一切描画せず非表示
    auto* gamePhase = GamePhaseControllerScript::GetInstance();
    int activeArea = gamePhase ? gamePhase->GetCurrentArea() : 0;
    if (targetArea_ != activeArea) {
        if (text2D_) {
            text2D_->SetActive(false);
        }
        return;
    }

    // ゲーム進行中（走行戦闘フェーズ）かエディタ編集時かを自動判別
    bool isGamePlay = false;
    if (gamePhase) {
        isGamePlay = (gamePhase->GetCurrentPhase() == GamePhaseControllerScript::Phase::Play);
    } else if (owner_) {
        auto* scene = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene && scene->GetDirector()) {
            if (auto* phaseCtrl = scene->GetDirector()->GetScript<GamePhaseControllerScript>()) {
                isGamePlay = (phaseCtrl->GetCurrentPhase() == GamePhaseControllerScript::Phase::Play);
            }
        }
    }

    // 描画直前に Transform と各パラメータを完全同期（エディタ移動ギズモにも即座に追従）
    UpdateVisuals(isGamePlay);
    DrawVisuals(isGamePlay);
    UpdateText2D(isGamePlay);
}

/// <summary>
/// ボス設定のフラグを切り替え、プレハブ名やデフォルト設定を同期します。
/// </summary>
/// <param name="boss">ボスの場合は true</param>
void EnemySpawnPointScript::SetBoss(bool boss) {
    isBoss_ = boss;
    prefabName_ = isBoss_ ? kBossEnemyPrefab : kDefaultEnemyPrefab;
    if (isBoss_) {
        count_ = 1;
        targetArea_ = 3; // エリア4（0起点で3）
    }
}

/// <summary>
/// 自機の座標がスポーン円の感知範囲（またはトリガーライン）に達しているか判定し、越えていれば敵を生成します。
/// </summary>
bool EnemySpawnPointScript::CheckAndTrigger(const Vector3& playerPos, int currentArea) {
    if (hasTriggered_) return false;
    if (targetArea_ != currentArea) return false;
    if (!owner_) return false;

    if (useCircleRadiusTrigger_) {
        // 円形エリア侵入判定 (XZ平面の距離判定: 円に入っていないのに敵が沸くのを防止)
        Vector3 centerPos = owner_->GetPosition();
        float dx = playerPos.x - centerPos.x;
        float dz = playerPos.z - centerPos.z;
        float distSq = dx * dx + dz * dz;
        float triggerRadius = GetEffectiveRadius() + triggerMarginRadius_;
        if (distSq <= triggerRadius * triggerRadius) {
            SpawnEnemies();
            hasTriggered_ = true;
            return true;
        }
    } else {
        // 従来のZ進行度ライン判定
        if (playerPos.z >= triggerZ_) {
            SpawnEnemies();
            hasTriggered_ = true;
            return true;
        }
    }
    return false;
}

bool EnemySpawnPointScript::CheckAndTrigger(float playerPosZ, int currentArea) {
    Vector3 dummyPos = { 0.0f, 0.0f, playerPosZ };
    if (owner_) {
        dummyPos.x = owner_->GetPosition().x;
    }
    return CheckAndTrigger(dummyPos, currentArea);
}

/// <summary>
/// 設定されたパラメータ（プレハブ名、個数、分散半径）に基づいて、
/// 自身のTransform座標を中心とした円周上に敵GameObjectを動的生成してシーンへ登録します。
/// </summary>
std::vector<GameObject*> EnemySpawnPointScript::SpawnEnemies() {
    std::vector<GameObject*> spawnedList;
    if (!owner_) return spawnedList;

    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return spawnedList;

    Vector3 centerPos = owner_->GetPosition();
    Vector3 baseRot = owner_->GetRotate();

    for (int i = 0; i < count_; ++i) {
        Vector3 spawnPos = centerPos;

        // 複数出現時の円形分散オフセット計算 (円の内側に綺麗に分散配置)
        if (count_ > 1) {
            float placeRadius = (std::min)(spreadRadius_, spawnRadius_ * 0.75f);
            float angle = (static_cast<float>(i) / static_cast<float>(count_)) * 2.0f * std::numbers::pi_v<float>;
            spawnPos.x += std::cos(angle) * placeRadius;
            spawnPos.z += std::sin(angle) * placeRadius;
        }

        // プレハブから新規GameObjectをインスタンス化
        auto enemyGo = PrefabManager::GetInstance().Instantiate(prefabName_, spawnPos, baseRot);
        if (enemyGo) {
            GameObject* rawPtr = enemyGo.get();
            scene->AddGameObject(std::move(enemyGo));
            spawnedList.push_back(rawPtr);
        }
    }

    return spawnedList;
}

void EnemySpawnPointScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.6f, 0.2f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【敵スポーン地点設定 (Spawn Point)】");
    ImGui::Separator();

    // 1. 基本設定
    ImGui::Text("出現状態: %s", hasTriggered_ ? "出現済み (Triggered)" : "待機中 (Ready)");
    if (hasTriggered_) {
        ImGui::SameLine();
        if (ImGui::Button("リセット##ResetTrigger")) {
            ResetTrigger();
        }
    }

    // ボス設定
    if (ImGui::Checkbox("ボス出現ポイント##IsBoss", &isBoss_)) {
        SetBoss(isBoss_);
    }

    // 敵種類・プレハブ選択（種類ごとのカラーと対応）
    const char* displayNames[] = {
        "Enemy (通常敵 - 橙)",
        "FastEnemy (俊足/突撃敵 - 水色)",
        "Shooter (遠距離/射撃敵 - 緑)",
        "HeavyEnemy (大型/重装敵 - 赤)",
        "BossEnemy (ボス敵 - 紫)"
    };
    const char* prefabNames[] = {
        "Enemy",
        "FastEnemy",
        "Shooter",
        "HeavyEnemy",
        "BossEnemy"
    };
    constexpr int kOptionCount = sizeof(displayNames) / sizeof(displayNames[0]);

    int currentIdx = 0;
    for (int i = 0; i < kOptionCount; ++i) {
        if (prefabName_ == prefabNames[i]) {
            currentIdx = i;
            break;
        }
    }

    if (ImGui::Combo("敵タイプ/プレハブ##PrefabSelect", &currentIdx, displayNames, kOptionCount)) {
        prefabName_ = prefabNames[currentIdx];
        SetBoss(currentIdx == (kOptionCount - 1));
    }

    // 現在の警戒レベルとテーマカラーのプレビュー表示
    HazardZoneColorTheme theme = GetVisualColorTheme();
    std::string levelText = GetSpawnCountLevelText();
    ImGui::Text("現在の表示ラベル:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(theme.textColor.x, theme.textColor.y, theme.textColor.z, theme.textColor.w), "%s", levelText.c_str());

    // 所属エリア
    ImGui::SliderInt("所属エリア##TargetArea", &targetArea_, 0, 3, "Area %d (0~3)");

    // 出現トリガー設定（円形エリア侵入またはZライン通過）
    ImGui::Checkbox("円形エリア侵入で出現##UseCircleTrigger", &useCircleRadiusTrigger_);
    if (useCircleRadiusTrigger_) {
        constexpr float kMinMargin = 0.0f;
        constexpr float kMaxMargin = 10.0f;
        ImGui::DragFloat("円感知マージン##TriggerMargin", &triggerMarginRadius_, 0.1f, kMinMargin, kMaxMargin, "%.1f m");
        ImGui::TextDisabled("※自機がエディタ上の円（半径 %.1fm）に入ると敵が出現します", GetEffectiveRadius() + triggerMarginRadius_);
    } else {
        // 出現トリガーZ
        ImGui::DragFloat("出現トリガーZライン##TriggerZ", &triggerZ_, 1.0f, 0.0f, 1000.0f, "%.1f m");
        if (owner_) {
            float ownerZ = owner_->GetPosition().z;
            ImGui::TextDisabled("配置位置Z: %.1f m (トリガーまで: %.1f m)", ownerZ, ownerZ - triggerZ_);
            if (ImGui::Button("配置位置の25m手前にトリガーを合わせる##AlignTrigger")) {
                triggerZ_ = ownerZ - kDefaultTriggerDistanceAhead;
                if (triggerZ_ < 0.0f) triggerZ_ = 0.0f;
            }
        }
    }

    // 出現・感知円のサイズ調整（単体・複数を問わず全スポーンポイントで自由に拡縮可能）
    ImGui::DragFloat("出現・感知円の半径##SpawnRadius", &spawnRadius_, 0.2f, kMinSpawnRadius, kMaxSpawnRadius, "%.1f m");
    ImGui::TextDisabled("※この円のサイズを変更しながら配置でき、自機が入ると敵が出現します");

    // 出現数と分散
    if (!isBoss_) {
        if (ImGui::SliderInt("同時出現数##SpawnCount", &count_, kMinSpawnCount, kMaxSpawnCount, "%d 体")) {
            // カウント変更時に即座にラベル更新
        }
        ImGui::SameLine();
        const char* levelSummary = (count_ <= kFewCountThreshold) ? "【少】" : (count_ <= kMediumCountThreshold) ? "【普】" : "【多】";
        ImGui::TextDisabled("%s", levelSummary);

        if (count_ > 1) {
            ImGui::DragFloat("分散半径##SpreadRadius", &spreadRadius_, 0.2f, kMinSpreadRadius, spawnRadius_, "%.1f m");
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "【可視化・エディタ表示設定】");
    ImGui::Checkbox("ゾーン可視化を表示##ShowHazardZone", &showHazardZone_);
    if (showHazardZone_) {
        constexpr float kMinCylinderH = 1.0f;
        constexpr float kMaxCylinderH = 15.0f;
        ImGui::DragFloat("シリンダー高さ##CylinderHeight", &cylinderHeight_, 0.1f, kMinCylinderH, kMaxCylinderH, "%.1f m");
        ImGui::TextDisabled("※エディタ中: 鮮明シリンダー＋トリガー線で強調\n※プレイ中: 薄い半透明リングのみで邪魔しない");
    }

    ImGui::Separator();

    // 2. エディタプレビュー機能
    if (ImGui::Button("▶ この地点の敵をテスト生成##TestSpawnBtn")) {
        SpawnEnemies();
    }
#endif
}
