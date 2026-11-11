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
    static constexpr float kOrbitRotateSpeed       = 0.004f;  // Blender風MMBオービット回転感度
    static constexpr float kPanSpeedFactor         = 0.0016f; // Blender風Shift+MMBパン移動感度 (距離比例)
    static constexpr float kZoomWheelFactor        = 0.10f;   // Blender風ホイールズーム倍率
    static constexpr float kMinCameraDistance      = 2.0f;    // 最小ズーム距離 (m)
    static constexpr float kMaxCameraDistance      = 500.0f;  // 最大ズーム距離 (m)
    static constexpr float kMinCameraPitch         = -1.55f;  // オービット下限角 (ラジアン)
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

    void RebuildGridAndBorderMeshes();
    void RebuildBuildingMesh();
    void RenderGridAndBorders(ID3D12GraphicsCommandList* cmdList);

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

    // プレビュー用3Dオブジェクト (上書き競合防止のため独立インスタンス化)
    std::unique_ptr<CubeObject> previewGroundBase_;      // 外側の拡張ベース土台 (暗色プラットフォーム)
    std::unique_ptr<CubeObject> previewStageGround_;     // ステージ編集エリアの床面プレート
    std::unique_ptr<CubeObject> previewCornerPole_;      // 四隅の境界マーカーポスト
    std::unique_ptr<CubeObject> previewBuildingCube_;    // 配置済みビル用キューブ
    std::unique_ptr<CubeObject> previewHoverCube_;       // ホバープレビュー半透明キューブ
    std::unique_ptr<LineObject> previewLine_;

    // グリッド線・境界線のバッチ描画用オブジェクトと頂点バッファ
    std::unique_ptr<CubeObject> gridBatchObject_;        // 通常グリッド線用マテリアル保持
    std::unique_ptr<CubeObject> majorGridBatchObject_;   // 5マスごと主グリッド線用マテリアル保持
    std::unique_ptr<CubeObject> borderBatchObject_;      // ステージ外枠境界線用マテリアル保持
    std::unique_ptr<CubeObject> startLineBatchObject_;   // START手前ライン用マテリアル保持
    std::unique_ptr<CubeObject> goalLineBatchObject_;    // GOAL奥ライン用マテリアル保持
    std::unique_ptr<CubeObject> hoverBorderBatchObject_; // ホバー中マス枠線用マテリアル保持

    Microsoft::WRL::ComPtr<ID3D12Resource> gridVbResource_;
    D3D12_VERTEX_BUFFER_VIEW gridVbView_{};
    uint32_t gridVertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> majorGridVbResource_;
    D3D12_VERTEX_BUFFER_VIEW majorGridVbView_{};
    uint32_t majorGridVertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> borderVbResource_;
    D3D12_VERTEX_BUFFER_VIEW borderVbView_{};
    uint32_t borderVertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> startLineVbResource_;
    D3D12_VERTEX_BUFFER_VIEW startLineVbView_{};
    uint32_t startLineVertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> goalLineVbResource_;
    D3D12_VERTEX_BUFFER_VIEW goalLineVbView_{};
    uint32_t goalLineVertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> hoverBorderVbResource_;
    D3D12_VERTEX_BUFFER_VIEW hoverBorderVbView_{};
    uint32_t hoverBorderVertexCount_ = 0;

    // 配置済みビルのバッチ描画用バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> buildingVbResource_;
    D3D12_VERTEX_BUFFER_VIEW buildingVbView_{};
    uint32_t buildingVertexCount_ = 0;
    bool isBuildingMeshDirty_ = true;

    // メッシュ再構築判定用キャッシュ
    int cachedGridWidth_ = -1;
    int cachedGridHeight_ = -1;
    float cachedTileSize_ = -1.0f;

    // スタジオ専用カメラ (Blender風オービット操作 & WASDフライスルー操作)
    std::shared_ptr<BaseCamera> studioCamera_;
    Vector3 cameraPosition_ = { 0.0f, 35.0f, -40.0f };
    Vector3 cameraRotation_ = { 0.65f, 0.0f, 0.0f }; // 俯瞰約37度
    Vector3 cameraTarget_ = { 0.0f, 0.0f, 0.0f };   // 注視点 (Orbitの中心)
    float cameraDistance_ = 65.0f;                  // 注視点からの距離 (m)
    float cameraPitch_ = 0.62f;                     // 俯瞰ピッチ角 (ラジアン)
    float cameraYaw_ = 0.0f;                        // 水平ヨー角 (ラジアン)
    bool isCameraNavigating_ = false;               // 操作中フラグ
    bool isOrbitNavigating_ = false;                // MMBドラッグによるOrbit/Pan/Zoom操作中フラグ
    bool isFlyNavigating_ = false;                  // RMBドラッグによるWASDフライ操作中フラグ

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
