#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 参考例スクリプト: バインドされたターゲットGameObjectを追尾・追従するコンポーネント
/// </summary>
class SampleFollowerScript : public ScriptComponent {
public:
    SampleFollowerScript();
    ~SampleFollowerScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

private:
    float followSpeed_ = 5.0f;
    float stoppingDistance_ = 2.5f;
    float lookAtSpeed_ = 10.0f;
    bool enableSmoothing_ = true;
};
