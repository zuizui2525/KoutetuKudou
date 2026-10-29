#pragma once
#include "Engine/Component/ScriptComponent.h"
#include <memory>
#include <string>

class Skydome;

/// <summary>
/// 天球（Skydome）を制御・描画するコンポーネントスクリプト
/// 任意のGameObjectにアタッチすることで天球の背景描画を有効化します。
/// </summary>
class SkydomeScript : public ScriptComponent {
public:
    SkydomeScript();
    ~SkydomeScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnInspectorGUI() override;

    Skydome* GetSkydome() const { return skydome_.get(); }

    // パラメータ取得・設定（シリアライズ対応）
    float GetRotationSpeed() const { return rotationSpeed_; }
    void SetRotationSpeed(float speed);

    const Vector4& GetColor() const { return color_; }
    void SetColor(const Vector4& color);

    const std::string& GetModelKey() const { return modelKey_; }
    void SetModelKey(const std::string& key);

    const std::string& GetTextureKey() const { return textureKey_; }
    void SetTextureKey(const std::string& key);

private:
    void EnsureSkydome();

private:
    std::unique_ptr<Skydome> skydome_;
    std::string modelKey_ = "skydome";
    std::string textureKey_ = "skydome";
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    float rotationSpeed_ = 0.002f;
};
