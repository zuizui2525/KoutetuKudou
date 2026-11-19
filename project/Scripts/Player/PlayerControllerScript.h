#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Audio/AudioStructs.h"
#include <string>
#include <memory>
#include <vector>
#include <array>

class ModelObject;
class FlatRingObject;
class ConeObject;
class GPUParticleObject;
class SpriteObject;
class CubeObject;
class Ring2DObject;

/// <summary>
/// 自機キャラクターのHP、弾数管理、射撃、リロードを制御するスクリプトコンポーネント
/// </summary>
class PlayerControllerScript : public ScriptComponent {
public:
    PlayerControllerScript();
    ~PlayerControllerScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void Draw2D() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 敵弾等から被弾した際のダメージ処理を実行します。
    /// HPが0以下になった場合に死亡フラグ（isDead_）をセットします。
    /// </summary>
    /// <param name="amount">受けるダメージ量</param>
    void TakeDamage(int amount);

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }

    /// <summary>
    /// 自機の死亡状態（撃墜済みか）を取得します。
    /// </summary>
    /// <returns>撃墜済みの場合 true</returns>
    bool IsDead() const { return isDead_; }

    /// <summary>
    /// 死亡溶解演出が進行中であるかを取得します。
    /// </summary>
    bool IsMelting() const { return isMelting_; }

    /// <summary>
    /// 死亡演出が完全に終了したか（シーン遷移許可フラグ）を取得します。
    /// </summary>
    bool IsDeathAnimationFinished() const { return isDead_ && !isMelting_; }

    /// <summary>
    /// 被弾後の無敵時間中であるかを取得します。
    /// </summary>
    /// <returns>無敵時間中の場合 true</returns>
    bool IsInvincible() const { return invincibilityTimer_ > 0; }

    // 弾数・リロード情報
    int GetAmmo() const { return ammo_; }
    int GetMaxAmmo() const { return maxAmmo_; }

    /// <summary>
    /// 現在リロード中（弾丸装填待機中）であるかを取得します。
    /// </summary>
    /// <returns>リロード中の場合 true</returns>
    bool IsReloading() const { return reloadTimer_ > 0; }
    int GetReloadTimer() const { return reloadTimer_; }
    int GetReloadDuration() const { return reloadDuration_; }

    /// <summary>
    /// レール走行上の進行方向ベクトルを設定します。射撃時の前方方向として利用されます。
    /// </summary>
    /// <param name="dir">進行方向（正規化ベクトル）</param>
    void SetDirection(const Vector3& dir) { direction_ = dir; }
    const Vector3& GetDirection() const { return direction_; }

    /// <summary>
    /// 走行フェーズ中かどうかの自動前進フラグを設定します。
    /// 走行中のみ射撃やリロードの入力が受け付けられます。
    /// </summary>
    /// <param name="autoMove">走行中の場合 true</param>
    void SetAutoMoving(bool autoMove) { isAutoMoving_ = autoMove; }
    bool IsAutoMoving() const { return isAutoMoving_; }

    /// <summary>
    /// 自機のルート走行速度を取得します。
    /// </summary>
    float GetMoveSpeed() const { return moveSpeed_; }

    /// <summary>
    /// リロード中の完全停止（速度0）および射撃中の減速（通常の3分の1）を反映した現在の実効レール走行速度を取得します。
    /// </summary>
    float GetEffectiveMoveSpeed() const {
        if (IsReloading()) {
            return kReloadingMoveSpeed;
        }
        return isShooting_ ? (moveSpeed_ * shootingMoveSpeedMultiplier_) : moveSpeed_;
    }

    /// <summary>
    /// 現在射撃中（減速適用中）であるかを取得します。
    /// </summary>
    bool IsShooting() const { return isShooting_; }

    /// <summary>
    /// 自機のルート走行速度を設定します。
    /// </summary>
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    /// <summary>
    /// 自機の1フレームあたりの移動差分ベクトル（ワールド速度）を取得します。
    /// </summary>
    const Vector3& GetVelocity() const { return currentVelocity_; }

    /// <summary>
    /// 弾丸への自機移動慣性同期の有効/無効を設定します。
    /// </summary>
    void SetEnableInertia(bool enable) { enableInertia_ = enable; }
    bool IsEnableInertia() const { return enableInertia_; }

    /// <summary>
    /// 現在の銃口（マズル先端）の3Dワールド座標を取得します。
    /// 銃モデルのワールドアフィン変換行列（カメラ追従・構えオフセット・リコイル）に基づいて計算されます。
    /// </summary>
    Vector3 GetMuzzleWorldPosition() const;

    /// <summary>
    /// 現在のマウスカーソル（レティクル）が指す3D空間上の着弾目標地点を取得します。
    /// </summary>
    Vector3 GetAimTargetPosition() const;

    /// <summary>
    /// 現在右クリック長押しによる幽霊吸引（Ghost Vacuum）を実行中かを取得します。
    /// </summary>
    bool IsVacuuming() const { return isVacuuming_; }

    /// <summary>
    /// ビル群や床の不透明描画完了後に呼び出し、半透明3D吸引ボルテックスリングおよびマズル硝煙を
    /// 深度バッファと正しく合成して描画します（ビルの裏に隠れる描画順不具合を根絶）。
    /// </summary>
    void DrawVacuumVortex3D();

    /// <summary>
    /// Step 1-6: ダダサバイバー風 3択アップグレード種別（全8種）
    /// </summary>
    enum class UpgradeType {
        MultiShot = 0,       // 武器変化: 同時発射弾数 +1（扇状レーザー）
        PiercingLaser = 1,   // 武器変化: 敵貫通回数 +1
        HighPower = 2,       // 火力強化: 弾丸威力 +1
        RapidFire = 3,       // 連射強化: 連射間隔短縮＆弾速UP
        ExtendedMag = 4,     // 取り回し: 最大装弾数 +20＆即フル装填
        QuickReload = 5,     // 取り回し: リロード時間 -22%短縮
        ArmorRepair = 6,     // 生存強化: 最大HP +3＆HP +5回復
        SoulVacuumBoost = 7, // 吸引特化: 幽霊吸引速度 +45%＆射程拡大
        Count = 8
    };

    static constexpr int kUpgradeTypeCount = static_cast<int>(UpgradeType::Count);
    static constexpr int kUpgradeChoiceCount = 3;

    struct UpgradeCardInfo {
        UpgradeType type = UpgradeType::MultiShot;
        std::string title;
        std::string categoryLabel;
        std::string description;
        Vector4 themeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        int currentLevel = 0;
        int maxLevel = 3;
    };

    /// <summary>
    /// これまでに吸引・成仏させた幽霊の累計数を取得します（Step 1-6 EXPシステム連携用）。
    /// </summary>
    int GetAbsorbedGhostCount() const { return absorbedGhostCount_; }

    /// <summary>
    /// 経験値（EXP）を待機キューへ加算し、毎フレーム1ずつゲージへ反映します。
    /// 必要値に到達した場合はレベルアップと3択強化権利を予約します。
    /// </summary>
    void GainExp(int amount);

    int GetPlayerLevel() const { return playerLevel_; }
    int GetCurrentExp() const { return currentExp_; }
    int GetExpToNextLevel() const { return expToNextLevel_; }
    int GetPendingExpToAdd() const { return pendingExpToAdd_; }
    bool HasPendingLevelUp() const { return pendingLevelUps_ > 0; }
    int GetPendingLevelUpCount() const { return pendingLevelUps_; }

    int GetUpgradeLevel(UpgradeType type) const;
    int GetUpgradeMaxLevel(UpgradeType type) const;
    UpgradeCardInfo GetUpgradeCardInfo(UpgradeType type) const;
    std::vector<UpgradeType> RollUpgradeChoices() const;
    void ApplyUpgradeChoice(UpgradeType type);

    int GetBulletDamage() const { return bulletDamage_; }
    int GetMultiShotCount() const { return multiShotCount_; }
    int GetBulletPierceCount() const { return bulletPierceCount_; }
    const std::vector<UpgradeType>& GetOwnedUpgradeOrder() const { return ownedUpgradeOrder_; }

private:
    /// <summary>
    /// マウス右クリック長押しによる幽霊吸収（Ghost Vacuum）の入力監視・捕捉吸引・3Dボルテックス更新を行います。
    /// </summary>
    void HandleGhostVacuum();
    /// <summary>
    /// マウス入力（左クリックまたは長押し連射）を監視し、射撃間隔や弾数制限を満たしている場合に Fire() を呼び出します。
    /// </summary>
    void HandleShooting();

    /// <summary>
    /// リロードキー（Rキー）の入力を監視し、リロードタイマーを開始します。
    /// </summary>
    void HandleReload();

    /// <summary>
    /// リロードを開始し、メカニカル装填音の再生とリロードタイマーを設定します。
    /// </summary>
    void StartReload();

    /// <summary>
    /// 画面中央のレティクル（マウス照準）へ向けて銃口から弾丸（Bulletプレハブ）を射出し、
    /// 射撃反動のカメラシェイクを誘発します。
    /// </summary>
    void Fire();

private:
    // マジックナンバー排除のための定数
    static constexpr int kDefaultMaxHp = 10;
    static constexpr int kDefaultInvincibilityDuration = 45; // 被弾後の無敵フレーム数 (約0.75秒)
    static constexpr int kDamageFlashCycle = 6;              // 被弾点滅の周期フレーム数
    static constexpr int kDefaultMaxAmmo = 50;       // 装弾数を50発に設定
    static constexpr int kDefaultReloadTime = 75;     // リロード待機（フレーム数: 約1.25秒）
    static constexpr int kDefaultShotInterval = 3;    // 連射間隔（フレーム数）
    static constexpr float kDefaultBulletSpeed = 0.5f;
    static constexpr float kBulletSpawnDistance = 1.0f; // 銃口の前方オフセット距離
    static constexpr float kDefaultMoveSpeed = 0.15f;   // 標準ルート走行速度
    static constexpr float kMinMoveSpeed = 0.01f;       // 最小走行速度
    static constexpr float kMaxMoveSpeed = 2.0f;        // 最大走行速度
    static constexpr float kReloadingMoveSpeed = 0.0f;  // リロード中のレール移動速度（完全停止）
    static constexpr float kShootingMoveSpeedMultiplier = 1.0f / 3.0f; // 射撃中のレール移動速度倍率（通常の3分の1）
    static constexpr float kMinShootingSpeedMultiplier = 0.05f;        // 射撃中速度倍率の最小値
    static constexpr float kMaxShootingSpeedMultiplier = 1.0f;         // 射撃中速度倍率の最大値
    static constexpr int kShootingSlowdownHoldFrames = 6;              // 連射フレーム間で速度がガタつかない減速保持フレーム数
    static constexpr int kShotMouseKey = 0;           // 左クリック
    static constexpr float kShootShakeIntensity = 0.06f; // 射撃時の反動カメラシェイク振幅
    static constexpr float kShootShakeDuration = 0.10f;  // 射撃時の反動カメラシェイク持続時間
    static constexpr int kMinHpLimit = 1;
    static constexpr int kMaxHpLimit = 100;
    static constexpr int kMinAmmoLimit = 1;
    static constexpr int kMaxAmmoLimit = 100;
    static inline const std::string kSeShootPath = "resources/Sounds/shoot.wav";
    static inline const std::string kSeDryFirePath = "resources/Sounds/dryfire.wav";
    static inline const std::string kSeReloadPath = "resources/Sounds/reload.wav";
    static constexpr float kShootVolume = 0.75f;
    static constexpr float kDryFireVolume = 0.65f;
    static constexpr float kReloadVolume = 0.80f;
    static constexpr float kDryFireKickbackZ = -0.015f;
    static constexpr float kDryFireKickPitch = 0.025f;
    static constexpr float kReloadDownOffsetY = -0.14f;
    static constexpr float kReloadTiltPitch = -0.22f;
    static constexpr float kReloadTiltRoll = 0.35f;

    // マガジンモデル定数 (マジックナンバー排除)
    static constexpr Vector3 kMagazineSize = { 0.032f, 0.14f, 0.065f }; // スチールマガジン寸法 (幅, 高さ, 奥行き: m)
    static constexpr Vector4 kMagazineColor = { 0.08f, 0.08f, 0.10f, 1.0f }; // ダークガンメタルスチール
    static constexpr Vector3 kMagRelativeOffset = { 0.0f, -0.09f, -0.03f }; // 銃グリップ底部の基準マガジン位置
    static constexpr float kMagDropDist = 0.55f; // 旧マガジンの落下距離 (m)
    static constexpr float kMagInsertStartDist = 0.32f; // 新マガジンの挿入開始距離 (m)
    static constexpr int kDeathMeltDuration = 100;             // 死亡演出全体フレーム数 (約1.67秒)
    static constexpr int kCameraAlignFrames = 22;             // カメラ正面回り込み待機フレーム数 (約0.36秒)
    static constexpr float kMeltHorizontalSpreadFactor = 2.0f;// 溶解時の横方向液状化拡大率
    static constexpr float kMeltVerticalCollapseFactor = 0.95f;// 溶解時の縦方向圧縮率
    static constexpr float kDeathShakeIntensity = 0.04f;      // 正面カメラ移動中の被弾微震動強度

    // メンバ変数
    int maxHp_ = kDefaultMaxHp;
    int hp_ = kDefaultMaxHp;
    int invincibilityTimer_ = 0;
    Vector4 originalColor_ = { 0.0f, 0.3f, 1.0f, 1.0f };
    bool isOriginalColorSaved_ = false;
    int maxAmmo_ = kDefaultMaxAmmo;
    int ammo_ = kDefaultMaxAmmo;
    bool infiniteAmmo_ = false; // デバッグ用: 残弾無限（デフォルトはfalseで有限30発）
    int reloadDuration_ = kDefaultReloadTime;
    int reloadTimer_ = 0;
    int shotInterval_ = kDefaultShotInterval;
    int shotIntervalTimer_ = 0;
    float bulletSpeed_ = kDefaultBulletSpeed;
    float moveSpeed_ = kDefaultMoveSpeed;
    float shootingMoveSpeedMultiplier_ = kShootingMoveSpeedMultiplier;
    int shootingSlowdownTimer_ = 0;
    bool isShooting_ = false;

    bool isAutoMoving_ = false;
    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    Vector3 currentVelocity_ = { 0.0f, 0.0f, 0.0f }; // 1フレームあたりの実移動速度ベクトル
    Vector3 prevPos_ = { 0.0f, 0.0f, 0.0f };
    bool hasPrevPos_ = false;
    bool enableInertia_ = true; // 弾丸への慣性同期（レティクル完全直進化）フラグ
    bool isDead_ = false;

    // 死亡溶解演出状態
    bool isMelting_ = false;
    int meltTimer_ = 0;
    Vector3 initialScale_ = { 1.0f, 1.0f, 1.0f };
    bool isInitialScaleSaved_ = false;
    Vector3 initialPos_ = { 0.0f, 0.0f, 0.0f };
    bool isInitialPosSaved_ = false;

    // 射撃・空撃ち・リロード効果音 (SE)
    SoundData seShoot_{};
    bool isSeShootLoaded_ = false;
    SoundData seDryFire_{};
    bool isSeDryFireLoaded_ = false;
    SoundData seReload_{};
    bool isSeReloadLoaded_ = false;
    float dryFireShakeProgress_ = 0.0f; // 空撃ち時のピクッ微振動 (0.0〜1.0)

    // 自機本体モデル関連定数 (マジックナンバー排除)
    static constexpr float kPlayerBaseScale = 0.72f; // マップ縮尺に美しく調和する自機基本スケール (m)

    // 手持ちGunモデル関連定数 (マジックナンバー排除)
    static inline const std::string kGunModelKey = "gun";
    static constexpr Vector4 kGunDefaultColor = { 0.12f, 0.12f, 0.14f, 1.0f }; // シックなマットブラック塗装
    static constexpr Vector4 kPlayerBulletColor = { 1.0f, 0.96f, 0.40f, 1.0f }; // VALORANT風ロングレーザー白熱イエロー光
    static constexpr Vector3 kGunDefaultOffset = { 0.22f, -0.22f, 0.42f };      // カメラローカル手前右下（Apex風の構え）
    static constexpr Vector3 kGunDefaultRotation = { 0.0f, 0.0f, -0.04f };      // 手首のロール傾きのみ（Pitch/Yawはレティクル自動追従）
    static constexpr Vector3 kGunDefaultScale = { 0.48f, 0.48f, 0.48f };        // Apex風にしっかり見せる大型スケール
    static constexpr Vector3 kDefaultGunMuzzleOffset = { 0.0f, 0.35f, 1.20f }; // gun.objモデル空間における銃口先端位置（前方に抜けた位置）
    static constexpr Vector3 kGunMuzzleLocalOffset = kDefaultGunMuzzleOffset;
    static constexpr float kGunRecoilKickbackZ = -0.06f;  // 射撃反動での後退量 (m)
    static constexpr float kGunRecoilPitch = -0.10f;     // 射撃反動での銃口跳ね上がり角 (rad)
    static constexpr float kGunRecoilRecovery = 0.20f;    // 反動回復速度 (Lerp係数)

    // 照準補正およびマズルフラッシュ関連定数 (マジックナンバー排除)
    static constexpr float kDefaultZeroInDistance = 15.0f;              // 空撃ち時の自然な標準収束距離 (m)
    static constexpr float kMinAimConvergenceDist = 2.5f;               // 極近距離での過度な角度折れを防ぐ最小収束距離 (m)
    static constexpr float kMaxAimRaycastDist = 80.0f;                  // レティクル中央視線レイの最大計測距離 (m)
    static constexpr float kAimRayBuildingStep = 1.0f;                  // 建物との距離計測レイマーチ刻み幅 (m)
    static constexpr float kAimRayEnemyPadding = 0.25f;                 // 敵輪郭付近でも敵距離へスムーズに収束するAABB拡張マージン (m)
    static constexpr Vector2 kDefaultAimCorrection = { 0.0f, 0.0f };   // レティクル視線と完全一致（視差バイアス不要）
    static constexpr float kBulletCameraForwardOffset = 0.5f;           // レティクル視線発射時のカメラ前方スポーン距離 (m)
    static constexpr Vector3 kDefaultBulletVisualScale = { 0.055f, 0.055f, 5.2f }; // VALORANT風ロングレーザートレーサー（太さ5.5cm, 長さ5.2m）
    static constexpr float kDefaultBulletCollisionRadius = 0.30f;       // 自機弾の快適な当たり判定半径 (m)
    static constexpr int kMuzzleFlashDuration = 1;                      // 火炎点灯フレーム数 (瞬間スナップ点灯 約0.016秒で視界クリア)
    static constexpr float kDefaultMuzzleFlameLength = 0.07f;           // 前方ジェット火炎の全長 (m) (銃口先端にコンパクトに集中)
    static constexpr float kDefaultMuzzleFlameRadius = 0.016f;          // 火炎コーンの底面半径 (m)
    static constexpr float kDefaultMuzzleCoreRatio = 0.60f;             // 高温インナーコアのスケール比率
    static constexpr float kDefaultMuzzleFlashForwardOffset = 0.005f;   // 銃口の真先に密着させる前方オフセット (m)
    static constexpr Vector4 kDefaultFlameOuterColor = { 1.0f, 0.46f, 0.04f, 0.95f }; // 鮮烈な炎オレンジ
    static constexpr Vector4 kDefaultFlameCoreColor  = { 1.0f, 0.96f, 0.45f, 1.0f };  // 超高輝度ホワイトイエロー高温核
    static constexpr Vector4 kDefaultCoreDiscColor   = { 1.0f, 1.0f, 0.90f, 1.0f };   // 銃口リム白熱発光
    static constexpr Vector4 kDefaultSmokeStartColor = { 0.35f, 0.20f, 0.12f, 0.35f }; // 半透明度を高めて標的の視認性を向上
    static constexpr Vector4 kDefaultSmokeEndColor   = { 0.12f, 0.12f, 0.14f, 0.00f }; // 冷えたチャコール黒煙 (完全透明へ)
    static constexpr Vector4 kDefaultBulletSparkStart = { 1.0f, 0.52f, 0.05f, 1.0f }; // 鮮烈ブライトオレンジ火花
    static constexpr Vector4 kDefaultBulletSparkEnd   = { 0.85f, 0.18f, 0.02f, 0.00f }; // ディープバーントオレンジ（冷却消滅）
    static constexpr uint32_t kDefaultMuzzleFlashGpuParticleCount = 6;  // 射撃時の専用GPU硝煙ガス粒子数 (大幅スリム化)
    static constexpr uint32_t kDefaultMuzzleMaxParticles = 512;         // 銃口専用GPUパーティクルバッファ数
    static constexpr float kDefaultMuzzleSmokeForwardOffset = 0.010f;   // 銃口直近の先端オフセット (m)
    static constexpr float kDefaultMuzzleSmokeSpeed = 1.2f;             // 前方硝煙ガス噴出初速 (m/s) (拡散抑制)
    static constexpr float kDefaultMuzzleSmokeSpread = 0.12f;           // 横拡散を極小化 (m/s)
    static constexpr float kDefaultMuzzleSmokeLifeMin = 0.04f;          // 最小煙寿命 (秒) (約2フレーム)
    static constexpr float kDefaultMuzzleSmokeLifeMax = 0.08f;          // 最大煙寿命 (秒) (約5フレームで即座に消滅)
    static constexpr float kDefaultMuzzleSmokeScaleStart = 0.006f;      // 発生時サイズ (超小型)
    static constexpr float kDefaultMuzzleSmokeScaleEnd = 0.018f;        // 拡散時サイズ (コンパクトに収束)

    // Gun 手持ちモデルおよびTransform設定
    std::unique_ptr<ModelObject> gunModel_;
    std::unique_ptr<CubeObject> magazineModel_; // リロード時の3Dマガジンオブジェクト
    Vector3 gunOffset_ = kGunDefaultOffset;
    Vector3 gunRotation_ = kGunDefaultRotation;
    Vector3 gunScale_ = kGunDefaultScale;
    Vector4 gunColor_ = kGunDefaultColor;
    Vector3 gunMuzzleOffset_ = kDefaultGunMuzzleOffset;
    float gunRecoilProgress_ = 0.0f; // 0.0 (通常) 〜 1.0 (反動最大)

    bool isGunVisible_ = true;      // 銃モデル描画フラグ

    // 照準補正パラメータ
    Vector2 aimCorrection_ = kDefaultAimCorrection;
    float zeroInDistance_ = kDefaultZeroInDistance;

    // リアル・ジェット火炎（3D円錐多層構造）＆ 硝煙ガスGPUパーティクル
    std::unique_ptr<ConeObject> muzzleFlameOuter_;     // 前方アウター火炎コーン（オレンジ）
    std::unique_ptr<ConeObject> muzzleFlameCore_;      // 前方インナー高温コア（白黄）
    std::unique_ptr<FlatRingObject> muzzleCoreDisc_;   // 銃口白熱発光ディスク
    std::unique_ptr<GPUParticleObject> muzzleGpuParticle_; // 銃口専用GPU硝煙ガスエミッター

    float muzzleFlameLength_ = kDefaultMuzzleFlameLength;
    float muzzleFlameRadius_ = kDefaultMuzzleFlameRadius;
    float muzzleCoreRatio_ = kDefaultMuzzleCoreRatio;
    float muzzleFlashForwardOffset_ = kDefaultMuzzleFlashForwardOffset;
    float muzzleSmokeForwardOffset_ = kDefaultMuzzleSmokeForwardOffset;
    float muzzleSmokeSpeed_ = kDefaultMuzzleSmokeSpeed;
    float muzzleSmokeSpread_ = kDefaultMuzzleSmokeSpread;
    float muzzleSmokeScaleStart_ = kDefaultMuzzleSmokeScaleStart;
    float muzzleSmokeScaleEnd_ = kDefaultMuzzleSmokeScaleEnd;
    float muzzleSmokeLifeMax_ = kDefaultMuzzleSmokeLifeMax;
    int muzzleFlashTimer_ = 0;
    float muzzleFlashRotation_ = 0.0f;
    uint32_t muzzleFlashGpuCount_ = kDefaultMuzzleFlashGpuParticleCount;

    // 統合カラーパレット（実機調色可能）
    Vector4 flameOuterColor_ = kDefaultFlameOuterColor;
    Vector4 flameCoreColor_ = kDefaultFlameCoreColor;
    Vector4 coreDiscColor_ = kDefaultCoreDiscColor;
    Vector4 smokeStartColor_ = kDefaultSmokeStartColor;
    Vector4 smokeEndColor_ = kDefaultSmokeEndColor;
    Vector4 bulletSparkStartColor_ = kDefaultBulletSparkStart;
    Vector4 bulletSparkEndColor_ = kDefaultBulletSparkEnd;

    // 自機弾パラメータ
    Vector3 bulletVisualScale_ = kDefaultBulletVisualScale;
    float bulletCollisionRadius_ = kDefaultBulletCollisionRadius;

    // 被弾ダメージビネット（画面外周の赤い靄）＆ 瞬間赤フラッシュ定数 (マジックナンバー排除)
    static constexpr float kDamageVignetteInitialAlpha = 0.85f; // 被弾瞬間の靄の最大濃度
    static constexpr float kDamageVignetteFadeSpeed = 0.022f;   // 靄の減衰速度（約38フレーム＝約0.63秒）
    static constexpr float kDamageFlashInitialAlpha = 0.18f;    // 被弾瞬間の全画面赤フラッシュ濃度
    static constexpr float kDamageFlashFadeSpeed = 0.015f;      // フラッシュの減衰速度
    static constexpr Vector4 kDamageVignetteColor = { 1.0f, 0.2f, 0.2f, 1.0f }; // 鮮烈な血煙レッド
    static constexpr Vector4 kDamageFlashColor = { 1.0f, 0.1f, 0.1f, 1.0f };    // 被弾フラッシュレッド

    std::unique_ptr<SpriteObject> damageVignetteSprite_; // 画面外周の赤い靄スプライト
    std::unique_ptr<SpriteObject> damageFlashSprite_;    // 画面全体の瞬間赤フラッシュスプライト
    float damageVignetteAlpha_ = 0.0f;                  // 現在のダメージビネット不透明度
    float damageFlashAlpha_ = 0.0f;                     // 現在の赤フラッシュ不透明度

    void EnsureDamageVisuals();                         // 被弾演出用スプライトの初期化
    void EnsureVacuumVisuals();                         // 幽霊吸引（Ghost Vacuum）用3D/2Dオブジェクト初期化
    void EnsureGunModel();
    void UpdateGunTransform();
    void UpdateDeathMeltAnimation();

    // Step 1-5: 右クリック長押しによる幽霊吸収（Ghost Vacuum）定数＆メンバ変数 (マジックナンバー排除)
    static constexpr int kVacuumMouseKey = 1;                   // マウス右クリック
    static constexpr float kVacuumMaxRange = 18.0f;             // 吸引ビームの最大有効射程 (m)
    static constexpr float kVacuumNearAutoCatchDist = 7.5f;     // カメラ前方付近に取り憑いている全幽霊を漏れなく捕捉する距離 (m)
    static constexpr float kVacuumConeCosThreshold = 0.55f;     // 前方照準コーン角度閾値 (cos ≈ 56度)
    static constexpr float kVacuumChargePerFrame = 0.030f;      // 1フレームあたり吸引ゲージ蓄積量 (約0.55秒ホールドで成仏)
    static constexpr int kVacuumVortexRingCount = 3;            // 銃口前方に展開する3D吸引ボルテックスリング数
    static constexpr uint32_t kVacuumRingDivideCount = 24;      // 3D吸引リングの分割数
    static constexpr float kVacuumRingBaseOuterR = 1.0f;        // 3D吸引リングの基準外径 (m)
    static constexpr float kVacuumRingBaseInnerR = 0.82f;       // 3D吸引リングの基準内径 (m)
    static constexpr float kVacuumVortexMaxDist = 0.90f;        // 3D吸引リングの出現前方距離 (m)
    static constexpr float kVacuumVortexMinDist = 0.04f;        // 3D吸引リングの銃口収束距離 (m)
    static constexpr float kVacuumVortexMaxScale = 0.24f;       // 3D吸引リングの出現時半径スケール (m)
    static constexpr float kVacuumVortexMinScale = 0.025f;      // 3D吸引リングの銃口収束時スケール (m)
    static constexpr float kVacuumAnimSpeed = 0.045f;           // 吸引ボルテックスの収束サイクル速度 (/frame)
    static constexpr float kVacuumReticleBaseRadius = 42.0f;    // 2Dレティクル外周サクションリング基準半径 (px)
    static constexpr float kVacuumReticlePulseAmp = 10.0f;      // 2Dサクションリングの収縮パルス幅 (px)
    static constexpr float kVacuumReticleThickness = 2.8f;      // 2Dサクションリングの線の太さ (px)
    static constexpr float kExorciseShakeIntensity = 0.08f;     // 幽霊成仏瞬間の爽快カメラシェイク振幅
    static constexpr float kExorciseShakeDuration = 0.14f;      // 幽霊成仏瞬間のカメラシェイク時間 (秒)
    static constexpr float kExorciseRecoilBoost = 0.55f;        // 幽霊成仏瞬間の銃キックバック反動量
    static constexpr int kAbsorbComboWindowFrames = 90;         // 複数幽霊の連続吸収を合算ポップアップする猶予フレーム数 (約1.5秒)

    bool isVacuuming_ = false;                                  // 右クリック吸引ホールド中フラグ
    bool isCapturingGhost_ = false;                             // 現在1体以上の幽霊を吸引捕捉中か
    float vacuumAnimPhase_ = 0.0f;                              // 吸引ボルテックスアニメーション位相 (0.0〜1.0)
    int absorbedGhostCount_ = 0;                                // 累計成仏・吸収霊魂カウント
    int absorbComboCount_ = 0;                                  // 連続吸収コンボ体数（ポップアップ合算表示用）
    int absorbComboExp_ = 0;                                    // 連続吸収コンボ合計EXP（ポップアップ合算表示用）
    int absorbComboTimer_ = 0;                                  // 連続吸収コンボ維持タイマー
    std::unique_ptr<FlatRingObject> vacuumVortexRings_[kVacuumVortexRingCount]; // 銃口3D吸引ボルテックスリング
    std::unique_ptr<Ring2DObject> vacuumReticleRing_;           // 2Dレティクル外周サクションリング

    // Step 1-6: 経験値（EXP）・レベルアップ・全8種アップグレード定数＆メンバ変数 (マジックナンバー排除)
    static constexpr int kInitialPlayerLevel = 1;               // 初期プレイヤーレベル
    static constexpr int kInitialExpToNextLevel = 100;          // Lv1->2 必要経験値 (以降レベルアップごとに2倍)
    static constexpr int kExpNextLevelMultiplier = 2;           // レベルアップごとの必要経験値倍率 (100 -> 200 -> 400 -> 800...)
    static constexpr int kMaxExpToNextLevel = 1000000;          // 必要経験値の安全上限キャップ
    static constexpr int kExpTickAddPerFrame = 1;               // 1フレームあたりにゲージへ加算するEXP量 (1ずつ40まで加算)
    static constexpr int kExpPerGhostAbsorb = 40;               // 幽霊1体吸収時の獲得EXP
    static constexpr int kDefaultBulletDamage = 1;              // 初期弾丸威力
    static constexpr int kDefaultMultiShotCount = 1;            // 初期同時発射数
    static constexpr int kDefaultPierceCount = 0;               // 初期敵貫通数
    static constexpr float kMultiShotSpreadRad = 0.045f;        // マルチショット1発あたりの水平拡散角 (rad ≈ 2.6度)
    static constexpr float kHighPowerBeamThicknessStep = 0.14f; // 攻撃力Lvごとのレーザー太さ強調率
    static constexpr int kExtendedMagBonusAmmo = 20;            // 拡張マガジン1Lvあたりの最大装弾数加算
    static constexpr float kQuickReloadTimeFactor = 0.78f;      // クイックリロード1Lvあたりのリロード時間倍率 (-22%)
    static constexpr int kMinReloadDurationFrames = 21;         // リロード時間の下限フレーム数 (約0.35秒)
    static constexpr int kMinShotIntervalFrames = 2;            // 連射間隔の下限フレーム数
    static constexpr float kRapidFireBulletSpeedBoost = 1.15f;  // ラピッドファイア1Lvあたりの弾速上昇倍率
    static constexpr int kArmorRepairMaxHpBonus = 3;            // アーマー＆リペア1Lvあたりの最大HP加算
    static constexpr int kArmorRepairHealAmount = 5;            // アーマー＆リペア取得時のHP回復量
    static constexpr float kVacuumSpeedBoostStep = 0.45f;       // ソウルバキューム改1Lvあたりの吸引速度加算 (+45%)
    static constexpr float kVacuumRangeBoostStep = 3.0f;        // ソウルバキューム改1Lvあたりの射程延長 (m)
    static constexpr float kVacuumConeCosReductionStep = 0.06f; // ソウルバキューム改1Lvあたりのコーン角拡大
    static constexpr float kMinVacuumConeCos = 0.50f;           // 吸引コーン角コサイン下限 (最大60度コーン)

    static constexpr int kMaxLvMultiShot = 3;
    static constexpr int kMaxLvPiercing = 3;
    static constexpr int kMaxLvHighPower = 5;
    static constexpr int kMaxLvRapidFire = 3;
    static constexpr int kMaxLvExtendedMag = 5;
    static constexpr int kMaxLvQuickReload = 4;
    static constexpr int kMaxLvArmorRepair = 5;
    static constexpr int kMaxLvSoulVacuum = 3;

    int playerLevel_ = kInitialPlayerLevel;
    int currentExp_ = 0;
    int expToNextLevel_ = kInitialExpToNextLevel;
    int pendingExpToAdd_ = 0;
    int pendingLevelUps_ = 0;
    void ProcessExpTick();
    int bulletDamage_ = kDefaultBulletDamage;
    int multiShotCount_ = kDefaultMultiShotCount;
    int bulletPierceCount_ = kDefaultPierceCount;
    float vacuumSpeedMultiplier_ = 1.0f;
    float vacuumRangeBonus_ = 0.0f;
    float vacuumConeCosBonus_ = 0.0f;
    std::array<int, kUpgradeTypeCount> upgradeLevels_{};
    std::vector<UpgradeType> ownedUpgradeOrder_;
};
