#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "Engine/Math/MathStructs.h"

class GameObject;

/// <summary>
/// Unityスタイルのプレハブ（Prefab）管理クラス（シングルトン）
/// GameObjectのテンプレート保存・読み込み、および新規UUID採番による動的インスタンス化を提供
/// </summary>
class PrefabManager {
public:
    static PrefabManager& GetInstance();

    /// <summary>
    /// GameObject の現在の状態を Prefab ファイル（resources/Prefabs/<Name>.prefab）として保存
    /// </summary>
    /// <param name="filePath">保存先ファイルパス</param>
    /// <param name="gameObject">保存対象のGameObject</param>
    /// <returns>保存に成功したか</returns>
    bool SavePrefab(const std::string& filePath, GameObject* gameObject);

    /// <summary>
    /// プレハブ名（例: "Enemy" や "Bullet"）またはパスから新しい GameObject インスタンスを生成
    /// 新規の一意な UUID が自動採番されます
    /// </summary>
    /// <param name="prefabNameOrPath">プレハブ名またはファイルパス</param>
    /// <param name="position">初期配置座標</param>
    /// <param name="rotation">初期回転角（度またはラジアン）</param>
    /// <returns>生成された GameObject インスタンス（失敗時は nullptr）</returns>
    std::unique_ptr<GameObject> Instantiate(
        const std::string& prefabNameOrPath,
        const Vector3& position = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f }
    );

    /// <summary>
    /// プレハブ格納フォルダパスの取得（resources/Prefabs）
    /// </summary>
    static const std::string& GetPrefabsDirectory();

    /// <summary>
    /// プレハブメモリキャッシュのクリア
    /// </summary>
    void ClearCache();

private:
    PrefabManager() = default;
    ~PrefabManager() = default;
    PrefabManager(const PrefabManager&) = delete;
    PrefabManager& operator=(const PrefabManager&) = delete;

    std::string ResolvePrefabPath(const std::string& nameOrPath) const;
    std::string SerializeGameObject(GameObject* gameObject) const;
    std::unique_ptr<GameObject> DeserializeGameObject(const std::string& jsonContent) const;

    // プレハブ定義のメモリキャッシュ (パス -> JSON文字列)
    std::unordered_map<std::string, std::string> prefabCache_;
};
