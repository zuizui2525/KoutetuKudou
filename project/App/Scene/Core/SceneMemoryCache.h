#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// <summary>
/// Unity/Unreal Engine スタイルのエディタ一時セッションキャッシュ管理クラス（シングルトン）
/// シーン切り替え時の編集内容をメモリ上に一時保持し、ディスクファイルを汚さずに安全なテスト実行を可能にする。
/// エディタ終了時に「保存しない」を選んだ場合は本キャッシュを全破棄することでディスクを元の状態に保つ。
/// </summary>
class SceneMemoryCache {
public:
    static SceneMemoryCache* GetInstance();

    /// <summary>
    /// シーンの一時シリアライズ文字列をメモリに保存
    /// </summary>
    void SetCache(const std::string& sceneName, const std::string& jsonString);

    /// <summary>
    /// キャッシュされたシーンのJSON文字列を取得（存在しない場合は空文字列）
    /// </summary>
    std::string GetCache(const std::string& sceneName) const;

    /// <summary>
    /// 指定したシーンの一時キャッシュが存在するか
    /// </summary>
    bool HasCache(const std::string& sceneName) const;

    /// <summary>
    /// 指定したシーンに変更（ダーティ）があるか
    /// </summary>
    bool IsDirty(const std::string& sceneName) const;

    /// <summary>
    /// いずれかのシーンに変更が保持されているか
    /// </summary>
    bool HasAnyDirty() const;

    /// <summary>
    /// 特定シーンのキャッシュを破棄
    /// </summary>
    void ClearCache(const std::string& sceneName);

    /// <summary>
    /// すべての一時キャッシュを破棄（「保存しない」で終了する際に呼び出す）
    /// </summary>
    void ClearAll();

    /// <summary>
    /// メモリに一時保持されている変更済みシーンをすべてディスク（.json）へ正式書き出し
    /// </summary>
    /// <param name="scenesDirectory">シーン保存先フォルダ</param>
    /// <returns>全ファイル保存成功したか</returns>
    bool SaveAllDirtyToDisk(const std::string& scenesDirectory = "resources/Scenes");

private:
    SceneMemoryCache() = default;
    ~SceneMemoryCache() = default;
    SceneMemoryCache(const SceneMemoryCache&) = delete;
    SceneMemoryCache& operator=(const SceneMemoryCache&) = delete;

    // シーン名 -> 一時JSON文字列
    std::unordered_map<std::string, std::string> cacheMap_;

    // ディスク未保存の変更があるシーン名一覧
    std::unordered_set<std::string> dirtyScenes_;
};
