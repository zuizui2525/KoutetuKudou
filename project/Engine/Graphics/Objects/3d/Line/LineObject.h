#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/MathStructs.h"

class LineObject : public Object3D {
public:
    LineObject() = default;
    ~LineObject() = default;

    /// @brief 初期化（線なのでデフォルトでライティング無効(0)を推奨）
    void Initialize(int lightingMode = 0);

    // 更新処理
    void Update();

    // 描画処理
    void Draw(const std::string& textureKey = "white");

    // ==========================================
    // Getter
    // ==========================================
    Vector3 GetStartPoint() const { return startPoint_; }
    Vector3 GetEndPoint() const { return endPoint_; }
    float GetThickness() const { return thickness_; }
    bool GetUseTransform() const { return useTransform_; }

    // ==========================================
    // Setter
    // ==========================================
    void SetStartPoint(const Vector3& start) { startPoint_ = start; needsUpdate_ = true; }
    void SetEndPoint(const Vector3& end) { endPoint_ = end; needsUpdate_ = true; }
    void SetThickness(float thickness) { thickness_ = thickness; needsUpdate_ = true; }
    void SetUseTransform(bool useTransform) { useTransform_ = useTransform; }

private:
    // メッシュ定数 (マジックナンバー排除)
    static constexpr uint32_t kVertexCount = 4;
    static constexpr uint32_t kIndexCount = 6;

    void CreateMesh();
    void UpdateVertex(); // カメラ位置に合わせてビルボード四角形を計算する

private:
    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    // バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    // パラメータ
    Vector3 startPoint_ = { 0.0f, 0.0f, 0.0f };
    Vector3 endPoint_ = { 0.0f, 1.0f, 0.0f };
    float thickness_ = 0.01f;
    bool needsUpdate_ = false; // 再生成フラグ
    bool useTransform_ = false; // Transformを使うかどうか
};
