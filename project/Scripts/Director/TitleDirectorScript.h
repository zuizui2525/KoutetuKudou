#pragma once
#include "Engine/Component/ScriptComponent.h"
#include <memory>
#include <string>

class Skybox;

/// <summary>
/// Titleシーン専用の進行管理・演出ディレクタースクリプト
/// </summary>
class TitleDirectorScript : public ScriptComponent {
public:
    TitleDirectorScript();
    ~TitleDirectorScript() override;

    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnInspectorGUI() override;

private:
    std::unique_ptr<Skybox> skybox_;
    std::string nextSceneName_ = "Game";
};
