#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Math/MathStructs.h"
#include "Engine/Base/BaseResource.h"

class Skybox : public Base3D {
public:
    struct SkyboxVertex {
        Vector4 position;
    };

    Skybox() = default;
    ~Skybox() = default;

    void Initialize();
    void Update();
    void Draw(const std::string& textureKey);

    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    void SetColor(const Vector4& color) {
        color_ = color;
        if (wvpData_) {
            wvpData_->color = color_;
        }
    }
    const Vector4& GetColor() const { return color_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;

    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    struct WVPData {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Vector4 color;
    };
    WVPData* wvpData_ = nullptr;

    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 半径を大きくするためにScaleを大きく設定
    Transform transform_{ {5000.0f, 5000.0f, 5000.0f}, {0,0,0}, {0,0,0} };
};
