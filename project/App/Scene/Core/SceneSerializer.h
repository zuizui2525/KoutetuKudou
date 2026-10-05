#pragma once
#include <string>
#include <vector>
#include <memory>

class GameObject;

/// <summary>
/// Unity/Unreal スタイルのシーンデータ（GameObjectおよび可変長コンポーネント配列）のJSONシリアライザ
/// </summary>
class SceneSerializer {
public:
    SceneSerializer() = default;
    ~SceneSerializer() = default;

    /// <summary>
    /// シーン状態をJSONファイルへ保存 (unique_ptr配列版)
    /// </summary>
    /// <param name="filePath">保存先ファイルパス (例: resources/Scenes/Debug.json)</param>
    /// <param name="sceneName">シーン識別名</param>
    /// <param name="gameObjects">保存対象のGameObject一覧</param>
    /// <returns>保存に成功したか</returns>
    static bool SaveScene(
        const std::string& filePath,
        const std::string& sceneName,
        const std::vector<std::unique_ptr<GameObject>>& gameObjects
    );

    /// <summary>
    /// シーン状態をJSONファイルへ保存 (生ポインタ配列版)
    /// </summary>
    /// <param name="filePath">保存先ファイルパス</param>
    /// <param name="sceneName">シーン識別名</param>
    /// <param name="gameObjects">保存対象のGameObject一覧</param>
    /// <returns>保存に成功したか</returns>
    static bool SaveScene(
        const std::string& filePath,
        const std::string& sceneName,
        const std::vector<GameObject*>& gameObjects
    );

    /// <summary>
    /// JSONファイルからシーン状態を読み込み復元
    /// </summary>
    /// <param name="filePath">読み込み元ファイルパス</param>
    /// <param name="outSceneName">復元されたシーン名</param>
    /// <param name="outGameObjects">復元されたGameObject一覧</param>
    /// <returns>読み込みに成功したか</returns>
    static bool LoadScene(
        const std::string& filePath,
        std::string& outSceneName,
        std::vector<std::unique_ptr<GameObject>>& outGameObjects
    );

    /// <summary>
    /// 動的コンポーネントの保存・復元の単体自動テストを実行
    /// </summary>
    /// <returns>テストが成功したか</returns>
    static bool RunSelfTest();
};
