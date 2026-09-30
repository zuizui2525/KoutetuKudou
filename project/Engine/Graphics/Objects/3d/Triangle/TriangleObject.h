#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include "Engine/Graphics/Objects/3d/Object3D.h"

class TriangleObject: public Object3D {
public:
    TriangleObject() = default;
    ~TriangleObject() = default;

    void Initialize(int lightingMode = 2);

    // 更新処理
    void Update();

    // 描画処理
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "");
  
private:
    // 頂点数定数 (マジックナンバー排除)
    static constexpr uint32_t kVertexCount = 3;

    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;

    // バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
};
