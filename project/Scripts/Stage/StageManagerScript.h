#pragma once
#include "Engine/Component/ScriptComponent.h"
#include <vector>

class GameObject;

/// <summary>
/// 全4エリアのステージマップ構造（左右境界柱、エリア境界ゲート柱等）を統括管理するスクリプト
/// </summary>
class StageManagerScript : public ScriptComponent {
public:
    StageManagerScript();
    ~StageManagerScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

private:
    void BuildStageGeometry();

private:
    // マジックナンバー排除のための定数
    static constexpr int kAreaCount = 4;
    static constexpr float kAreaLength = 120.0f;     // 1エリアの長さ（m）
    static constexpr float kBoundaryX = 50.0f;       // 左右の境界幅（m）
    static constexpr float kPillarSpacingZ = 30.0f;  // 柱の間隔（m）
    static constexpr float kPillarHeight = 8.0f;     // 柱の高さ（m）
    static constexpr float kPillarRadius = 1.0f;     // 柱の太さ（m）

    // 柱やステージ装飾の生成済みオブジェクト追跡
    std::vector<GameObject*> stageProps_;
};
