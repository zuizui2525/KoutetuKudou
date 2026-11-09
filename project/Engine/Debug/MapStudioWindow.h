#pragma once

#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/Texture/RenderTexture.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Scripts/Stage/MapData.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <vector>

class CubeObject;
class LineObject;
class BaseCamera;

/// <summary>
/// GPUパーティクルスタジオ（ParticleStudioWindow）のUI設計に準拠した、
/// 独立プレビュー画面・オービットカメラ・専用インスペクターを備える3Dマップチップ制作スタジオ。
/// 「空白」と「ビル」のみに特化し、直感的なマップ配置環境を提供します。
/// </summary>
class MapStudioWindow {
public:
    // マジックナンバー排除のための定数
    static constexpr uint32_t kDefaultPreviewWidth = 1280;
    static constexpr uint32_t kDefaultPreviewHeight = 720;
    static constexpr UINT kStudioSrvHeapIndex = TextureManager::kReservedMapStudioIndex; // 予約スロット 5
    static constexpr float kCameraMoveSpeed        = 0.40f;   // デバッグカメラ移動速度 (m/frame)
    static constexpr float kCameraFastMultiplier   = 2.50f;   // Shiftダッシュ倍率
    static constexpr float kCameraRotateSpeed      = 0.003f;  // マウス回転感度
    static constexpr float kCameraWheelSensitivity = 2.00f;   // ホイール移動感度
    static constexpr float kMaxCameraPitch         = 1.55f;   // 上下首振り上限 (約89度)
    static constexpr float kDefaultInspectorWidth  = 380.0f;
    static constexpr float kMinInspectorWidth      = 260.0f;
    static constexpr float kMinViewportWidth       = 220.0f;
    static constexpr float kSplitterWidth          = 6.0f;
    static constexpr size_t kMaxPathLength         = 256;
    static constexpr size_t kMaxUndoHistory        = 50;

#ifdef _USEIMGUI
    MapStudioWindow();
    ~MapStudioWindow();

    /// <summary>
    /// スタジオウィンドウの描画（左右2ペイン、スプリッター、インスペクター）
    /// </summary>
    void Draw(bool* open, ID3D12GraphicsCommandList* cmdList);

    /// <summary>
    /// プレビュー画面リソースの初期化
    /// </summary>
    bool Initialize();

    /// <summary>
    /// プレビュー画面の3D描画実行
    /// </summary>
    void RenderPreview(ID3D12GraphicsCommandList* cmdList);

    /// <summary>
    /// 指定アセットファイルを開いて編集対象にする
    /// </summary>
    bool OpenMapAsset(const std::string& filePath);

    /// <summary>
    /// 現在のマップデータをファイルへ保存する
    /// </summary>
    bool SaveMapAsset(const std::string& filePath = "");

    bool IsOpen() const { return isOpen_; }
    void SetOpen(bool open) { isOpen_ = open; }
    const MapGrid& GetEditingGrid() const { return mapGrid_; }
#endif

private:
#ifdef _USEIMGUI
    void DrawToolbar();
    void DrawViewport();
    void DrawControlBar();
    void DrawStudioInspector();

    void UpdateCamera();
    void ResetCamera();
    void SetTopDownView();
    bool PerformGroundRaycast(const Vector2& mouseLocalPos, int& outGx, int& outGz);

    void PlaceBuilding(int x, int z);
    void RemoveBuilding(int x, int z);
    void ExecuteUndo();
    void ExecuteRedo();

    void OpenSaveFileDialog();
    void OpenLoadFileDialog();

private:
    bool isInitialized_ = false;
    bool isOpen_ = false;

    // プレビューレンダリングリソース
    std::unique_ptr<RenderTexture> previewRenderTexture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};

    // 深度ステンシル
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

    // プレビュー用3Dオブジェクト
    std::unique_ptr<CubeObject> previewCube_;
    std::unique_ptr<LineObject> previewLine_;

    // スタジオ専用デバッグカメラ (SceneViewと同一のWASD+マウス操作)
    std::shared_ptr<BaseCamera> studioCamera_;
    Vector3 cameraPosition_ = { 0.0f, 35.0f, -40.0f };
    Vector3 cameraRotation_ = { 0.65f, 0.0f, 0.0f }; // 俯瞰約37度
    bool isCameraNavigating_ = false; // 右クリック操作中フラグ

    // UI状態
    float inspectorWidth_ = kDefaultInspectorWidth;
    Vector2 viewportSize_ = { 0.0f, 0.0f };
    Vector2 viewportScreenPos_ = { 0.0f, 0.0f };
    bool isViewportHovered_ = false;
    int selectedBgColorIndex_ = 0; // 0: ダークグレー, 1: ブラック, 2: ディープブルー

    // 編集ツール (1: ビル配置, 0: 空白消去)
    int activeTool_ = kTileBuilding;

    // マップデータ
    MapGrid mapGrid_;
    std::string currentMapName_ = "Stage1_Area0";
    char mapNameBuffer_[kMaxPathLength] = "Stage1_Area0";

    // ホバー状態
    int hoveredGridX_ = -1;
    int hoveredGridZ_ = -1;
    bool isHoveredValid_ = false;
    int lastEditedGridX_ = -1;
    int lastEditedGridZ_ = -1;

    // Undo / Redo
    struct TileAction {
        int x = 0;
        int z = 0;
        int prevTile = kTileEmpty;
        int nextTile = kTileEmpty;
    };
    using ActionBatch = std::vector<TileAction>;
    struct UndoStep {
        bool isSnapshot = false;
        ActionBatch actions;
        MapGrid snapshotBefore;
        MapGrid snapshotAfter;
    };
    std::vector<UndoStep> undoStack_;
    std::vector<UndoStep> redoStack_;
    ActionBatch currentBatch_;

    // ステータスメッセージ
    std::string statusMessage_;
    bool isStatusSuccess_ = true;
    bool showResetModal_ = false;
#endif
};
