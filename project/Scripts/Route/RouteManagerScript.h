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

    /// <summary>
    /// マウスドラッグによる手書き入力座標を受け取り、サンプリング間隔の判定、
    /// スタート地点付近での自動リセット、ゴール到達判定を処理します。
    /// </summary>
    /// <param name="worldIntersectPos">地面または2Dマップ上のレイ交差ワールド座標</param>
    /// <param name="isTrigger">ドラッグ開始（マウスダウン）フレームか</param>
    /// <param name="isPress">マウスボタン押下中か</param>
    void UpdateDrawing(const Vector3& worldIntersectPos, bool isTrigger, bool isPress);

    /// <summary>
    /// 手書きルート（rawPoints_）に対して移動平均フィルタによる平滑化を行い、
    /// Catmull-Romスプライン曲線補間パス（pathPoints_）および等速走行用累積距離テーブルを構築します。
    /// </summary>
    void FinalizeRoute();

    /// <summary>
    /// 現在のエリアの手書きルートおよび補間データをクリアし、
    /// スタート地点座標のみが登録された初期状態へリセットします。
    /// </summary>
    void ResetRoute();

    /// <summary>
    /// 指定されたエリアインデックス（0〜3）に基づいてスタートZ座標およびゴールZ座標を設定し、
    /// ルートを初期化します。
    /// </summary>
    /// <param name="areaIndex">エリア番号（0起点）</param>
    void SetupArea(int areaIndex);

    int GetCurrentAreaIndex() const { return currentAreaIndex_; }
    float GetCurrentAreaStartZ() const { return currentAreaStartZ_; }
    float GetCurrentAreaGoalZ() const { return currentAreaGoalZ_; }

    /// <summary>
    /// スタート地点からの累積走行距離（m）における、ルート曲線上の3Dワールド座標を取得します。
    /// 距離テーブル間の線形補間により滑らかな位置を算出します。
    /// </summary>
    /// <param name="distance">スタートからの走行距離（m）</param>
    /// <returns>曲線上の3Dワールド座標</returns>
    Vector3 GetPositionAtDistance(float distance) const;

    /// <summary>
    /// スタート地点からの累積走行距離（m）における、ルート曲線の接線ベクトル（正規化された進行方向）を取得します。
    /// </summary>
    /// <param name="distance">スタートからの走行距離（m）</param>
    /// <returns>進行方向の正規化単位ベクトル</returns>
    Vector3 GetTangentAtDistance(float distance) const;

    /// <summary>
    /// スタート地点からの累積走行距離（m）における、自機カメラ・モデルの向き（Pitch, Yaw, Roll）オイラー角を取得します。
    /// </summary>
    /// <param name="distance">スタートからの走行距離（m）</param>
    /// <returns>ラジアン単位の回転角 { pitch, yaw, 0.0f }</returns>
    Vector3 GetRotationAtDistance(float distance) const;

    float GetTotalDistance() const { return totalDistance_; }
    bool HasReachedGoal() const { return hasReachedGoal_; }
    bool IsDrawing() const { return isDrawing_; }
    const std::vector<Vector3>& GetPathPoints() const { return pathPoints_; }
    const std::vector<Vector3>& GetRawPoints() const { return rawPoints_; }
    float GetAreaRadius() const { return kAreaRadius; }

private:
    /// <summary>
    /// Catmull-Rom補間された各制御点間のユークリッド距離を積算し、
    /// 弧長パラメータ（Arc-Length Table）を構築して等速走行を可能にします。
    /// </summary>
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
