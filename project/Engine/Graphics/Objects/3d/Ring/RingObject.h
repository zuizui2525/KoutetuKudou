#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/MathStructs.h"

class RingObject : public Object3D {
public:
    RingObject() = default;
    ~RingObject() = default;

    void Initialize(int lightingMode = 2);

    // 更新処理
    void Update();

    // 描画処理
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "");

    // Getter
    float GetMainRadius() const { return mainRadius_; }
    float GetTubeRadius() const { return tubeRadius_; }
    uint32_t GetMainSubdivision() const { return mainSubdivision_; }
    uint32_t GetTubeSubdivision() const { return tubeSubdivision_; }

    // Setter
    void SetMainRadius(float radius) { mainRadius_ = radius; needsUpdate_ = true; }
    void SetTubeRadius(float radius) { tubeRadius_ = radius; needsUpdate_ = true; }
    void SetMainSubdivision(uint32_t subdiv) { mainSubdivision_ = subdiv; needsUpdate_ = true; }
    void SetTubeSubdivision(uint32_t subdiv) { tubeSubdivision_ = subdiv; needsUpdate_ = true; }

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
    float mainRadius_ = 1.0f;       // リング全体の主半径
    float tubeRadius_ = 0.3f;       // チューブ部分の半径
    uint32_t mainSubdivision_ = 32; // 円周方向の分割数
    uint32_t tubeSubdivision_ = 16; // チューブ断面方向の分割数
    bool needsUpdate_ = false;      // 再生成フラグ
};
