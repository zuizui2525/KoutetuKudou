#pragma once
#include <string>

class GameObject;

/// <summary>
/// Unity / Unreal Engine スタイルのコンポーネント基底インターフェース
/// </summary>
class IComponent {
public:
    virtual ~IComponent() = default;

    /// <summary>
    /// コンポーネント生成時・初期化時の処理
    /// </summary>
    virtual void Initialize() {}

    /// <summary>
    /// 毎フレームの更新処理
    /// </summary>
    virtual void Update() {}

    /// <summary>
    /// 3D描画処理
    /// </summary>
    virtual void Draw() {}

    /// <summary>
    /// 2D描画処理
    /// </summary>
    virtual void Draw2D() {}

    /// <summary>
    /// インスペクターUI描画処理（コンポーネント独自のパラメータ編集）
    /// </summary>
    virtual void DrawInspector() = 0;

    /// <summary>
    /// コンポーネントの型識別名（シリアライズ・デバッグ用）
    /// </summary>
    virtual std::string GetComponentTypeName() const = 0;

    /// <summary>
    /// 有効・無効状態の取得・設定
    /// </summary>
    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }

    /// <summary>
    /// オーナー（所属するGameObject）の取得・設定
    /// </summary>
    void SetOwner(GameObject* owner) { owner_ = owner; }
    GameObject* GetOwner() const { return owner_; }

protected:
    GameObject* owner_ = nullptr;
    bool isActive_ = true;
};
