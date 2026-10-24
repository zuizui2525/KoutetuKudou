#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Audio/AudioStructs.h"
#include <string>

/// <summary>
/// 自機キャラクターのHP、弾数管理、射撃、リロードを制御するスクリプトコンポーネント
/// </summary>
class PlayerControllerScript : public ScriptComponent {
public:
    PlayerControllerScript();
    ~PlayerControllerScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
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

    // メンバ変数
    int maxHp_ = kDefaultMaxHp;
    int hp_ = kDefaultMaxHp;
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

    // 射撃効果音 (SE)
    SoundData seShoot_{};
    bool isSeShootLoaded_ = false;
};
