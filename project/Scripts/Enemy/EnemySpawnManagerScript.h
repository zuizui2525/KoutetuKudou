#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <vector>
#include <string>

class EnemySpawnPointScript;
class GameObject;

/// <summary>
/// シーン上に配置された敵スポーン地点（EnemySpawnPointScript）の一括統括、
/// エディタ上でのワンクリック敵配置・一覧管理・プレビュー、および走行中の出現判定を実行するスクリプト。
/// </summary>
class EnemySpawnManagerScript : public ScriptComponent {
public:
    EnemySpawnManagerScript();
    ~EnemySpawnManagerScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;
    void OnGUI() override;

    /// <summary>
    /// 独立した敵配置パレットウィンドウ（ImGui）を開閉します。
    /// </summary>
    /// <param name="open">開く場合は true</param>
    void SetEditorWindowOpen(bool open) { isEditorWindowOpen_ = open; }
    bool IsEditorWindowOpen() const { return isEditorWindowOpen_; }

    /// <summary>
    /// エリア切り替え時やリセット時に、シーン上の全スポーンポイントの出現フラグをリセットします。
    /// </summary>
    /// <param name="startZ">現在のエリアのスタートZ座標</param>
    void ResetSpawns(float startZ = 0.0f);

    /// <summary>
    /// 現在進行中のエリア番号（0〜3）を設定します。
    /// </summary>
    /// <param name="areaIndex">エリアインデックス</param>
    void SetCurrentArea(int areaIndex) { currentArea_ = areaIndex; }
    int GetCurrentArea() const { return currentArea_; }

    /// <summary>
    /// シーン上に存在するすべての EnemySpawnPointScript を探索して内部リストを更新します。
    /// </summary>
    void RefreshSpawnPoints();

    /// <summary>
    /// 指定エリア・座標に新しい敵スポーン地点 GameObject を生成し、シーンへ登録して選択状態にします。
    /// </summary>
    /// <param name="spawnPos">出現ワールド座標</param>
    /// <param name="areaIndex">対象エリア番号</param>
    /// <param name="isBoss">ボス敵の場合は true</param>
    /// <returns>生成されたGameObjectのポインタ</returns>
    GameObject* CreateSpawnPoint(const Vector3& spawnPos, int areaIndex, bool isBoss = false);

    /// <summary>
    /// シーン上の配置済みスポーン地点に基づいて、テスト用の敵を一括生成して目視プレビューします。
    /// </summary>
    void PreviewAllSpawns();

    /// <summary>
    /// テスト生成された敵プレビューオブジェクトを一括消去します。
    /// </summary>
    void ClearPreviewEnemies();

private:
    /// <summary>
    /// 自機の現在Z座標に基づき、配置済みスポーン地点の通過判定を実行します。
    /// </summary>
    void CheckSpawnPoints(float playerPosZ);

    /// <summary>
    /// スポーン地点が未配置のエリア用のフォールバック定期湧き判定を実行します。
    /// </summary>
    void CheckPeriodicFallback(const Vector3& playerPos);

    /// <summary>
    /// 独立した敵配置パレット（ImGui）の内部GUIを描画します。
    /// </summary>
    void DrawPaletteUI();

    /// <summary>
    /// エディタ用カメラ（SceneViewカメラ）の注視点を指定オブジェクトの位置へスムーズにフォーカスします。
    /// </summary>
    /// <param name="targetObj">注視対象のGameObject</param>
    void FocusCameraOnObject(GameObject* targetObj);

private:
    // マジックナンバー排除のための定数
    static constexpr float kDefaultPaletteWidth = 420.0f;  // 独立パレットの初期幅
    static constexpr float kDefaultPaletteHeight = 560.0f; // 独立パレットの初期高さ
    static constexpr float kSpawnListChildHeight = 240.0f; // スポーン一覧テーブルの高さ
    static constexpr float kReopenButtonHeight = 32.0f;    // パレット再表示ボタンの高さ
    static constexpr float kCameraFocusElevation = 5.0f;   // フォーカス時のカメラ高さオフセット
    static constexpr float kCameraFocusDistance = 14.0f;   // フォーカス時のカメラ手前距離
    static constexpr float kDefaultSpawnIntervalZ = 30.0f; // 定期湧き間隔（Z距離）
    static constexpr float kDefaultBossSpawnZ = 360.0f;    // ボス出現ラインZ
    static constexpr int kBossAreaIndex = 3;               // ボスが存在するエリアインデックス
    static constexpr float kSpawnOffsetBack = -10.0f;      // 自機背後スポーンオフセット
    static constexpr float kSpawnOffsetSide = 10.0f;      // 左右オフセット
    static constexpr float kBossSpawnDistanceAhead = 50.0f;// ボス前方スポーン距離
    static constexpr float kAreaLengths[4] = { 120.0f, 240.0f, 360.0f, 480.0f }; // 各エリアの終端Z座標

    // メンバ変数
    float spawnIntervalZ_ = kDefaultSpawnIntervalZ;
    float lastSpawnZ_ = 0.0f;
    float bossSpawnZ_ = kDefaultBossSpawnZ;
    bool hasBossSpawned_ = false;
    int currentArea_ = 0;
    bool enableSpawning_ = true;
    bool enablePeriodicFallback_ = false; // スポーンポイント未配置時の自動定期湧き（デフォルトOFF）
    bool isEditorWindowOpen_ = true;      // 独立した敵配置パレットの開閉状態（デフォルトON）

    // シーンから収集したスポーンポイント参照キャッシュ
    std::vector<EnemySpawnPointScript*> cachedSpawnPoints_;
    int selectedEditorAreaTab_ = 0;
};
