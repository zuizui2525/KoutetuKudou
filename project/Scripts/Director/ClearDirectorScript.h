#pragma once
#include "Engine/Component/ScriptComponent.h"
#include <string>

/// <summary>
/// Clearシーン専用の祝砲花火演出・シーン遷移ディレクタースクリプト
/// </summary>
class ClearDirectorScript : public ScriptComponent {
public:
    ClearDirectorScript();
    ~ClearDirectorScript() override;

    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnInspectorGUI() override;

private:
    int fireworkTimer_ = 0;
    std::string nextSceneName_ = "Title";

    // 祝砲花火パラメータ（インスペクター調整可能）
    int minInterval_ = 30;
    int maxIntervalRange_ = 61;
    float rangeX_ = 60.0f;
    float rangeZ_ = 20.0f;
    float minY_ = -5.0f;
};
