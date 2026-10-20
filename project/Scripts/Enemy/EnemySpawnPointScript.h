#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>
#include <vector>

class GameObject;

/// <summary>
/// 個々の敵スポーン地点を定義するスクリプトコンポーネント。
/// GameObject の Transform（位置・回転）をそのまま出現位置として利用し、
/// エディタ上で移動ギズモを用いて直感的に配置・調整可能。
/// </summary>
class EnemySpawnPointScript : public ScriptComponent {
public:
    EnemySpawnPointScript();
    ~EnemySpawnPointScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 自機のZ座標がトリガーライン（triggerZ_）に達しているか判定し、
    /// 条件を満たしていれば SpawnEnemies() を実行します。
    /// </summary>
    /// <param name="playerPosZ">自機の現在Z座標</param>
    /// <param name="currentArea">現在のエリア番号</param>
    /// <returns>スポーンが実行された場合 true</returns>
    bool CheckAndTrigger(float playerPosZ, int currentArea);

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

    int GetTargetArea() const { return targetArea_; }
    void SetTargetArea(int area) { targetArea_ = area; }

    bool HasTriggered() const { return hasTriggered_; }

    bool IsBoss() const { return isBoss_; }
    void SetBoss(bool boss);

private:
    // マジックナンバー排除のための定数
    static constexpr int kMinSpawnCount = 1;
    static constexpr int kMaxSpawnCount = 5;
    static constexpr float kMinSpreadRadius = 0.0f;
    static constexpr float kMaxSpreadRadius = 20.0f;
    static constexpr float kDefaultSpreadRadius = 3.0f;
    static constexpr float kDefaultTriggerDistanceAhead = 25.0f; // スポーン地点の手前何mでトリガーするか
    static inline const std::string kDefaultEnemyPrefab = "Enemy";
    static inline const std::string kBossEnemyPrefab = "BossEnemy";

    // メンバ変数（インスペクター調整可能）
    std::string prefabName_ = kDefaultEnemyPrefab;
    float triggerZ_ = 0.0f;
    int count_ = 1;
    float spreadRadius_ = kDefaultSpreadRadius;
    int targetArea_ = 0;
    bool isBoss_ = false;
    bool hasTriggered_ = false;
    bool autoSyncTriggerZWithPosition_ = true; // TransformのZ座標に合わせてトリガーZを自動同期
};
