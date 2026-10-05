#pragma once
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include <d3d12.h>
#include <wrl.h>

class LightGizmoObject : public Object3D {
public:
    LightGizmoObject() = default;
    ~LightGizmoObject() override;

    void Initialize(int lightingMode = 0) override;
    void Update() override;
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "");

    void SetRadius(float radius);

private:
    void CreateMesh();

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    float radius_ = 0.5f;
    bool needsUpdate_ = false;
};
