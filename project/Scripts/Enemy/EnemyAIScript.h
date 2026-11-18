#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>

class SpriteObject;
class Line2DObject;
class Ring2DObject;
class Circle2DObject;
class FlatRingObject;

/// <summary>
/// 敵キャラクターのAIステートマシン、部位別ダメージ判定、追従・攻撃を制御するスクリプトコンポーネント
/// </summary>
class EnemyAIScript : public ScriptComponent {
public:
    /// <summary>
    /// 敵のアーキタイプ種別（レールシューターの黄金律に基づく役割分担）
    /// </summary>
    enum class EnemyType {
        ParallelChaser, // 【横並走＆前方強襲型】自機の横を並走し、攻撃時は前に飛び出す（壁自動回避対応）
        Ambush,         // 【定点・待ち伏せ型】物陰や指定位置に陣取り、自機を迎撃
        FlyBy           // 【突撃・すれ違い型】一定速度ですれ違いざまに攻撃して後方へ離脱
    };

    enum class AiState {
        Aiming,            // 照準・ロックオン（自機へ視線を同期）
        AttackCharge,      // 攻撃チャージ予兆（収束リング・点滅）
        PounceStrike,      // 溜め位置からの空中跳躍切り込み＆空中隙露出（カウンター絶好のチャンス）
        UnavoidableAttack, // 弾丸発射
        Cooldown,          // 攻撃後クールタイム
        Stun,              // 攻撃阻止スタン
        Ghost              // 撃破後の幽霊状態（視界妨害・右クリ吸収へ接続）
    };

    EnemyAIScript();
    ~EnemyAIScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void Draw2D() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 自機弾命中時の被弾ダメージ処理を実行します。
    /// ヘッドショット（isCritical == true）の場合はダメージが2倍となり、被弾フラッシュ色（黄色）を一時的に適用します。
    /// </summary>
    /// <param name="amount">基礎ダメージ量</param>
    /// <param name="isCritical">頭部コライダーへの命中判定</param>
    void TakeDamage(int amount, bool isCritical = false);

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }

    /// <summary>
    /// 敵が完全に消滅したかを取得します。
    /// </summary>
    bool IsDead() const { return isDead_; }

    /// <summary>
    /// 敵が幽霊状態（視界妨害・右クリ吸収可能）であるかを取得します。
    /// </summary>
    bool IsGhost() const { return aiState_ == AiState::Ghost; }

    /// <summary>
    /// ボス属性を設定し、最大HPを高耐久値（kDefaultBossHp）に更新します。
    /// </summary>
    void SetBoss(bool isBoss);
    bool IsBoss() const { return isBoss_; }

    EnemyType GetEnemyType() const { return enemyType_; }
    void SetEnemyType(EnemyType type) { enemyType_ = type; }

    AiState GetAiState() const { return aiState_; }
    void SetAiState(AiState state) { aiState_ = state; }

private:
    /// <summary>
    /// レールシューター型ステートマシン（Ambush / FlyBy）を更新します。
    /// </summary>
    void UpdateAI();

    /// <summary>
    /// 自機座標へ向けて敵弾（Bulletプレハブ）を射出します。
    /// </summary>
    void FireBullet();

    /// <summary>
    /// 被弾時の点滅タイマーを更新し、タイマー終了時に元のメッシュマテリアル色へ復帰します。
    /// </summary>
    void HandleHitFlash();

    /// <summary>
    /// トランスフォーマー風アーケードターゲットサイトおよび円形HPバー用描画オブジェクトを初期化します。
    /// </summary>
    void EnsureTargetSightVisuals();

    /// <summary>
    /// 3D足元地面警告サークルおよび胴体ビルボード警告サークル用描画オブジェクトを初期化します。
    /// </summary>
    void EnsureChargeRingVisuals();

    /// <summary>
    /// 敵の円形ターゲットサイト（外周八角形HPゲージ＋照準クロスヘア＋攻撃予兆収束リング）を描画します。
    /// </summary>
    void DrawTargetSight();

    /// <summary>
    /// 敵の攻撃チャージ予兆3Dサークル（収縮する警告リング）を描画します。
    /// </summary>
    void DrawChargeRings();

    /// <summary>
    /// 地上溜め（AttackCharge）から空中浮遊（PounceStrike）を経て攻撃発射に至るまでの統合攻撃進行度（0.0 〜 1.0）を取得します。
    /// </summary>
    float GetOverallAttackProgress() const;

    /// <summary>
    /// 自機との間に遮蔽物（建物）があるかをレイキャストサンプリングで判定します。
    /// </summary>
    bool HasLineOfSightToPlayer() const;

    /// <summary>
    /// 指定座標の近傍にビル壁が存在するかをサンプリング判定します。
    /// </summary>
    bool IsNearBuilding(const Vector3& worldPos, float radius = 1.5f) const;

private:
    // マジックナンバー排除のための定数定義
    static constexpr int kDefaultNormalHp = 15;                // 通常敵HP（秒間20発連射で約0.75秒の撃ち応え）
    static constexpr int kDefaultBossHp = 150;                 // ボス敵HP（約7.5秒の集中連射）
    static constexpr float kNormalTargetCenterYOffset = 0.0f;  // 通常敵の真ん中・照準中心Yオフセット (頭の上ではなく中心)
    static constexpr float kBossTargetCenterYOffset = 0.0f;    // ボス敵の真ん中・照準中心Yオフセット
    static constexpr float kNormalTargetRadius = 28.0f;        // 通常敵のサイトリング半径 (px)
    static constexpr float kBossTargetRadius = 45.0f;          // ボス敵のサイトリング半径 (px)
    static constexpr int kHpSegmentCount = 8;                  // 八角形HPバーのセグメント数
    static constexpr float kHpSegmentThickness = 4.0f;         // HPバーセグメント線の太さ (px)
    static constexpr float kHpSegmentGapRatio = 0.12f;         // セグメント間のスリット隙間率
    static constexpr float kAttackGaugeSpeed = 0.016f;         // 攻撃チャージ速度（約1.0秒）
    static constexpr float kChargePhaseRatio = 0.20f;          // 全体攻撃シークエンスにおける地上チャージの割合 (20%: 残り80%を空中でゆっくり収縮)
    static constexpr int kDefaultCooldownDuration = 120;       // 攻撃後のクールタイムフレーム数（約2.0秒）
    static constexpr int kEnemyBulletDamage = 1;               // 敵弾ダメージ（1ダメージ）
    static constexpr float kCullDistanceBack = 40.0f;          // 自機通過後のカリング距離 (m)
    static constexpr float kAmbushPerceptionRange = 40.0f;     // 待ち伏せ型がプレイヤーを感知して照準を開始する距離 (m)
    static constexpr float kFlyBySpeed = 0.32f;                // すれ違い型の直線前進速度 (m/frame)
    static constexpr int kHitFlashDuration = 11;               // 被弾点滅持続フレーム数 (約0.18秒で鮮明に視認)
    static constexpr float kHitKnockbackDist = 0.20f;          // 被弾時の衝撃押し戻しノックバック距離 (m)
    static constexpr int kDefaultHitSquashDuration = 6;        // 被弾縮みバウンド持続フレーム数 (約0.10秒)
    static constexpr Vector3 kHitSquashScale = { 1.12f, 0.86f, 1.12f }; // 被弾時の縮みバウンド変形比率 (Y軸潰れ、XZ軸膨張)
    static constexpr float kBulletSpawnOffsetZ = -1.0f;        // 弾発射オフセット
    static constexpr float kEnemyBulletSpeed = 0.35f;

    // 3D攻撃予兆サークル（収縮する地面＆胸部警告リング）定数 (マジックナンバー排除)
    static constexpr float k3DChargeRingBaseOuterRadius = 1.0f; // 基準外径 (m)
    static constexpr float k3DChargeRingBaseInnerRadius = 0.85f;// 基準内径 (m: 幅0.15mのくっきりリング)
    static constexpr float k3DChargeRingStartScale = 0.80f;     // チャージ開始時の外径スケール (0.80m: 敵胴体幅にジャストフィットし視認性抜群)
    static constexpr float k3DChargeRingEndScale = 0.0f;        // 攻撃発動時の完全収縮スケール (0.0m！完全に敵の中心へ収束)
    static constexpr float k3DChargeRingGroundY = 0.05f;        // 地面すれすれの接地高度 (m)
    static constexpr float k3DChargeRingForwardOffset = 0.32f;  // 敵中心から自機（カメラ）方向へ手前に出すオフセット (m)
    static constexpr Vector4 k3DChargeRingColor = { 1.0f, 0.15f, 0.15f, 0.48f };     // 半透明の警告赤（敵が透けて見える）
    static constexpr Vector4 k3DChargeRingBlinkColor = { 1.0f, 0.95f, 0.95f, 0.72f }; // 臨界時の白熱点滅（半透明）

    // 横並走＆空中跳躍切り込み・壁回避定数 (マジックナンバー排除)
    static constexpr float kSideDistanceX = 6.8f;              // 自機からの横並走・移送距離 (m)（ゆったり拡張）
    static constexpr float kParallelLeadZ = 10.0f;             // 並走時の自機前方相対Z位置 (m)（真横ではなく斜め前方へ）
    static constexpr float kChargeLeadZ = 15.0f;               // 攻撃チャージ時の自機前方相対Z位置 (m)（開けた前方中距離で堂々とチャージ）
    static constexpr float kChargeSideOffsetX = 5.2f;          // 攻撃チャージ時の横オフセット (m)
    
    // 扇状3アタックゾーン定数（狙う場所の固定化防止: 斜め前・正面上空・その場横ジャンプの中距離最適化）
    static constexpr float kZone1OffsetX = 4.2f;               // Zone 1: 斜め前方横オフセット (m)
    static constexpr float kZone1OffsetZ = 13.5f;              // Zone 1: 斜め前方Zオフセット (m)（快適な中距離交戦レンジ）
    static constexpr float kZone1OffsetY = 1.5f;               // Zone 1: 斜め前方高度 (m)（目線よりわずかに上の見やすい高さ）

    static constexpr float kZone2OffsetX = 0.8f;               // Zone 2: 正面上空横オフセット (m)
    static constexpr float kZone2OffsetZ = 16.0f;              // Zone 2: 正面上空Zオフセット (m)（正面奥の堂々と狙えるレンジ）
    static constexpr float kZone2OffsetY = 2.0f;               // Zone 2: 正面上空高度 (m)（見失わない正面の空）

    static constexpr float kZone3OffsetX = 5.0f;               // Zone 3: 側方ジャンプ横オフセット (m)（視界内に収まる横幅）
    static constexpr float kZone3OffsetZ = 11.0f;              // Zone 3: 側方ジャンプZオフセット (m)（視界端に綺麗に見える前方）
    static constexpr float kZone3OffsetY = 1.3f;               // Zone 3: 側方ジャンプ高度 (m)（目の前の絶妙な高さ）

    static constexpr float kAirborneLeapSpeed = 0.16f;         // 空中跳躍の加速Lerp係数
    static constexpr int kAirborneVulnerableDuration = 300;    // 空中滞空で隙を晒すフレーム数（5.0秒！じっくり狙い撃てる時間）
    static constexpr float kAirborneHoverFrequency = 8.0f;     // 空中ホバリング微振動の周波数
    static constexpr float kAirborneHoverAmplitude = 0.08f;    // 空中ホバリング微振動の振幅 (m)
    static constexpr float kAirbornePitchTilt = 0.22f;         // 空中見下ろし前傾ピッチ角度 (rad)
    static constexpr float kAirborneRollTilt = 0.16f;          // 空中切り込みバンクロール角度 (rad)
    static constexpr float kEnemyBaseScale = 0.68f;            // マップ縮尺に美しく調和する敵基本スケール (m)
    static constexpr float kEnemyGroundYOffset = 0.68f;         // 敵接地高さオフセット (cube.obj 半径1.0 × スケール0.68m、足元をY=0に接地)
    static constexpr float kAirborneBaseScaleMultiplier = 1.15f; // 空中隙露出時のスマート拡大率 (1.15倍)
    static constexpr float kAirbornePulseScaleDelta = 0.08f;   // スマートな脈動幅 (+0.08倍 → 最大1.23倍)
    static constexpr float kAirborneScalePulseFrequency = 6.0f;// 拡大脈動周波数
    static constexpr float kRetreatSideDistanceX = 7.5f;       // 射撃後の外側離脱距離 (m)
    static constexpr float kRetreatLerpSpeed = 0.08f;          // 射撃後の離脱Lerp係数
    static constexpr float kWallCheckMargin = 1.5f;            // 壁近接判定マージン (m)
    static constexpr float kParallelLerpSpeed = 0.05f;         // 並走追従の滑らかなLerp係数
    static constexpr int kDefaultCruiseDuration = 80;          // 飛びかかりまでの並走フレーム数（約1.3秒）
    static constexpr float kEnemyBodyRadius = 0.6f;            // 敵の衝突判定半径 (m)

    // ワープ（瞬間移動）防止＆敵同士重なり防止定数 (マジックナンバー排除)
    static constexpr float kMaxEnemyFrameSpeed = 0.35f;        // 1フレームあたりの最大移動速度キャップ (m/frame)
    static constexpr float kCatchUpDistanceThreshold = 8.0f;   // 自機より後方に取り残されたと判定する距離 (m)
    static constexpr float kEnemySeparationDistance = 2.2f;    // 敵同士が重ならないための最小安全距離 (m)
    static constexpr float kEnemySeparationPushForce = 0.5f;   // 敵同士の重なり反発押し戻し係数

    // 頭上飛び越えオーバーヘッドジャンプ定数 (マジックナンバー排除)
    static constexpr int kVaultDuration = 45;                  // 逆サイド飛び越え滞空フレーム数（約0.75秒）
    static constexpr float kVaultApexHeight = 3.4f;            // プレイヤー頭上を越える最高高度 (m)

    // Fortnite風 画面中央脅威＆サウンドビジュアライザー定数 (マジックナンバー排除)
    static constexpr float kThreatBaseRadius = 115.0f;         // 画面中央放射状目盛り円の基準半径 (px)
    static constexpr int kRadialTickCount = 20;                // 全周の放射状目盛り数
    static constexpr float kRadialTickInnerLen = 6.0f;         // 目盛り線の内側長さ (px)
    static constexpr float kRadialTickOuterLen = 6.0f;         // 目盛り線の外側長さ (px)
    static constexpr float kRadialTickPulseScale = 18.0f;      // 接近音波による最大伸長幅 (px)
    static constexpr float kRadialTickAlpha = 0.12f;           // 目盛りの控えめな常時表示アルファ値
    static constexpr float kThreatRayInnerGap = 10.0f;         // 警告円と警告線の隙間 (px)
    static constexpr float kThreatRayLength = 62.0f;           // 敵方角へ伸びる警告線の長さ (px)
    static constexpr float kThreatRayThickness = 5.5f;         // 警告線の太さ (px)
    static constexpr float kThreatArrowWingLength = 20.0f;     // くさび型矢印の羽の長さ (px)
    static constexpr float kThreatArrowWingAngle = 2.45f;      // くさび型矢印の開き角 (rad)
    static constexpr int kThreatArcSegmentCount = 5;           // 警戒円弧のセグメント数
    static constexpr float kThreatArcThickness = 4.0f;         // 警戒円弧の太さ (px)
    static constexpr float kSoundPerceptionRange = 40.0f;      // 走行音・波形を検知表示する最大距離 (m)

    // トランスフォーマー風アタックマーカー定数 (マジックナンバー排除)
    static constexpr int kStunDuration = 45;                   // 攻撃阻止スタン持続フレーム数（約0.75秒）
    static constexpr float kTimingMarkerStartRadius = 40.0f;   // タイミング収束リング最大半径 (px) (スマートに小型化し弱点へ綺麗に収束)
    static constexpr float kMarkerThickness = 2.5f;            // 収束リング枠線の太さ (px)
    static constexpr float kCenterDotRadius = 4.0f;            // 中心ドット半径 (px)
    static constexpr float kCenterCrosshairSize = 10.0f;       // 中心照準クロスヘアの長さ (px)
    static constexpr float kCenterCrosshairGap = 4.0f;         // クロスヘアとサイトリングの間隔 (px)
    static constexpr float kChargeShakeIntensity = 0.05f;      // チャージ中の微小震動強度

    // メンバ変数
    int maxHp_ = kDefaultNormalHp;
    int hp_ = kDefaultNormalHp;
    bool isBoss_ = false;
    bool isDead_ = false;

    EnemyType enemyType_ = EnemyType::ParallelChaser; // デフォルトは横並走＆前方強襲型
    AiState aiState_ = AiState::Aiming;
    float attackGauge_ = 0.0f;
    int cooldownTimer_ = 0;
    int cooldownDuration_ = kDefaultCooldownDuration;
    int parallelCruiseTimer_ = 0;
    int strikeVulnerableTimer_ = 0;                   // 空中滞空・隙タイマー
    int attackZoneIndex_ = 0;                         // 扇状3アタックゾーン番号 (0: 斜め前, 1: 正面上空, 2: 側方ジャンプ)
    float sideSign_ = 1.0f;                           // +1.0f: 右側並走 / -1.0f: 左側並走
    int bulletDamage_ = kEnemyBulletDamage;
    int hitFlashTimer_ = 0;
    int hitSquashTimer_ = 0;                          // 被弾縮みバウンドタイマー
    int stunTimer_ = 0;
    float markerPulseTimer_ = 0.0f;

    // FlyBy型用ベクトル
    Vector3 flyByDir_ = { 0.0f, 0.0f, -1.0f };
    bool flyByInitialized_ = false;

    Vector4 originalColor_ = { 1.0f, 0.1f, 0.1f, 1.0f }; // 敵基本色（赤）
    Vector3 originalScale_ = { 1.0f, 1.0f, 1.0f };       // 敵基本スケール
    bool isOriginalScaleSaved_ = false;                  // 初期スケール退避フラグ

    // 頭上飛び越えオーバーヘッドジャンプ制御変数
    bool isVaulting_ = false;
    int vaultTimer_ = 0;
    float vaultStartX_ = 0.0f;
    float vaultTargetX_ = 0.0f;
    void StartVault(float targetSideSign);

    // トランスフォーマー風アーケードターゲットサイト＆円形HPバー描画用オブジェクト (DirectX 12)
    std::unique_ptr<Line2DObject> hpSegments_[kHpSegmentCount]; // 八角形HPセグメントゲージ
    std::unique_ptr<Ring2DObject> targetBaseRing_;              // 下地ベースリング（暗色枠）
    std::unique_ptr<Ring2DObject> attackTimingRing_;            // 攻撃予兆タイミング収束リング
    std::unique_ptr<Circle2DObject> targetCenterDot_;           // 照準中心ドット
    std::unique_ptr<Line2DObject> targetCrossLines_[4];         // 十字照準クロスヘア

    // 3D空間攻撃予兆サークル（収縮する警告リング）用オブジェクト (DirectX 12)
    std::unique_ptr<FlatRingObject> groundChargeRing_;          // 足元地面に広がる警告リング
    std::unique_ptr<FlatRingObject> bodyChargeRing_;            // 敵胴体周囲に広がる警告リング

    // Fortnite風 画面中央放射状目盛りサークル＆脅威インジケーター描画用オブジェクト (DirectX 12)
    std::unique_ptr<Line2DObject> radialTicks_[kRadialTickCount]; // 全周放射状目盛り線（サンバースト）
    std::unique_ptr<Line2DObject> threatRay_;                   // 敵方角へ伸びる警告主線
    std::unique_ptr<Line2DObject> threatArrowWings_[2];         // 警告矢印の左右両翼
    std::unique_ptr<Line2DObject> threatArcBars_[kThreatArcSegmentCount]; // 警告円周上の赤い警戒アーク
    void DrawThreatIndicator();                                 // 脅威インジケーター描画メソッド

    // 敵同士の重なり防止（エネミーセパレーション）
    static inline std::vector<EnemyAIScript*> sActiveEnemies_;  // シーン上の全アクティブ敵インスタンス
    static inline int sNextZoneIndex_ = 0;                      // スポーン敵へのゾーン交互割り当てカウンター
    void ApplyEnemySeparation(Vector3& pos);                    // 敵同士の重なり反発押し出し処理
};
