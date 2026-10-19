#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <vector>

/// <summary>
/// マウスドラッグによる手書きルート入力、Catmull-Rom曲線補間、等速走行距離テーブルを管理するスクリプト
/// </summary>
class RouteManagerScript : public ScriptComponent {
public:
    RouteManagerScript();
    ~RouteManagerScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

    // ルート入力処理
    void UpdateDrawing(const Vector3& worldIntersectPos, bool isTrigger, bool isPress);
    void FinalizeRoute();
    void ResetRoute();

    // エリア管理
    void SetupArea(int areaIndex);
    int GetCurrentAreaIndex() const { return currentAreaIndex_; }
    float GetCurrentAreaStartZ() const { return currentAreaStartZ_; }
    float GetCurrentAreaGoalZ() const { return currentAreaGoalZ_; }

    // 走行距離に応じた位置・向き・回転の取得
    Vector3 GetPositionAtDistance(float distance) const;
    Vector3 GetTangentAtDistance(float distance) const;
    Vector3 GetRotationAtDistance(float distance) const;

    float GetTotalDistance() const { return totalDistance_; }
    bool HasReachedGoal() const { return hasReachedGoal_; }
    bool IsDrawing() const { return isDrawing_; }
    const std::vector<Vector3>& GetPathPoints() const { return pathPoints_; }
    const std::vector<Vector3>& GetRawPoints() const { return rawPoints_; }
    float GetAreaRadius() const { return kAreaRadius; }

private:
    void BuildEqualSpacingTable();

private:
    // マジックナンバー排除のための定数
    static constexpr float kAreaRadius = 5.0f;
    static constexpr float kMinPointDistance = 0.5f;
    static constexpr float kMapBoundaryX = 50.0f;
    static constexpr int kPathDivision = 6;
    static constexpr float kAreaLength = 120.0f; // 1エリアあたりのZ長

    // メンバ変数
    int currentAreaIndex_ = 0;
    float currentAreaStartZ_ = 0.0f;
    float currentAreaGoalZ_ = 120.0f;

    std::vector<Vector3> rawPoints_;
    std::vector<Vector3> pathPoints_;
    std::vector<float> accumDistances_;
    float totalDistance_ = 0.0f;

    bool isDrawing_ = false;
    bool hasReachedGoal_ = false;
};
