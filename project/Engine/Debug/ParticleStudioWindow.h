#pragma once
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleSetting.h"
#include "Engine/Graphics/Texture/RenderTexture.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <vector>

/// <summary>
/// GPUパーティクルスタジオ (専用メインワークスペース型エディター)
/// 独立したオフスクリーンプレビュー、オービットカメラ、作成ウィザード、詳細パラメータ編集を提供
/// </summary>
class ParticleStudioWindow {
public:
    // マジックナンバー排除のための定数
    static constexpr uint32_t kDefaultPreviewWidth = 1280;
    static constexpr uint32_t kDefaultPreviewHeight = 720;
    static constexpr UINT kStudioSrvHeapIndex = 1500; // SRVヒープの予約スロット
    static constexpr float kMinCameraDistance = 1.0f;
    static constexpr float kMaxCameraDistance = 100.0f;
    static constexpr float kDefaultCameraDistance = 8.0f;
    static constexpr float kCameraRotateSpeed = 0.005f;
    static constexpr float kCameraZoomSpeed = 0.5f;
    static constexpr float kCameraPanSpeed = 0.01f;
    static constexpr float kDefaultInspectorWidth = 440.0f;
    static constexpr float kMinInspectorWidth     = 280.0f;
    static constexpr float kMinViewportWidth      = 220.0f;
    static constexpr float kSplitterWidth         = 6.0f;
    static constexpr float kInspectorLabelReservedWidth = 170.0f;

#ifdef _USEIMGUI
    ParticleStudioWindow();
    ~ParticleStudioWindow();

    /// <summary>
    /// スタジオウィンドウの描画（DockSpace対応メインウィンドウ）
    /// </summary>
    /// <param name="open">開閉フラグポインタ</param>
    /// <param name="cmdList">描画コマンドリスト</param>
    void Draw(bool* open, ID3D12GraphicsCommandList* cmdList);

    /// <summary>
    /// プレビュー画面の初期化
    /// </summary>
    bool Initialize();

    /// <summary>
    /// プレビュー用パーティクルとシミュレーションの更新
    /// </summary>
    void Update(float deltaTime);

    /// <summary>
    /// プレビューの描画実行
    /// </summary>
    void RenderPreview(ID3D12GraphicsCommandList* cmdList);
#endif

private:
#ifdef _USEIMGUI
    // 内部GUIヘルパー
    void DrawToolbar();
    void DrawViewport();
    void DrawControlBar();
    void DrawStudioInspector();
    void DrawWizardModal();

    // カメラ更新
    void UpdateCamera();
    void ResetCamera();

    // プリセット適用
    void ApplyPreset(const std::string& presetName);

private:
    bool isInitialized_ = false;

    // プレビューレンダリングリソース
    std::unique_ptr<RenderTexture> previewRenderTexture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};

    // プレビュー専用深度ステンシルリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

    // プレビュー専用パーティクルインスタンス
    std::unique_ptr<GPUParticleObject> previewParticle_;
    GPUParticleSetting currentSetting_;

    // プレビュー用オービットカメラ
    Vector3 cameraTarget_ = { 0.0f, 1.0f, 0.0f };
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.35f;
    float cameraDistance_ = kDefaultCameraDistance;
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projMatrix_{};
    Vector3 cameraPos_{};

    // シミュレーション状態制御
    bool isPlaying_ = true;
    float timeScale_ = 1.0f;
    bool requestStepFrame_ = false;

    // ビューポート状態
    bool isViewportHovered_ = false;
    Vector2 viewportSize_ = { static_cast<float>(kDefaultPreviewWidth), static_cast<float>(kDefaultPreviewHeight) };
    int selectedBgColorIndex_ = 0; // 0: ダークグレー, 1: 黒, 2: ブルー, 3: 明るいグレー

    // ウィザードポップアップフラグ
    bool showWizardModal_ = false;

    // ファイル保存・読み込み
    char saveFileNameBuffer_[128] = "MyCustomParticle";
    std::string currentLoadedFile_ = "";

    // 左右ペイン分割スプリッター (マウスドラッグで調整可能)
    float inspectorWidth_ = kDefaultInspectorWidth;
#endif
};
