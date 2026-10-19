#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 走行距離やZ座標進行に応じた敵（通常敵・ボス）の動的スポーンを管理するスクリプト
/// </summary>
class EnemySpawnManagerScript : public ScriptComponent {
public:
    EnemySpawnManagerScript();
    ~EnemySpawnManagerScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

    void ResetSpawns(float startZ = 0.0f);
    void SetCurrentArea(int areaIndex) { currentArea_ = areaIndex; }

private:
    void CheckPeriodicSpawn(const Vector3& playerPos);
    void CheckBossSpawn(const Vector3& playerPos);

private:
    // マジックナンバー排除のための定数
    static constexpr float kDefaultSpawnIntervalZ = 30.0f; // 定期湧き間隔（Z距離）
    static constexpr float kDefaultBossSpawnZ = 360.0f;    // ボス出現ラインZ
    static constexpr int kBossAreaIndex = 3;               // ボスが存在するエリアインデックス（0起点でエリア4）
    static constexpr float kSpawnOffsetBack = -10.0f;      // 自機背後スポーンオフセット
    static constexpr float kSpawnOffsetSide = 10.0f;      // 左右オフセット
    static constexpr float kBossSpawnDistanceAhead = 50.0f;// ボス前方スポーン距離

    // メンバ変数
    float spawnIntervalZ_ = kDefaultSpawnIntervalZ;
    float lastSpawnZ_ = 0.0f;
    float bossSpawnZ_ = kDefaultBossSpawnZ;
    bool hasBossSpawned_ = false;
    int currentArea_ = 0;
    bool enableSpawning_ = true;
};
