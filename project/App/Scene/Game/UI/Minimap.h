#pragma once
#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/RenderStructs.h"

class SpriteObject;
class Stage;
class Route;
class Input;
class CameraManager;
class StageEditor;

/**
 * @brief 2Dミニマップの描画およびルート描画モード時の入力を管理するクラス
 */
class Minimap {
public:
    Minimap();
    ~Minimap();

    // 初期化処理
    void Initialize(Stage* stage);

    // 更新処理 (線の手書き入力、LoL風カメラ移動、各スプライトの位置計算、敵配置操作)
    void Update(Input* input, Route* route, Stage* stage, Vector3& ioTargetZoom, StageEditor* stageEditor = nullptr);

    // 描画処理 (左ビューポート設定、スプライト一括描画)
    void Draw(int currentAreaIndex);

private:
    // マジックナンバー排除のための定数
    static inline const float kMinimapWidthRatio = 0.3f;            // 画面全体に対するミニマップ（左側）の幅比率
    static inline const float kAspect3D = 0.45f;                    // マップのアスペクト比フィット計算用アスペクト比
    static inline const float kBossSpawnLineZ = 180.0f;              // ボス戦が始まるZ座標
    static inline const Vector4 kBossAreaColor = { 1.0f, 0.0f, 0.0f, 0.3f }; // ボスエリアのカラー（赤の半透明）

    // 各スプライトのサイズ定数
    static inline const Vector2 kStartGoalIconSize = { 24.0f, 24.0f }; // スタート/ゴールアイコンのサイズ
    static inline const Vector2 kIndicatorIconSize = { 24.0f, 24.0f }; // 自機インジケータアイコンのサイズ
    static inline const Vector2 kPillarIconSize = { 16.0f, 16.0f };     // 障害物柱アイコンのサイズ
    static inline const float kFrameThickness = 2.0f;                   // 枠線の太さ
    static inline const float kRouteLineThickness = 2.0f;               // 手書きルート軌跡線の太さ

private:
    // 2Dスプライトリソース
    std::unique_ptr<SpriteObject> minimapBg_;
    std::unique_ptr<SpriteObject> startIcon_;
    std::unique_ptr<SpriteObject> goalIcon_;
    std::unique_ptr<SpriteObject> indicatorIcon_;
    std::unique_ptr<SpriteObject> bossArea2D_;                        // 【新規】ボス戦エリア用の赤半透明スプライト
    std::vector<std::unique_ptr<SpriteObject>> pillarIcons_;
    std::unique_ptr<SpriteObject> zoomFrame2D_[4];                    // ズームカメラ視野枠
    std::unique_ptr<SpriteObject> minimapBorderFrame2D_[4];          // ミニマップ外枠

    // 一時的な計算情報
    CameraManager* cameraMgr_ = nullptr;
    size_t activeMiniMapLineCount_ = 0;

private:
    // 一括バッチ描画用GPUリソース
    static inline const size_t kMaxMiniMapSegments = 2000;              // 一括描画の最大セグメント数
    static inline const uint32_t kVerticesPerSegment = 4;
    static inline const uint32_t kIndicesPerSegment = 6;

    Microsoft::WRL::ComPtr<ID3D12Resource> batchVertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> batchIndexResource_;
    D3D12_VERTEX_BUFFER_VIEW batchVbView_{};
    D3D12_INDEX_BUFFER_VIEW batchIbView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> batchWvpResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> batchMaterialResource_;
    TransformationMatrix* batchWvpData_ = nullptr;
    Material* batchMaterialData_ = nullptr;
};
