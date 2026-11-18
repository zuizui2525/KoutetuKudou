#pragma once
#include "Engine/Component/ScriptComponent.h"
#include <string>
#include <memory>

class SpriteObject;
class TextObject2D;
class TextTextureGenerator;

/// <summary>
/// Clearシーン専用の祝砲花火演出・オービットシネマティックカメラ・豪華ビクトリーHUDディレクタースクリプト
/// </summary>
class ClearDirectorScript : public ScriptComponent {
public:
    ClearDirectorScript();
    ~ClearDirectorScript() override;

    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void Draw2D() override;
    void OnInspectorGUI() override;

private:
    void Init2DObjects();
    void RenderVictoryHUD();

    // マジックナンバー排除のための定数
    static constexpr uint32_t kGpuFireworkParticleCount = 3000; // 1発あたりのGPU祝砲花火粒子数
    static constexpr int kDefaultFireworkInterval = 38;         // 自動打ち上げ基本間隔（約0.63秒ごと）
    static constexpr float kCameraOrbitRadius = 14.0f;          // クリアシーンでのカメラ旋回半径 (m)
    static constexpr float kCameraOrbitHeight = 4.5f;           // クリアシーンでのカメラ基準高さ (m)
    static constexpr float kCameraOrbitSpeed = 0.005f;          // カメラの優雅な旋回速度 (rad/frame)
    static constexpr float kCameraLookAtY = 8.0f;               // カメラ注視点（上空の花火を見上げる高さ）
    static constexpr float kPressSpaceBlinkFreq = 4.5f;         // 「PRESS SPACE」テキストの点滅周波数
    static constexpr float kBannerWidth = 760.0f;               // ビクトリーバナー横幅 (px)
    static constexpr float kBannerHeight = 210.0f;              // ビクトリーバナー高さ (px)
    static constexpr float kFadeInSpeed = 0.025f;               // シーン開始時の明転フェードイン速度 (/frame)

    int fireworkTimer_ = 0;
    int nextFireworkDelay_ = kDefaultFireworkInterval;
    float cameraOrbitAngle_ = 0.0f;
    float victoryTimer_ = 0.0f;
    float sceneFadeInAlpha_ = 1.0f;                             // 開始時は100%真っ黒で準備前描画を完全隠蔽
    std::string nextSceneName_ = "Title";

    // 祝砲花火打ち上げ範囲パラメータ（インスペクター調整可能）
    int minInterval_ = 25;
    int maxIntervalRange_ = 45;
    float rangeX_ = 45.0f;
    float baseY_ = 14.0f;
    float rangeY_ = 10.0f;
    float baseZ_ = 25.0f;
    float rangeZ_ = 25.0f;

    // DirectX 12 ビクトリーHUD用パーツ
    bool is2DInitialized_ = false;
    std::unique_ptr<SpriteObject> sceneFadeSprite_;
    std::unique_ptr<SpriteObject> victoryBannerSprite_;
    std::unique_ptr<TextObject2D> victoryTitleTextObj_;
    std::unique_ptr<TextTextureGenerator> victoryTitleTextGen_;
    std::unique_ptr<TextObject2D> victorySubTextObj_;
    std::unique_ptr<TextTextureGenerator> victorySubTextGen_;
    std::unique_ptr<TextObject2D> pressSpaceTextObj_;
    std::unique_ptr<TextTextureGenerator> pressSpaceTextGen_;
};
