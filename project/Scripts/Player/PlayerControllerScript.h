#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Audio/AudioStructs.h"
#include <string>
#include <memory>

class ModelObject;
class FlatRingObject;

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
    /// 自機のルート走行速度を設定します。
    /// </summary>
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    /// <summary>
    /// 現在の銃口（マズル先端）の3Dワールド座標を取得します。
    /// 銃モデルのワールドアフィン変換行列（カメラ追従・構えオフセット・リコイル）に基づいて計算されます。
    /// </summary>
    Vector3 GetMuzzleWorldPosition() const;

    /// <summary>
    /// 現在のマウスカーソル（レティクル）が指す3D空間上の着弾目標地点を取得します。
    /// </summary>
    Vector3 GetAimTargetPosition() const;

private:
    /// <summary>
    /// マウス入力（左クリックまたは長押し連射）を監視し、射撃間隔や弾数制限を満たしている場合に Fire() を呼び出します。
    /// </summary>
    void HandleShooting();

    /// <summary>
    /// リロードキー（Rキー）の入力を監視し、リロードタイマーを開始します。
    /// </summary>
    void HandleReload();

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
    static constexpr int kDefaultMaxAmmo = 30;
    static constexpr int kDefaultReloadTime = 20;     // リロード待機（フレーム数）
    static constexpr int kDefaultShotInterval = 3;    // 連射間隔（フレーム数）
    static constexpr float kDefaultBulletSpeed = 0.5f;
    static constexpr float kBulletSpawnDistance = 1.0f; // 銃口の前方オフセット距離
    static constexpr float kDefaultMoveSpeed = 0.15f;   // 標準ルート走行速度
    static constexpr float kMinMoveSpeed = 0.01f;       // 最小走行速度
    static constexpr float kMaxMoveSpeed = 2.0f;        // 最大走行速度
    static constexpr int kShotMouseKey = 0;           // 左クリック
    static constexpr float kShootShakeIntensity = 0.06f; // 射撃時の反動カメラシェイク振幅
    static constexpr float kShootShakeDuration = 0.10f;  // 射撃時の反動カメラシェイク持続時間
    static constexpr int kMinHpLimit = 1;
    static constexpr int kMaxHpLimit = 100;
    static constexpr int kMinAmmoLimit = 1;
    static constexpr int kMaxAmmoLimit = 100;
    static inline const std::string kSeShootPath = "resources/Sounds/shoot.wav";
    static constexpr float kShootVolume = 0.75f;
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
    bool infiniteAmmo_ = true; // デバッグ用: 残弾無限（弾切れによる不発を防止）
    int reloadDuration_ = kDefaultReloadTime;
    int reloadTimer_ = 0;
    int shotInterval_ = kDefaultShotInterval;
    int shotIntervalTimer_ = 0;
    float bulletSpeed_ = kDefaultBulletSpeed;
    float moveSpeed_ = kDefaultMoveSpeed;

    bool isAutoMoving_ = false;
    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    bool isDead_ = false;

    // 死亡溶解演出状態
    bool isMelting_ = false;
    int meltTimer_ = 0;
    Vector3 initialScale_ = { 1.0f, 1.0f, 1.0f };
    bool isInitialScaleSaved_ = false;
    Vector3 initialPos_ = { 0.0f, 0.0f, 0.0f };
    bool isInitialPosSaved_ = false;

    // 射撃効果音 (SE)
    SoundData seShoot_{};
    bool isSeShootLoaded_ = false;

    // 手持ちGunモデル関連定数 (マジックナンバー排除)
    static inline const std::string kGunModelKey = "gun";
    static constexpr Vector4 kGunDefaultColor = { 0.12f, 0.12f, 0.14f, 1.0f }; // シックなマットブラック塗装
    static constexpr Vector4 kPlayerBulletColor = { 0.08f, 0.08f, 0.10f, 1.0f }; // 漆黒ガンメタル
    static constexpr Vector3 kGunDefaultOffset = { 0.22f, -0.22f, 0.42f };      // カメラローカル手前右下（Apex風の構え）
    static constexpr Vector3 kGunDefaultRotation = { 0.0f, 0.0f, -0.04f };      // 手首のロール傾きのみ（Pitch/Yawはレティクル自動追従）
    static constexpr Vector3 kGunDefaultScale = { 0.48f, 0.48f, 0.48f };        // Apex風にしっかり見せる大型スケール
    static constexpr Vector3 kDefaultGunMuzzleOffset = { 0.0f, 0.35f, 1.20f }; // gun.objモデル空間における銃口先端位置（前方に抜けた位置）
    static constexpr Vector3 kGunMuzzleLocalOffset = kDefaultGunMuzzleOffset;
    static constexpr float kGunRecoilKickbackZ = -0.06f;  // 射撃反動での後退量 (m)
    static constexpr float kGunRecoilPitch = -0.10f;     // 射撃反動での銃口跳ね上がり角 (rad)
    static constexpr float kGunRecoilRecovery = 0.20f;    // 反動回復速度 (Lerp係数)

    // 照準補正およびマズルフラッシュ関連定数 (マジックナンバー排除)
    static constexpr float kDefaultZeroInDistance = 22.0f;              // 交戦レンジに合わせたゼロイン距離 (m)
    static constexpr Vector2 kDefaultAimCorrection = { -0.18f, 0.16f }; // 視差を解消する左(X)・上(Y)の照準バイアス補正
    static constexpr int kMuzzleFlashDuration = 3;                      // フラッシュ点灯フレーム数 (約0.05秒の瞬間閃光)
    static constexpr float kDefaultMuzzleFlashScale = 0.035f;           // 銃口口径(約3.4cm)に合わせた小型基準半径 (m)
    static constexpr float kDefaultMuzzleFlashForwardOffset = 0.04f;    // 銃口の真先に突き出す前方オフセット (m)
    static constexpr float kMuzzleFlashBaseScale = kDefaultMuzzleFlashScale;
    static constexpr Vector4 kMuzzleFlashColor = { 1.0f, 0.90f, 0.40f, 1.0f }; // 発光オレンジイエロー

    // Gun 手持ちモデルおよびTransform設定
    std::unique_ptr<ModelObject> gunModel_;
    Vector3 gunOffset_ = kGunDefaultOffset;
    Vector3 gunRotation_ = kGunDefaultRotation;
    Vector3 gunScale_ = kGunDefaultScale;
    Vector4 gunColor_ = kGunDefaultColor;
    Vector3 gunMuzzleOffset_ = kDefaultGunMuzzleOffset;
    float gunRecoilProgress_ = 0.0f; // 0.0 (通常) 〜 1.0 (反動最大)

    // 照準補正パラメータ
    Vector2 aimCorrection_ = kDefaultAimCorrection;
    float zeroInDistance_ = kDefaultZeroInDistance;

    // マズルフラッシュ発光オブジェクト
    std::unique_ptr<FlatRingObject> muzzleFlashDisc_;
    float muzzleFlashScale_ = kDefaultMuzzleFlashScale;
    float muzzleFlashForwardOffset_ = kDefaultMuzzleFlashForwardOffset;
    int muzzleFlashTimer_ = 0;
    float muzzleFlashRotation_ = 0.0f;

    void EnsureGunModel();
    void UpdateGunTransform();
    void UpdateDeathMeltAnimation();
};
