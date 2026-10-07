#pragma once  
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Debug/IGameObject.h"
#include <string>

class BaseCamera : public IGameObject {  
public:  
    virtual ~BaseCamera();  

    virtual void Initialize();  
    virtual void Update();  
    void DrawInspector() override;

    // 座標・回転の操作  
    void SetPosition(const Vector3& pos) { transform_.translate = pos; }  
    const Vector3& GetPosition() const { return transform_.translate; }  
    void SetRotation(const Vector3& rot) { transform_.rotate = rot; }  
    const Vector3& GetRotation() const { return transform_.rotate; }  

    // 注視点(Target)の操作  
    void SetTarget(const Vector3& target) {  
        target_ = target;  
        useTarget_ = true;  
    }  
    void DisableTarget() { useTarget_ = false; }  
    const Vector3& GetTarget() const { return target_; }
    bool IsUseTarget() const { return useTarget_; }  

    // 実効的な回転角（注視点モードの場合は視線方向から計算したオイラー角）を取得
    Vector3 GetCalculatedRotation() const;  

    // 行列取得  
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }  
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }  
    void UpdateProjection(float aspect);

    // スクリーン座標からワールド空間へのレイを生成する
    void CreateRay(const Vector2& screenPos, float windowWidth, float windowHeight, Vector3& rayStart, Vector3& rayDir) const;

protected:  
    Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Matrix4x4 viewMatrix_ = Math::MakeIdentity();  
    Matrix4x4 projectionMatrix_ = Math::MakeIdentity();

    float fov_ = 0.45f;  
    float aspectRatio_ = 16.0f / 9.0f;  
    float nearZ_ = 0.1f;  
    float farZ_ = 1000.0f;  

    Vector3 target_ = { 0.0f, 0.0f, 0.0f };  
    bool useTarget_ = false;  
};
