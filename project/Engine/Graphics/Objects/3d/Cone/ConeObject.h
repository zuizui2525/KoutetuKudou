#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/MathStructs.h"

class ConeObject : public Object3D {
public:
    ConeObject() = default;
    ~ConeObject() = default;

    void Initialize(int lightingMode = 2);

    // 更新処理
    void Update();

    // 描画処理
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "");

    // Getter
    float GetRadius() const { return radius_; }
    float GetHeight() const { return height_; }
    uint32_t GetSubdivision() const { return subdivision_; }

    // Setter
    void SetRadius(float radius) { radius_ = radius; needsUpdate_ = true; }
    void SetHeight(float height) { height_ = height; needsUpdate_ = true; }
    void SetSubdivision(uint32_t subdiv) { subdivision_ = subdiv; needsUpdate_ = true; }

private:
    // メッシュ定数 (マジックナンバー排除)
    static constexpr uint32_t kIndicesPerSegment = 6;

    void CreateMesh();

private:
    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    // バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    // パラメータ
    float radius_ = 0.5f;           // 底面の半径
    float height_ = 1.0f;           // 高さ
    uint32_t subdivision_ = 16;     // 円周の分割数
    bool needsUpdate_ = false;      // 再生成フラグ
};
