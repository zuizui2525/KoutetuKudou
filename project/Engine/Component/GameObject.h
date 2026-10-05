#pragma once
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include "Engine/Debug/IGameObject.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Component/IComponent.h"

/// <summary>
/// Unity / Unreal Engine スタイルの汎用エンティティクラス
/// 任意のコンポーネントを着脱・保持できるコンテナとして機能
/// </summary>
class GameObject : public IGameObject {
public:
    explicit GameObject(const std::string& name = "GameObject");
    ~GameObject() override;

    /// <summary>
    /// 初期化処理
    /// </summary>
    virtual void Initialize();

    /// <summary>
    /// 毎フレームの更新処理（全アクティブコンポーネントを更新）
    /// </summary>
    void Update() override;

    /// <summary>
    /// 3D描画処理（描画系コンポーネントを呼び出し）
    /// </summary>
    virtual void Draw();

    /// <summary>
    /// 2D描画処理（2D描画系コンポーネントを呼び出し）
    /// </summary>
    virtual void Draw2D();

    /// <summary>
    /// インスペクターGUI描画（Transform編集、全コンポーネント描画、Add Componentメニュー）
    /// </summary>
    void DrawInspector() override;

    /// <summary>
    /// ワールド行列の更新
    /// </summary>
    void UpdateMatrix();

    // ========================================================
    // トランスフォーム操作
    // ========================================================
    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }

    Vector3& GetPosition() { return transform_.translate; }
    const Vector3& GetPosition() const { return transform_.translate; }
    void SetPosition(const Vector3& pos) { transform_.translate = pos; isMatrixDirty_ = true; }

    Vector3& GetRotate() { return transform_.rotate; }
    const Vector3& GetRotate() const { return transform_.rotate; }
    void SetRotate(const Vector3& rot) { transform_.rotate = rot; isMatrixDirty_ = true; }

    Vector3& GetScale() { return transform_.scale; }
    const Vector3& GetScale() const { return transform_.scale; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; isMatrixDirty_ = true; }

    const Matrix4x4& GetWorldMatrix() const { return matWorld_; }

    // ========================================================
    // コンポーネント管理 (UnityライクなテンプレートAPI)
    // ========================================================

    /// <summary>
    /// コンポーネントを新規追加
    /// </summary>
    template <typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of<IComponent, T>::value, "T must derive from IComponent");
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->SetOwner(this);
        comp->Initialize();
        T* rawPtr = comp.get();
        components_.push_back(std::move(comp));
        return rawPtr;
    }

    /// <summary>
    /// 指定型のコンポーネントを取得
    /// </summary>
    template <typename T>
    T* GetComponent() const {
        static_assert(std::is_base_of<IComponent, T>::value, "T must derive from IComponent");
        for (const auto& comp : components_) {
            if (auto casted = dynamic_cast<T*>(comp.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    /// <summary>
    /// 指定型のコンポーネントが存在するか確認
    /// </summary>
    template <typename T>
    bool HasComponent() const {
        return GetComponent<T>() != nullptr;
    }

    /// <summary>
    /// 指定型のコンポーネントを削除
    /// </summary>
    template <typename T>
    bool RemoveComponent() {
        static_assert(std::is_base_of<IComponent, T>::value, "T must derive from IComponent");
        auto it = std::find_if(components_.begin(), components_.end(), [](const std::unique_ptr<IComponent>& comp) {
            return dynamic_cast<T*>(comp.get()) != nullptr;
        });
        if (it != components_.end()) {
            RemoveComponentByIndex(static_cast<size_t>(std::distance(components_.begin(), it)));
            return true;
        }
        return false;
    }

    /// <summary>
    /// 登録されている全コンポーネント一覧を取得
    /// </summary>
    const std::vector<std::unique_ptr<IComponent>>& GetComponents() const { return components_; }

    /// <summary>
    /// インデックス指定でコンポーネントを削除（GPU同期を経て安全に解放）
    /// </summary>
    void RemoveComponentByIndex(size_t index);

private:
    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Matrix4x4 matWorld_{};
    bool isMatrixDirty_ = true;

    std::vector<std::unique_ptr<IComponent>> components_;
};
