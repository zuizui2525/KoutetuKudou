#pragma once

#include "Engine/Component/ScriptComponent.h"
#include "Scripts/Stage/MapData.h"
#include <string>
#include <vector>

class GameObject;

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
    void OnDestroy() override;
    void OnInspectorGUI() override;

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

private:
    /// <summary>
    /// mapGrid_ に基づいてシーン上にビルのGameObject群を展開します。
    /// </summary>
    void BuildMapGeometry();

    /// <summary>
    /// JSONファイルからマップデータ（map設定およびbuildings座標配列）をパースします。
    /// </summary>
    bool ParseMapJson(const std::string& jsonString);

private:
    static constexpr size_t kMaxPathBufferLength = 256;

    // パラメータ
    std::string mapAssetPath_ = "resources/Maps/Stage1_Area0.json";
    bool generateColliders_ = true;
    bool isMapLoaded_ = false;

    // グリッドデータ
    MapGrid mapGrid_;

    // 生成済みビルGameObject群の参照リスト
    std::vector<GameObject*> generatedBuildings_;
};
