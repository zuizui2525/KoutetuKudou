#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

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

    // 体力・被弾処理
    void TakeDamage(int amount);
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    bool IsDead() const { return isDead_; }

    // 弾数・リロード情報
    int GetAmmo() const { return ammo_; }
    int GetMaxAmmo() const { return maxAmmo_; }
    bool IsReloading() const { return reloadTimer_ > 0; }
    int GetReloadTimer() const { return reloadTimer_; }

    // 進行方向
    void SetDirection(const Vector3& dir) { direction_ = dir; }
    const Vector3& GetDirection() const { return direction_; }

    // 走行中フラグ
    void SetAutoMoving(bool autoMove) { isAutoMoving_ = autoMove; }
    bool IsAutoMoving() const { return isAutoMoving_; }

private:
    void HandleShooting();
    void HandleReload();
    void Fire();

private:
    // マジックナンバー排除のための定数
    static constexpr int kDefaultMaxHp = 10;
    static constexpr int kDefaultMaxAmmo = 30;
    static constexpr int kDefaultReloadTime = 20;     // リロード待機（フレーム数）
    static constexpr int kDefaultShotInterval = 3;    // 連射間隔（フレーム数）
    static constexpr float kDefaultBulletSpeed = 0.5f;
    static constexpr float kBulletSpawnDistance = 1.0f; // 銃口の前方オフセット距離
    static constexpr int kShotMouseKey = 0;           // 左クリック
    static constexpr float kShootShakeIntensity = 0.06f; // 射撃時の反動カメラシェイク振幅
    static constexpr float kShootShakeDuration = 0.10f;  // 射撃時の反動カメラシェイク持続時間
    static constexpr int kMinHpLimit = 1;
    static constexpr int kMaxHpLimit = 100;
    static constexpr int kMinAmmoLimit = 1;
    static constexpr int kMaxAmmoLimit = 100;

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

    bool isAutoMoving_ = false;
    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    bool isDead_ = false;
};
