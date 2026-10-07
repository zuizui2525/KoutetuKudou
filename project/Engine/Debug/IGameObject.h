#pragma once
#include <string>

/// <summary>
/// 全てのゲームオブジェクトがデバッグUI（ヒエラルキー／インスペクター）と連携するための共通インターフェース
/// </summary>
class IGameObject {
public:
    virtual ~IGameObject(); // デストラクタで自動登録解除

    // オブジェクト名の取得・設定
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name);

    // 表示・非表示フラグの取得・設定
    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }

    // インスペクター描画用（各派生オブジェクトでデバッグUIコードを呼び出す）
    virtual void DrawInspector() = 0;

    // 更新処理の共通インターフェース（通常フレーム更新用）
    virtual void Update() {}

    // エディタ／ポーズ中の更新処理（行列再計算や表示同期のみ行い、アニメーションは停止）
    virtual void UpdateEditor() { Update(); }

protected:
    // 派生クラスの Initialize() で呼び出す初期化ヘルパー
    void InitializeGameObject(const std::string& defaultName);

    std::string name_;
    bool isVisible_ = true;
};
