#pragma once
#include "Engine/Graphics/Objects/3d/Object3D.h"

// XY平面上のリング（真ん中がくり抜かれた円）エフェクト用Primitive
class FlatRingObject : public Object3D {
public:
    // 分割数、外径、内径を指定して初期化
    FlatRingObject(uint32_t divide = 32, float outerRadius = 1.0f, float innerRadius = 0.2f);
    ~FlatRingObject() = default;

    void Initialize(int lightingMode);
    void Update();
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "");

    // パラメータ変更用セッター
    void SetDivide(uint32_t divide) { divide_ = divide; needsUpdate_ = true; }
    void SetOuterRadius(float radius) { outerRadius_ = radius; needsUpdate_ = true; }
    void SetInnerRadius(float radius) { innerRadius_ = radius; needsUpdate_ = true; }

private:
    // メッシュ定数 (マジックナンバー排除)
    static constexpr uint32_t kIndicesPerQuad = 6;

    void CreateMesh();

    uint32_t divide_;
    float outerRadius_;
    float innerRadius_;
    bool needsUpdate_ = false;

    // Vertex buffer & Index buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};
};
