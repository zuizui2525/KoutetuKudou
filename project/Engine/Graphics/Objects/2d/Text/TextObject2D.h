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

/**
 * @brief 2Dテキスト描画用の板ポリゴンオブジェクト
 * TextTextureGeneratorで生成されたテキストテクスチャを貼り付けて2D描画を行います。
 */
class TextObject2D : public Base2D, public IGameObject {
public:
    TextObject2D();
    ~TextObject2D() override;

    void Initialize();
    void Update() override;
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, bool draw = true);
    void DrawInspector() override;

    // トランスフォーム操作
    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }
    void SetTransform(const Transform& transform) { transform_ = transform; }

    Vector3& GetPosition() { return transform_.translate; }
    const Vector3& GetPosition() const { return transform_.translate; }
    void SetPosition(const Vector3& pos) { transform_.translate = pos; }

    Vector3& GetRotate() { return transform_.rotate; }
    const Vector3& GetRotate() const { return transform_.rotate; }
    void SetRotate(const Vector3& rot) { transform_.rotate = rot; }

    Vector3& GetScale() { return transform_.scale; }
    const Vector3& GetScale() const { return transform_.scale; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // マテリアル・サイズ
    Material* GetMaterialData() { return materialData_; }
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }
    void SetSize(float width, float height);
    void SetColor(const Vector4& color);

private:
    // メッシュ定数 (マジックナンバー排除)
    static constexpr uint32_t kVertexCount = 4;
    static constexpr uint32_t kIndexCount = 6;
    static constexpr float kDefaultWidth = 100.0f;
    static constexpr float kDefaultHeight = 32.0f;
    static constexpr float kDefaultShininess = 30.0f;

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
