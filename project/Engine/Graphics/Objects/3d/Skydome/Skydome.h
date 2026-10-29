#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Math/MathStructs.h"
#include "Engine/Base/BaseResource.h"

/// <summary>
/// 天球（Skydome）描画クラス
/// カメラ位置への自動追従、緩やかな自転、最奥深度でのアンリット描画を提供
/// </summary>
class Skydome : public Base3D {
public:
    struct WVPData {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Vector4 color;
    };

    Skydome() = default;
    ~Skydome();

    void Initialize(const std::string& modelKey = "skydome");
    void Update();
    void Draw(const std::string& textureKey = "");

    // 姿勢・外観操作
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    const Vector3& GetScale() const { return transform_.scale; }

    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    const Vector3& GetRotate() const { return transform_.rotate; }

    void SetColor(const Vector4& color);
    const Vector4& GetColor() const { return color_; }

    // 自転速度操作（ラジアン/秒）
    void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }
    float GetRotationSpeed() const { return rotationSpeed_; }

    // 有効/無効
    void SetActive(bool active) { isActive_ = active; }
    bool IsActive() const { return isActive_; }

    void DrawInspector();

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
    WVPData* wvpData_ = nullptr;

    std::string modelKey_ = "skydome";
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Transform transform_{ { 500.0f, 500.0f, 500.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    float rotationSpeed_ = 0.002f; // 緩やかな自転速度（ラジアン/フレーム）
    bool isActive_ = true;
};
