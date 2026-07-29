#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/3d/Sphere/SphereObject.h"

struct LineSegmentData {
    Vector3 startPoint;
    Vector3 endPoint;
    float thickness = 0.1f;
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

class Input;
class CameraManager;
class BaseCamera;

/**
 * @brief 手書きルートの記録、補間、および等速走行制御を管理するクラス
 */
class Route {
public:
    Route();
    ~Route() = default;

    void Initialize(Input* input, CameraManager* cameraMgr);
    void Update(BaseCamera* activeCamera);
    void Update2D(const Vector3& intersectPos);
    void UpdateSpheres();
    void UpdateLines();
    void Draw();
    void DrawSpheres();
    void SyncFrom(const Route* other);

    // 走行モード開始時の補間計算
    void FinalizeRoute();

    // 走行距離に応じた補間座標・向き・回転の算出
    Vector3 GetPositionAtDistance(float distance) const;
    Vector3 GetTangentAtDistance(float distance) const;
    Vector3 GetRotationAtDistance(float distance) const;

    // ゲッター/セッター
    float GetTotalDistance() const { return totalDistance_; }
    const std::vector<float>& GetAccumDistances() const { return accumDistances_; }
    const std::vector<Vector3>& GetRawPoints() const { return rawPoints_; } // ★追加
    bool IsDrawing() const { return isDrawing_; }
    bool HasReachedGoal() const { return hasReachedGoal_; }
    void Reset();
    void SetupArea(int areaIndex);
    int GetCurrentAreaIndex() const { return currentAreaIndex_; }
    float GetCurrentAreaStartZ() const { return currentAreaStartZ_; }
    float GetCurrentAreaGoalZ() const { return currentAreaGoalZ_; }
    void ClearForNewArea();

    void AddGizmoRect(const Vector3& center, float width, float depth, const Vector4& color);
    void AddGizmoCircle(const Vector3& center, float radius, const Vector4& color);

private:
    // 補間計算ヘルパー
    Vector3 CatmullRomInterpolate(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) const;
    void BuildEqualSpacingTable();
    void CreateBatchResources();

private:
    // マジックナンバー排除のための定数
    static inline const float kMinPointDistance = 0.2f;              // 軌跡点間の最小距離
    static inline const float kPlaneIntersectY = 0.0f;               // 地平面Y座標
    static inline const float kGoalAreaZ = 240.0f;                  // ゴールエリアの中心Z
    static inline const float kStartAreaZ = -240.0f;                // スタートエリアの中心Z
    static inline const float kAreaRadius = 4.0f;                   // 円形判定エリアの半径
    static inline const float kMapBoundaryX = 15.0f;                // マップの左右外枠
    static inline const float kMapBoundaryZ = 500.0f;               // マップの前後外枠
    static inline const int kCircleDivision = 32;                   // 円形ギズモの描画分割数
    static inline const float kPi = 3.14159265f;                    // 円周率
    static inline const float kLineThickness = 0.4f;                // ルート線の太さ
    static inline const float kGizmoThickness = 0.15f;              // ギズモ線の太さ
    static inline const Vector4 kLineColor = { 1.0f, 1.0f, 0.0f, 1.0f }; // ルート線の色（目立つ黄色）
    static inline const float kHalf = 0.5f;

    static inline const size_t kMaxSegments = 2000;                  // 一括描画の最大セグメント数
    static inline const uint32_t kVerticesPerSegment = 4;
    static inline const uint32_t kIndicesPerSegment = 6;

private:
    Input* input_ = nullptr;
    CameraManager* cameraMgr_ = nullptr;

    std::vector<Vector3> rawPoints_;                 // 記録した軌跡の点配列
    std::vector<Vector3> pathPoints_;                // 補間された滑らかなルート配列
    std::vector<float> accumDistances_;              // 各補間点の累積距離テーブル（等速化用）
    float totalDistance_ = 0.0f;                     // ルートの総距離

    std::vector<LineSegmentData> lineSegments_;          // 手書きルートの直線セグメント
    std::vector<LineSegmentData> editorGizmoSegments_;   // ギズモ枠線用セグメント

    // 一括バッチ描画用GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> batchVertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> batchIndexResource_;
    D3D12_VERTEX_BUFFER_VIEW batchVbView_{};
    D3D12_INDEX_BUFFER_VIEW batchIbView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> batchWvpResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> batchMaterialResource_;
    TransformationMatrix* batchWvpData_ = nullptr;
    Material* batchMaterialData_ = nullptr;

    std::unique_ptr<SphereObject> startSphere_;
    std::unique_ptr<SphereObject> goalSphere_;

    bool isDrawing_ = false;                         // 描画中フラグ
    bool hasReachedGoal_ = false;                    // ゴールエリア到達フラグ

private:
    void SetupAreaGizmos();

    int currentAreaIndex_ = 0;                       // 現在のエリアインデックス (0〜3)
    float currentAreaStartZ_ = -240.0f;              // 現在のエリアの開始Z座標
    float currentAreaGoalZ_ = -120.0f;               // 現在のエリアの目標Z座標
};
