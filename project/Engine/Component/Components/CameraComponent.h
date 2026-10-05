#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <memory>
#include <string>

class BaseCamera;

/// <summary>
/// Unityライクなカメラコンポーネント
/// 所属するGameObjectのTransform（位置・回転）と連動し、ビュー・プロジェクション行列を管理する
/// </summary>
class CameraComponent : public IComponent {
public:
    CameraComponent();
    ~CameraComponent() override;

    void Initialize() override;
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "Camera"; }

    // ==========================================
    // パラメータ操作
    // ==========================================
    float GetFov() const { return fov_; }
    void SetFov(float fov);

    float GetNearZ() const { return nearZ_; }
    void SetNearZ(float nearZ);

    float GetFarZ() const { return farZ_; }
    void SetFarZ(float farZ);

    float GetAspectRatio() const { return aspectRatio_; }
    void SetAspectRatio(float aspect);

    bool IsMainCamera() const { return isMainCamera_; }
    void SetMainCamera(bool isMain);

    // 注視点モード操作
    bool IsUseTarget() const { return useTarget_; }
    void SetUseTarget(bool useTarget) { useTarget_ = useTarget; }
    const Vector3& GetTarget() const { return target_; }
    void SetTarget(const Vector3& target) { target_ = target; }

    // ==========================================
    // 行列・レイ取得
    // ==========================================
    const Matrix4x4& GetViewMatrix() const;
    const Matrix4x4& GetProjectionMatrix() const;

    /// <summary>
    /// スクリーン座標からワールド空間へのレイを生成する（ピッキング用）
    /// </summary>
    void CreateRay(const Vector2& screenPos, float windowWidth, float windowHeight, Vector3& rayStart, Vector3& rayDir) const;

    /// <summary>
    /// 内部で保持している BaseCamera インスタンスへのアクセス
    /// </summary>
    std::shared_ptr<BaseCamera> GetCameraInstance() const { return cameraInstance_; }

private:
    void UpdateProjection();
    void RegisterToCameraManager();
    void UnregisterFromCameraManager();

private:
    std::shared_ptr<BaseCamera> cameraInstance_;

    // カメラ光学パラメータ (デフォルト値)
    float fov_ = 0.45f;
    float aspectRatio_ = 16.0f / 9.0f;
    float nearZ_ = 0.1f;
    float farZ_ = 1000.0f;

    // 注視点モード
    bool useTarget_ = false;
    Vector3 target_ = { 0.0f, 0.0f, 0.0f };

    // メインカメラ判定
    bool isMainCamera_ = true;
    bool isRegistered_ = false;
    std::string registeredName_ = "";
};
