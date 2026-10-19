#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 参考例スクリプト: 矢印キーまたはWASDで平面的に移動するプレイヤー操作コンポーネント
/// </summary>
class SamplePlayerScript : public ScriptComponent {
public:
    SamplePlayerScript();
    ~SamplePlayerScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

private:
    float moveSpeed_ = 10.0f;
    bool enableAutoOrbit_ = false;
    float orbitAngle_ = 0.0f;
    float orbitRadius_ = 5.0f;
    float orbitSpeed_ = 1.5f;
    Vector3 initialPosition_ = { 0.0f, 0.0f, 0.0f };
};
