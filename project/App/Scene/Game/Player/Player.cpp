#include "App/Scene/Game/Player/Player.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Base/WindowApp/WindowApp.h"

Player::Player() {
    cube_ = std::make_unique<CubeObject>();
    weaponCube_ = std::make_unique<CubeObject>();
    bulletEffectName_ = kBulletEffectName;
}

void Player::Initialize() {
    cube_->Initialize();
    cube_->SetPosition(kInitialPosition);
    cube_->SetSize(kPlayerScale);
    cube_->SetColor(kPlayerColor); // プレイヤーを青くする

    weaponCube_->Initialize();
    weaponCube_->SetSize(kWeaponScale);
    weaponCube_->SetColor(kWeaponColor);

    ClearBullets();
    InitializeHpBar(); // 体力と体力バーの初期化
    isAutoMoving_ = false; // 自動走行フラグをリセット

    ammo_ = kMaxAmmo;
    reloadTimer_ = 0;
    shotIntervalTimer_ = 0;
}

void Player::Update() {
    hasFiredThisFrame_ = false;
    Input* input = InputResource::GetInput();
    UpdateInput(input);
}

void Player::UpdateInput(Input* input) {
    if (!input) {
        return;
    }

    Vector3 pos = cube_->GetPosition();

    cube_->Update();

    // 連射間隔タイマーの更新
    if (shotIntervalTimer_ > 0) {
        shotIntervalTimer_--;
    }

    // リロードタイマーの更新
    if (reloadTimer_ > 0) {
        reloadTimer_--;
        if (reloadTimer_ == 0) {
            ammo_ = kMaxAmmo;
        }
    }

    // リロード入力 (Rキー)
    if (reloadTimer_ <= 0 && ammo_ < kMaxAmmo) {
        if (input->Trigger(kReloadKey)) {
            reloadTimer_ = kReloadTime;
        }
    }

    // 弾の発射処理 (マウス左クリック)
    // ※リロード中でなく、連射間隔タイマーが終了しており、弾数が残っている場合のみ射撃可能
    if (reloadTimer_ <= 0 && shotIntervalTimer_ <= 0 && ammo_ > 0) {
        if (input->MouseTrigger(kShotMouseKey)) {
            // 弾数を消費
            ammo_--;
            shotIntervalTimer_ = kShotInterval;
            hasFiredThisFrame_ = true;

            // 武器先端（銃口）から発射する (演出用)
            static const float kMuzzleOffsetZ = 0.8f;
            Vector3 muzzlePos = Math::Add(weaponPosition_, Math::Multiply(kMuzzleOffsetZ, direction_));

            // マウスカーソル位置へのレイをカメラから計算して弾を誘導する
            auto camera = CameraResource::GetCameraManager()->GetActiveCamera();
            float clientW = static_cast<float>(WindowApp::kClientWidth);
            float clientH = static_cast<float>(WindowApp::kClientHeight);
            Vector2 viewSize = GameViewWindow::GetGameViewSize();
            Vector2 mousePos = GameViewWindow::GetMousePosition();

            // 1280x720の固定解像度スケールに変換する
            Vector2 scaledMousePos = mousePos;
            if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
                scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
                scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
            }

            Vector3 rayStart = cameraPosition_;
            Vector3 rayDir = direction_;
            if (camera) {
                camera->CreateRay(scaledMousePos, clientW, clientH, rayStart, rayDir);
            }

            // レイの方向に十分進んだ目標地点
            static const float kTargetDistance = 50.0f;
            Vector3 targetPos = Math::Add(rayStart, Math::Multiply(kTargetDistance, rayDir));

            // 銃口から照準先ターゲットへの方向ベクトル
            Vector3 bulletDir = Math::Normalize(Math::Subtract(targetPos, muzzlePos));
            Vector3 bulletVel = Math::Multiply(kBulletSpeed, bulletDir);

            bullets_.push_back(std::make_unique<Bullet>(muzzlePos, bulletVel, kBulletEffectName));
        }
    }

    // 弾の更新
    UpdateBullets();

    // 体力バーの追従更新
    UpdateHpBar(pos);
}

void Player::Draw() {
    // プレイヤー本体の描画
    if (cube_) {
        cube_->Draw(kTextureKey, kEnvMapKey);
    }
    
    // 武器モデルの描画
    if (weaponCube_) {
        weaponCube_->Draw(kTextureKey, kEnvMapKey);
    }
    
    DrawBullets();
    DrawHpBar(); // 体力バーの描画
}

void Player::UpdateWeapon(const Matrix4x4& viewMatrix) {
    if (!weaponCube_) {
        return;
    }

    // カメラのビュー行列の逆行列（カメラのワールド行列）を求める
    Matrix4x4 camWorld = Math::Inverse(viewMatrix);

    // カメラの右・上・前ベクトルとカメラ位置を抽出
    Vector3 right   = { camWorld.m[0][0], camWorld.m[0][1], camWorld.m[0][2] };
    Vector3 up      = { camWorld.m[1][0], camWorld.m[1][1], camWorld.m[1][2] };
    Vector3 forward = { camWorld.m[2][0], camWorld.m[2][1], camWorld.m[2][2] };
    Vector3 camPos  = { camWorld.m[3][0], camWorld.m[3][1], camWorld.m[3][2] };

    // メンバ変数に保存
    cameraPosition_ = camPos;
    direction_ = Math::Normalize(forward); // 向きベクトルを正規化

    // 武器のワールド座標を計算 (カメラ位置からローカルオフセット分移動)
    Vector3 rightOffset = Math::Multiply(kWeaponLocalOffset.x, right);
    Vector3 upOffset    = Math::Multiply(kWeaponLocalOffset.y, up);
    Vector3 forwardOffset = Math::Multiply(kWeaponLocalOffset.z, forward);
    
    Vector3 weaponPos = Math::Add(camPos, Math::Add(rightOffset, Math::Add(upOffset, forwardOffset)));
    weaponPosition_ = weaponPos;

    // 前方向ベクトルからピッチとヨーを逆算して、武器の回転角度を計算する
    float yaw = std::atan2(forward.x, forward.z);
    float pitch = -std::asin(forward.y);

    // 武器の回転を設定 (銃口を画面中央に寄せるヨーオフセットを適用)
    Vector3 weaponRot = { pitch, yaw + kWeaponYawOffset, 0.0f };

    weaponCube_->SetPosition(weaponPos);
    weaponCube_->SetRotate(weaponRot);
    weaponCube_->Update();
}
