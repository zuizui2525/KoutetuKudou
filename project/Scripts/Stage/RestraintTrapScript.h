#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>
#include <vector>

class GameObject;
class FlatRingObject;
class CylinderObject;

/// <summary>
/// ステージギミック：拘束停止円トラップ（Restraint Circle）
/// 自機が円内に侵入すると、一定時間（デフォルト3.5秒）移動を強制停止させ、
/// 周囲の敵を迎え撃つ緊迫した防衛戦（将来のラッシュウェーブ連動対応）を発生させます。
/// </summary>
class RestraintTrapScript : public ScriptComponent {
public:
    enum class TrapState {
        Armed,    // 待機中（侵入で発動）
        Active,   // 拘束発動中（エネルギー檻展開＆カウントダウン中）
        Disarmed  // 解除済み（制圧完了）
    };

    RestraintTrapScript();
    ~RestraintTrapScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// トラップを初期待機状態（Armed）へリセットします。
    /// </summary>
    void ResetTrap();

    // ゲッター / セッター
    float GetTrapRadius() const { return trapRadius_; }
    void SetTrapRadius(float radius) { trapRadius_ = radius; }

    float GetRestraintDurationSec() const { return GetEffectiveRestraintDurationSec(); }
    void SetRestraintDurationSec(float seconds) { restraintDurationSec_ = seconds; }
    float GetEffectiveRestraintDurationSec() const;

    float GetDurationPerEnemySec() const { return durationPerEnemySec_; }
    void SetDurationPerEnemySec(float secPerEnemy);

    int GetTargetArea() const { return targetArea_; }
    void SetTargetArea(int area) { targetArea_ = area; }

    TrapState GetTrapState() const { return state_; }
    bool IsDisarmed() const { return state_ == TrapState::Disarmed; }
    bool IsActive() const { return state_ == TrapState::Active; }

    // Step 1-3（拘束連動エネミーラッシュ）設定
    bool IsTriggerRushOnActivate() const { return triggerRushOnActivate_; }
    void SetTriggerRushOnActivate(bool enable) { triggerRushOnActivate_ = enable; }
    int GetRushEnemyCount() const { return rushEnemyCount_; }
    void SetRushEnemyCount(int count);

    /// <summary>
    /// シーン上に存在するすべての拘束トラップ一覧を取得します（ミニマップ同期・一括制御用）。
    /// </summary>
    static const std::vector<RestraintTrapScript*>& GetActiveTraps() { return sActiveTraps_; }

    /// <summary>
    /// シーン上の全拘束トラップを待機状態（Armed）へ一括リセットします。
    /// </summary>
    static void ResetAllTraps();

    /// <summary>
    /// 現在の状態に応じたミニマップ表示用カラー（RGBA）を取得します。
    /// </summary>
    Vector4 GetMapColor() const;

private:
    void EnsureVisuals();
    void UpdateAndDrawVisuals(bool isPlayPhase);

    /// <summary>
    /// 拘束発動中の連続エネミーラッシュ（時間差360度包囲湧き）を更新します。
    /// </summary>
    void UpdateRushWave();

    /// <summary>
    /// 指定ウェーブ番号のラッシュ敵を1体、死角・前方の安全な道路座標へ出現させます。
    /// </summary>
    void SpawnSingleRushEnemy(int waveIndex);

    /// <summary>
    /// 建物に埋まらず自機から射線が通る安全な360度包囲スポーン座標を探索します。
    /// </summary>
    bool FindSafeRushSpawnPosition(const Vector3& playerPos, float desiredAngleRad,
                                   Vector3& outPos, float& outAngleRad) const;

private:
    // マジックナンバー排除のための定数定義
    static constexpr float kDefaultTrapRadius = 4.5f;             // デフォルト拘束円半径 (m)
    static constexpr float kMinTrapRadius = 1.5f;                 // 最小半径 (m)
    static constexpr float kMaxTrapRadius = 15.0f;                // 最大半径 (m)
    static constexpr float kDefaultDurationPerRushEnemySec = 2.0f;// 敵1体あたりの基準拘束時間 (秒/体)
    static constexpr float kMinDurationPerEnemySec = 0.5f;        // 敵1体あたり最小拘束時間 (秒/体)
    static constexpr float kMaxDurationPerEnemySec = 5.0f;        // 敵1体あたり最大拘束時間 (秒/体)
    static constexpr float kDefaultRestraintDurationSec = 6.0f;   // デフォルト拘束時間 (3体 × 2.0秒 = 6.0秒)
    static constexpr float kMinRestraintDurationSec = 1.0f;       // 最小拘束時間 (秒)
    static constexpr float kMaxRestraintDurationSec = 20.0f;      // 最大拘束時間 (秒)
    static constexpr float kTargetFps = 60.0f;                    // 秒→フレーム変換基準FPS
    static constexpr uint32_t kRingDivideCount = 32;              // リング分割数
    static constexpr uint32_t kCageSubdivisions = 24;             // 檻シリンダー分割数
    static constexpr float kUnitBaseRadius = 1.0f;                // 基準外径 (m)
    static constexpr float kDiscInnerRadius = 0.0f;               // 地面円盤の内径 (0.0で塗りつぶし)
    static constexpr float kOuterRingInnerRadius = 0.86f;         // 外周警告リングの内径比率
    static constexpr float kPulseRingInnerRadius = 0.90f;         // 内側収束パルスリングの内径比率
    static constexpr float kTrapDiscElevation = 0.04f;            // 地面円盤の接地浮かせ高さ (m)
    static constexpr float kTrapRingElevation = 0.055f;           // 外周リングの接地浮かせ高さ (m)
    static constexpr float kTrapPulseRingElevation = 0.065f;      // 脈動リングの接地浮かせ高さ (m)
    static constexpr float kCageActiveHeight = 3.2f;              // 発動時のエネルギー檻高さ (m)
    static constexpr float kCagePreviewHeight = 1.6f;             // ルート手書き/待機時のガイド高さ (m)
    static constexpr float kCagePreviewBobAmp = 0.28f;            // 待機時エネルギー檻の上下脈動幅 (m)
    static constexpr float kFloatingRingInnerRadius = 0.84f;      // 上部浮遊ロックリングの内径比率
    static constexpr float kFloatingRingPreviewBaseY = 1.65f;     // 待機時上部ロックリングの基準高度 (m)
    static constexpr float kFloatingRingActiveBaseY = 3.1f;       // 発動時上部ロックリングの基準高度 (m)
    static constexpr float kFloatingRingBobAmpY = 0.35f;          // 上部ロックリングの上下プレス脈動幅 (m)
    static constexpr float kFloatingRingBobSpeed = 0.045f;        // 上部ロックリングの上下脈動速度 (rad/frame)
    static constexpr float kPulseSpeed = 0.025f;                  // パルス収縮アニメーション速度
    static constexpr float kCageRotationSpeed = 0.03f;            // エネルギー檻の自転速度
    static constexpr float kHalfPi = 1.5707963f;                  // 90度 (rad)
    static constexpr float kMinVisibleScale = 0.05f;              // 最小可視スケール閾値
    static constexpr float kFullVisibleAlpha = 1.0f;              // 完全不透明度係数
    static constexpr float kDisarmFadeOutSpeed = 0.020f;          // 作動後の透明度減衰速度 (/frame, 約0.83秒で滑らかに消滅)
    static constexpr float kMinVisibleAlpha = 0.005f;             // 描画打ち切り最小アルファ閾値

    // カラーパレット（通常敵オレンジと明確に区別できるネオンパープル／警告マゼンタレッド）
    static constexpr Vector4 kArmedDiscColor = { 0.72f, 0.12f, 0.95f, 0.24f };
    static constexpr Vector4 kArmedRingColor = { 0.88f, 0.22f, 1.0f, 0.78f };
    static constexpr Vector4 kArmedPulseColor = { 0.95f, 0.45f, 1.0f, 0.65f };
    static constexpr Vector4 kArmedCageColor = { 0.78f, 0.18f, 1.0f, 0.16f };
    static constexpr Vector4 kArmedLockRingColor = { 0.95f, 0.38f, 1.0f, 0.82f };

    static constexpr Vector4 kActiveDiscColor = { 1.0f, 0.12f, 0.35f, 0.36f };
    static constexpr Vector4 kActiveRingColor = { 1.0f, 0.18f, 0.42f, 0.90f };
    static constexpr Vector4 kActivePulseColor = { 1.0f, 0.75f, 0.82f, 0.78f };
    static constexpr Vector4 kActiveCageColor = { 1.0f, 0.15f, 0.40f, 0.28f };
    static constexpr Vector4 kActiveLockRingColor = { 1.0f, 0.30f, 0.55f, 0.92f };

    // Step 1-3: 拘束連動エネミーラッシュ定数 (マジックナンバー排除)
    static constexpr int kDefaultRushEnemyCount = 3;              // デフォルト連続湧き敵数
    static constexpr int kMinRushEnemyCount = 1;                  // 最小ラッシュ数
    static constexpr int kMaxRushEnemyCount = 8;                  // 最大ラッシュ数
    static constexpr int kRushSpawnIntervalFrames = 33;           // 連続湧き間隔 (約0.55秒)
    static constexpr int kRushEnemyHp = 10;                       // 360度迎撃バトル向け爽快HP (通常15→10)
    static constexpr float kRushSpawnDistanceFar = 12.0f;         // 第一候補スポーン距離 (m)
    static constexpr float kRushSpawnDistanceNear = 8.5f;         // 狭い路地用フォールバック距離 (m)
    static constexpr float kRushEnemyGroundY = 0.68f;             // 敵接地Y座標 (m)
    static constexpr float kRushBuildingSafeMargin = 1.6f;        // ビル壁安全マージン (m)
    static constexpr int kRushLosSampleSteps = 6;                 // スポーン時射線チェック分割数
    static constexpr uint32_t kRushSpawnSparkCount = 35;          // 転送出現スパーク粒子数

    // メンバ変数
    float trapRadius_ = kDefaultTrapRadius;
    float durationPerEnemySec_ = kDefaultDurationPerRushEnemySec;
    float restraintDurationSec_ = kDefaultRestraintDurationSec;
    bool autoCalcDurationByEnemyCount_ = true;
    int targetArea_ = 0;
    TrapState state_ = TrapState::Armed;
    int activeTimerFrames_ = 0;
    float visualPulseTimer_ = 0.0f;
    float cageRotationAngle_ = 0.0f;
    float floatingBobTimer_ = 0.0f;
    float disarmFadeAlpha_ = kFullVisibleAlpha;
    bool triggerRushOnActivate_ = true;
    int rushEnemyCount_ = kDefaultRushEnemyCount;
    int rushSpawnedCount_ = 0;
    int rushSpawnCooldown_ = 0;
    float rushBaseForwardYaw_ = 0.0f;

    GameObject* cachedPlayer_ = nullptr;

    // 3Dビジュアルオブジェクト（毎フレーム頂点再生成を行わずSetScaleのみで安全高速描画）
    std::unique_ptr<FlatRingObject> groundDisc_;
    std::unique_ptr<FlatRingObject> outerRing_;
    std::unique_ptr<FlatRingObject> innerPulseRing_;
    std::unique_ptr<FlatRingObject> floatingLockRing_;
    std::unique_ptr<CylinderObject> energyCage_;

    static std::vector<RestraintTrapScript*> sActiveTraps_;
};
