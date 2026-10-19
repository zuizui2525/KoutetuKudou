#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>
#include <vector>

class LineObject;
class RouteManagerScript;

/// <summary>
/// 手書きルート曲線、スタート・ゴールサークルを3D空間上に可視化レンダリングするスクリプト
/// </summary>
class RouteRendererScript : public ScriptComponent {
public:
    RouteRendererScript();
    ~RouteRendererScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnInspectorGUI() override;

private:
    void DrawPolyline(const std::vector<Vector3>& points, const Vector4& color, float thickness, size_t& linePoolIdx);
    void DrawCircle(const Vector3& center, float radius, const Vector4& color, float thickness, size_t& linePoolIdx);
    LineObject* GetOrCreateLine(size_t index);
    std::vector<Vector3> DownsamplePoints(const std::vector<Vector3>& points, float minSpacing) const;

private:
    // マジックナンバー排除のための定数
    static constexpr int kCircleSegments = 32;       // サークルの分割数
    static constexpr float kGroundYOffset = 0.05f;   // 地面とのZファイティング防止オフセット
    static constexpr float kRenderMinSpacing = 1.0f; // 描画ラインのサンプリング間隔 (m)
    static constexpr size_t kInitialLinePoolSize = 160;

    // ビジュアルパラメータ（インスペクター調整可能）
    float completedPathThickness_ = 0.25f;           // 完成ルートの太さ
    float drawingPathThickness_ = 0.18f;             // 手書き中の太さ
    float circleThickness_ = 0.2f;                   // スタート・ゴール枠の太さ

    Vector4 completedPathColor_ = { 0.1f, 0.85f, 1.0f, 1.0f }; // 鮮やかなシアン
    Vector4 drawingPathColor_ = { 1.0f, 0.8f, 0.2f, 1.0f };    // 描画中のオレンジ
    Vector4 startCircleColor_ = { 0.2f, 1.0f, 0.3f, 1.0f };    // スタートのグリーン
    Vector4 goalCircleColor_ = { 1.0f, 0.25f, 0.25f, 1.0f };   // ゴールのレッド

    RouteManagerScript* routeMgr_ = nullptr;
    std::vector<std::unique_ptr<LineObject>> linePool_;
};
