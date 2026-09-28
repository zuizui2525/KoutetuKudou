#pragma once
#include "App/Scene/Game/Player/Shooter.h"
#include "Engine/Input/Input.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"

/**
 * @brief プレイヤーキャラクタークラス
 */
class Player : public Shooter {
public:
    Player();
    ~Player() override = default;

    void Initialize() override;
    void Update() override; // 継承元に合わせるため引数なし、Update内でInputResource等から取得
    void UpdateInput(Input* input); // インプットを明示的に受ける更新メソッド
    void UpdateWeapon(const Matrix4x4& viewMatrix); // 武器位置・角度更新
    void Draw() override;

    // 位置・回転の操作
    void SetPosition(const Vector3& pos) { cube_->SetPosition(pos); }
    void SetRotation(const Vector3& rot) { cube_->SetRotate(rot); }
    const Vector3& GetRotation() const { return cube_->GetRotate(); }

    // 自動走行ステートの操作
    void SetAutoMoving(bool autoMove) { isAutoMoving_ = autoMove; }
    bool IsAutoMoving() const { return isAutoMoving_; }

    // 走行方向ベクトルの操作
    void SetDirection(const Vector3& dir) { direction_ = dir; }
    const Vector3& GetDirection() const { return direction_; }

    // 衝突判定用インターフェース実装
    Vector3 GetPosition() const override { return cube_->GetPosition(); }
    Vector3 GetSize() const override { return cube_->GetSize(); }

    // 弾数・リロード情報の取得
    int GetAmmo() const { return ammo_; }
    bool IsReloading() const { return reloadTimer_ > 0; }
    int GetReloadTimer() const { return reloadTimer_; }
    static int GetMaxAmmo() { return kMaxAmmo; }
    static int GetReloadTime() { return kReloadTime; }
    static float GetAutoSpeed() { return kSpeed; }
    bool HasFiredThisFrame() const { return hasFiredThisFrame_; }

private:
    bool isAutoMoving_ = false; // 自動走行中かどうかのフラグ
    Vector3 direction_ = { 0.0f, 0.0f, 1.0f }; // プレイヤーの現在の進行方向（正面）

    // 弾数・射撃制御用の変数
    int ammo_ = kMaxAmmo;
    int reloadTimer_ = 0;
    int shotIntervalTimer_ = 0;
    bool hasFiredThisFrame_ = false;

private:
    // マジックナンバー排除のための定数
    static inline const Vector3 kInitialPosition = { 0.0f, 0.0f, -6.0f };   // 初期位置
    static inline const Vector3 kPlayerScale = { 1.0f, 1.0f, 1.0f };        // サイズ
    static inline const Vector3 kBulletVelocity = { 0.0f, 0.0f, 0.25f };    // 弾の初速度
    static inline const float kSpeed = 0.05f;                                 // 移動速度（自動走行速度）
    static inline const float kMoveLimitX = 8.0f;                           // 左右移動限界
    static inline const std::string kTextureKey = "white";                  // テクスチャ
    static inline const std::string kEnvMapKey = "";                        // 環境マップ
    static inline const int kReloadKey = DIK_R;                             // リロードキー
    static inline const int kShotMouseKey = 0;                              // 射撃（左クリック）キー
    static inline const Vector4 kPlayerColor = { 0.0f, 0.3f, 1.0f, 1.0f };  // プレイヤーカラー（青）
    static inline const std::string kBulletEffectName = "YellowFire";       // 弾エフェクト名（黄色い炎）
    static inline const Vector3 kWeaponScale = { 0.18f, 0.12f, 1.2f };      // 武器のサイズ
    static inline const Vector4 kWeaponColor = { 0.2f, 0.2f, 0.2f, 1.0f };  // 武器の色
    static inline const Vector3 kWeaponLocalOffset = { 0.35f, -0.45f, 1.5f }; // カメラ基準の武器ローカルオフセット
    static inline const float kWeaponYawOffset = -0.08f;                     // 銃身を中央に向けるヨーオフセット
    static inline const float kBulletSpeed = 0.5f;                          // 弾の速度

    static constexpr int kMaxAmmo = 30;                                      // 最大装填数
    static constexpr int kReloadTime = 20;                                  // リロード時間 (0.33秒)
    static constexpr int kShotInterval = 3;                                // 連射間隔 (0.05秒)

private:
    std::unique_ptr<CubeObject> cube_;
    std::unique_ptr<CubeObject> weaponCube_; // 一人称表示用武器モデル
    Vector3 cameraPosition_ = { 0.0f, 0.0f, 0.0f }; // 最新のカメラ位置
    Vector3 weaponPosition_ = { 0.0f, 0.0f, 0.0f }; // 最新の武器位置
};
