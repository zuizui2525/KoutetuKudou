#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>
#include <vector>
#include <memory>

class GameObject;
class FlatRingObject;
class CylinderObject;
class TextRenderer2DComponent;

/// <summary>
/// 個々の敵スポーン地点を定義するスクリプトコンポーネント。
/// GameObject の Transform（位置・回転）をそのまま出現位置として利用し、
/// エディタ上で移動ギズモを用いて直感的に配置・調整可能。
/// また、ゲーム内でのルート選択用ハザードゾーン（半透明リング）および
/// エディタ用の強調可視化（シリンダー・トリガー境界線・接続線）を自動描画します。
/// </summary>
class EnemySpawnPointScript : public ScriptComponent {
public:
    EnemySpawnPointScript();
    ~EnemySpawnPointScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 自機の座標がスポーン円の感知範囲（またはトリガーライン）に達しているか判定し、
    /// 条件を満たしていれば SpawnEnemies() を実行します。
    /// </summary>
    /// <param name="playerPos">自機の現在座標 (XYZ)</param>
    /// <param name="currentArea">現在のエリア番号</param>
    /// <returns>スポーンが実行された場合 true</returns>
    bool CheckAndTrigger(const Vector3& playerPos, int currentArea);
    bool CheckAndTrigger(float playerPosZ, int currentArea); // 後方互換性用

    /// <summary>
    /// エディタ可視化および衝突判定で使用する有効半径 (m) を取得します。
    /// </summary>
    float GetEffectiveRadius() const;

    /// <summary>
    /// 指定されたプレハブ（Enemy / BossEnemy）を自身の座標周囲に指定数（count_）生成します。
    /// </summary>
    /// <returns>生成された敵GameObjectのポインタリスト</returns>
    std::vector<GameObject*> SpawnEnemies();

    /// <summary>
    /// エリア切り替え時やリスタート時に、出現済みフラグ（hasTriggered_）をリセットします。
    /// </summary>
    void ResetTrigger() { hasTriggered_ = false; }

    // ゲッター / セッター
    const std::string& GetPrefabName() const { return prefabName_; }
    void SetPrefabName(const std::string& name) { prefabName_ = name; }

    float GetTriggerZ() const { return triggerZ_; }
    void SetTriggerZ(float z) { triggerZ_ = z; }

    int GetCount() const { return count_; }
    void SetCount(int count) { count_ = count; }

    float GetSpreadRadius() const { return spreadRadius_; }
    void SetSpreadRadius(float radius) { spreadRadius_ = radius; }

    float GetSpawnRadius() const { return spawnRadius_; }
    void SetSpawnRadius(float radius) { spawnRadius_ = radius; }

    int GetTargetArea() const { return targetArea_; }
    void SetTargetArea(int area) { targetArea_ = area; }

    bool HasTriggered() const { return hasTriggered_; }

    bool IsBoss() const { return isBoss_; }
    void SetBoss(bool boss);

    bool IsShowHazardZone() const { return showHazardZone_; }
    void SetShowHazardZone(bool show) { showHazardZone_ = show; }

    float GetCylinderHeight() const { return cylinderHeight_; }
    void SetCylinderHeight(float h) { cylinderHeight_ = h; }

    bool IsUseCircleRadiusTrigger() const { return useCircleRadiusTrigger_; }
    void SetUseCircleRadiusTrigger(bool use) { useCircleRadiusTrigger_ = use; }

    float GetTriggerMarginRadius() const { return triggerMarginRadius_; }
    void SetTriggerMarginRadius(float margin) { triggerMarginRadius_ = margin; }

    /// <summary>
    /// 現在の敵数とボス設定に応じた定性的テキスト（少・普・多、または [★BOSS]）を取得します。
    /// </summary>
    std::string GetSpawnCountLevelText() const;

    /// <summary>
    /// 敵種類に応じたビジュアルカラーセット
    /// </summary>
    struct HazardZoneColorTheme {
        Vector4 ringEditor;     // エディタ時地面外周リング色
        Vector4 cylinderEditor; // エディタ時立体警戒円柱色
        Vector4 discEditor;     // エディタ時地面塗りつぶし円盤色
        Vector4 ringPlay;       // ゲーム中地面外周リング色
        Vector4 discPlay;       // ゲーム中地面塗りつぶし円盤色
        Vector4 textColor;      // 頭上テキスト色
    };

    /// <summary>
    /// 現在のプレハブ名・ボス属性に応じたカラーテーマを取得します。
    /// </summary>
    HazardZoneColorTheme GetVisualColorTheme() const;

private:
    void EnsureVisuals();
    void UpdateVisuals(bool isGamePlay);
    void DrawVisuals(bool isGamePlay);
    void UpdateText2D(bool isGamePlay);

private:
    // マジックナンバー排除のための定数
    static constexpr int kMinSpawnCount = 1;
    static constexpr int kMaxSpawnCount = 5;
    static constexpr int kFewCountThreshold = 1;       // 1体以下: [少]
    static constexpr int kMediumCountThreshold = 3;    // 2〜3体: [普] (4体以上: [多])

    static constexpr float kMinSpawnRadius = 1.0f;
    static constexpr float kMaxSpawnRadius = 30.0f;
    static constexpr float kDefaultSpawnRadius = 4.0f;           // スポーン・侵入判定のデフォルト半径 (m)
    static constexpr float kMinSpreadRadius = 0.0f;
    static constexpr float kMaxSpreadRadius = 20.0f;
    static constexpr float kDefaultSpreadRadius = 3.0f;
    static constexpr float kDefaultTriggerDistanceAhead = 25.0f; // スポーン地点の手前何mでトリガーするか
    static constexpr float kDefaultCylinderHeight = 3.5f;
    static constexpr float kGroundRingElevation = 0.05f;         // Z-Fighting防止の床面微小浮遊
    static constexpr float kDefaultTriggerMarginRadius = 0.5f;   // 円形トリガー感知マージン (m)

    static inline const std::string kDefaultEnemyPrefab = "Enemy";
    static inline const std::string kBossEnemyPrefab = "BossEnemy";
    static inline const std::string kFastEnemyPrefab = "FastEnemy";
    static inline const std::string kShooterEnemyPrefab = "Shooter";
    static inline const std::string kHeavyEnemyPrefab = "HeavyEnemy";

    // メンバ変数（インスペクター調整可能）
    std::string prefabName_ = kDefaultEnemyPrefab;
    float triggerZ_ = 0.0f;
    int count_ = 1;
    float spawnRadius_ = kDefaultSpawnRadius;   // 円の半径（エディタ上のサイズおよび自機侵入判定の基準半径）
    float spreadRadius_ = kDefaultSpreadRadius; // 複数体出現時の配置分散半径
    int targetArea_ = 0;
    bool isBoss_ = false;
    bool hasTriggered_ = false;
    bool autoSyncTriggerZWithPosition_ = true; // TransformのZ座標に合わせてトリガーZを自動同期
    bool showHazardZone_ = true;               // ゾーン可視化の有効フラグ
    bool useCircleRadiusTrigger_ = true;       // 円形エリア侵入で敵を出現させる（円に入ってないのに沸くのを防止）
    float triggerMarginRadius_ = kDefaultTriggerMarginRadius; // 感知マージン
    float cylinderHeight_ = kDefaultCylinderHeight;
    float visualRotationAngle_ = 0.0f;

    // 3D可視化Primitive（地面塗りつぶし円盤ディスク・外周リング・立体警戒円柱）
    std::unique_ptr<FlatRingObject> groundDisc_;
    std::unique_ptr<FlatRingObject> groundRing_;
    std::unique_ptr<CylinderObject> warningCylinder_;

    // 敵情報表示用 Text2D (TextRenderer2DComponent)
    TextRenderer2DComponent* text2D_ = nullptr;
};
