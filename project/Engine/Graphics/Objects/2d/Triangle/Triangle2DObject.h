#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/RenderStructs.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Debug/IGameObject.h"

class Triangle2DObject : Base2D, public IGameObject {
public:
    Triangle2DObject() = default;
    ~Triangle2DObject() = default;

    void Initialize(int lightingMode = 0);

    // 更新処理
    void Update() override;

    // 描画処理
    void Draw(const std::string& textureKey = "white", bool draw = true);

    void DrawInspector() override;

    // Getter
    Transform& GetTransform() { return transform_; }
    Vector3& GetScale() { return transform_.scale; }
    Vector3& GetRotate() { return transform_.rotate; }
    Vector3& GetPosition() { return transform_.translate; }
    Transform& GetUVTransform() { return uvTransform_; }
    Material* GetMaterialData() { return materialData_; }
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }

    // Setter
    void SetTransform(const Transform& transform) { transform_ = transform; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetPosition(const Vector3& position) { transform_.translate = position; }
    void SetSize(float width, float height);

private:
    // メッシュ定数 (マジックナンバー排除)
    static constexpr uint32_t kVertexCount = 3;
    static constexpr uint32_t kIndexCount = 3;
    static constexpr float kDefaultWidth = 100.0f;
    static constexpr float kDefaultHeight = 100.0f;
    static constexpr float kNormalZ = -1.0f;

    // 頂点座標を更新する内部関数
    void UpdateVertexData();

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    Material* materialData_ = nullptr;
    TransformationMatrix* wvpData_ = nullptr;
    VertexData* vertexData_ = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Transform uvTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    float width_ = kDefaultWidth;
    float height_ = kDefaultHeight;
};
