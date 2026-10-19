#pragma once
#include "Engine/Component/ScriptComponent.h"
#include <string>

/// <summary>
/// GameOverシーン専用の演出・シーン遷移ディレクタースクリプト
/// </summary>
class GameOverDirectorScript : public ScriptComponent {
public:
    GameOverDirectorScript();
    ~GameOverDirectorScript() override;

    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

private:
    std::string nextSceneName_ = "Title";
    float tvNoiseStrength_ = 0.1f;
};
