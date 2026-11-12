#pragma once

#include "Engine/Component/ScriptComponent.h"
#include "Scripts/Stage/MapData.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>

class GameObject;
class CubeObject;

/// <summary>
/// マップアセット（JSON）を読み込み、シーン上にビル群およびコライダーを一括展開するスクリプトコンポーネント。
/// エフェクトコンポーネントと同様にGameObjectへアタッチして使用し、MapStudioでの編集連携に対応します。
/// </summary>
class MapRendererScript : public ScriptComponent {
public:
    MapRendererScript();
    ~MapRendererScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnDestroy() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// シーン上でアクティブな MapRendererScript のインスタンスを取得します。
    /// </summary>
    static MapRendererScript* GetInstance() { return sInstance_; }

    /// <summary>
    /// シーン上でアクティブな MapRendererScript の全インスタンス一覧を取得します。
    /// </summary>
    static const std::vector<MapRendererScript*>& GetAllInstances() { return sInstances_; }

    /// <summary>
    /// マップJSONアセットを指定して読み込み、ビル群をシーン上に展開します。
    /// </summary>
    /// <param name="filePath">マップアセットのパス (例: resources/Maps/Stage1_Area0.json)</param>
    /// <returns>読み込み・構築に成功したか</returns>
    bool LoadMapAsset(const std::string& filePath);

    /// <summary>
    /// 現在展開されているビル群オブジェクトをシーンから全削除します。
    /// </summary>
    void ClearMapGeometry();

    /// <summary>
    /// 現在読み込まれているグリッドデータを取得します。
    /// </summary>
    const MapGrid& GetMapGrid() const { return mapGrid_; }

    /// <summary>
    /// マップアセットのパスを取得・設定します。
    /// </summary>
    const std::string& GetMapAssetPath() const { return mapAssetPath_; }
    void SetMapAssetPath(const std::string& path) { mapAssetPath_ = path; }

    /// <summary>
    /// マップデータが正常にロードされているかを取得します。
    /// </summary>
    bool IsMapLoaded() const { return isMapLoaded_; }

    /// <summary>
    /// コライダーの自動付与を有効・無効化します。
    /// </summary>
    void SetGenerateColliders(bool enable) { generateColliders_ = enable; }
    bool IsGenerateColliders() const { return generateColliders_; }

    /// <summary>
    /// マップ全体のワールドバウンディング情報（最小・最大座標）を取得します。
    /// </summary>
    Vector3 GetMapWorldMin() const;
    Vector3 GetMapWorldMax() const;

    /// <summary>
    /// 指定された3Dワールド座標がビル（建物）の内部（またはマージン半径内）にあるかを O(1) で判定します。
    /// </summary>
    /// <param name="worldPos">判定対象の3Dワールド座標</param>
    /// <param name="radius">判定マージン半径（m）</param>
    /// <returns>建物内部にある場合 true</returns>
    static bool IsInsideBuilding(const Vector3& worldPos, float radius = 0.0f);

    /// <summary>
    /// 指定されたワールド座標が建物内（または衝突半径内）にある場合、建物の外側（道路方向）へ脱出するための押し返しベクトルを計算します。
    /// </summary>
    /// <param name="worldPos">現在のワールド座標</param>
    /// <param name="radius">キャラクターの衝突半径（m）</param>
    /// <returns>押し返しベクトル（非衝突時はゼロベクトル）</returns>
    static Vector3 GetBuildingPushout(const Vector3& worldPos, float radius);

private:
    /// <summary>
    /// mapGrid_ に基づいてビル群の静的バッチメッシュおよび境界コライダーを構築します。
    /// </summary>
    void BuildMapGeometry();

    /// <summary>
    /// 全ビルの直方体頂点を結合し、単一の頂点バッファへアップロードします（ドローコール極限削減）。
    /// </summary>
    void RebuildBuildingMesh();

    /// <summary>
    /// GPU頂点バッファおよびバッチ描画オブジェクトを遅延解放キューで安全に破棄します。
    /// </summary>
    void ReleaseMeshResources();

    /// <summary>
    /// コース外脱落防止用の外枠境界コライダーを生成します。
    /// </summary>
    void BuildBoundaryColliders();

    /// <summary>
    /// 生成済みの外枠境界コライダーを全破棄します。
    /// </summary>
    void ClearBoundaryColliders();

    /// <summary>
    /// JSONファイルからマップデータ（map設定およびbuildings座標配列）をパースします。
    /// </summary>
    bool ParseMapJson(const std::string& jsonString);

private:
    static constexpr size_t kMaxPathBufferLength = 256;
    static constexpr float kAreaSpanZ = 120.0f;       // 1エリアの標準Z長
    static constexpr int kMaxVisibleAreaDelta = 1;     // 距離カリング許容エリア差 (現在エリア±1エリアのみ描画)

    // パラメータ
    std::string mapAssetPath_ = "resources/Maps/Stage1_Area0.json";
    bool generateColliders_ = true;
    bool isMapLoaded_ = false;

    // グリッドデータ
    MapGrid mapGrid_;

    // 静的バッチ結合メッシュ（全ビルを1回のDrawコールで一括描画）
    Microsoft::WRL::ComPtr<ID3D12Resource> buildingVbResource_;
    D3D12_VERTEX_BUFFER_VIEW buildingVbView_{};
    uint32_t buildingVertexCount_ = 0;
    std::unique_ptr<CubeObject> batchCubeObject_;
    bool isMeshDirty_ = false;

    // コース境界コライダーGameObject群（左右各1個、計2個のみ生成）
    std::vector<GameObject*> boundaryColliders_;

    // 旧個別ビルオブジェクト互換用（破棄クリーンアップ用）
    std::vector<GameObject*> generatedBuildings_;

    // シングルトンインスタンス参照および全インスタンス追跡
    static inline MapRendererScript* sInstance_ = nullptr;
    static inline std::vector<MapRendererScript*> sInstances_;
};
