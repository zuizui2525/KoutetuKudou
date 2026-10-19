#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>
#include <utility>
#include <unordered_map>

class GameObject;

/// <summary>
/// UnityのMonoBehaviourに相当するユーザースクリプトコンポーネント基底クラス
/// </summary>
class ScriptComponent : public IComponent {
public:
    ScriptComponent() = default;
    explicit ScriptComponent(std::string scriptName) : scriptName_(std::move(scriptName)) {}
    ~ScriptComponent() override = default;

    /// <summary>
    /// プレハブから動的にGameObjectを実体化（Unityライクな動的生成API）
    /// </summary>
    static GameObject* Instantiate(
        const std::string& prefabName,
        const Vector3& position = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f }
    );

    /// <summary>
    /// GameObjectを安全に現在のシーンから破棄（Unityライクな破棄API）
    /// </summary>
    static void Destroy(GameObject* gameObject);

    /// <summary>
    /// 画面演出用カメラシェイク（画面揺れ）を発生させるヘルパー
    /// </summary>
    static void ShakeCamera(float intensity, float duration);

    /// <summary>
    /// コンポーネント生成直後に1度だけ呼ばれる処理
    /// </summary>
    virtual void Awake() {}

    /// <summary>
    /// 初回更新フレーム直前に1度だけ呼ばれる処理
    /// </summary>
    virtual void Start() {}

    /// <summary>
    /// Start() を二重実行しないよう安全に呼び出す
    /// </summary>
    void CallStart() {
        if (!hasStarted_) {
            hasStarted_ = true;
            Start();
        }
    }

    /// <summary>
    /// 初期化（IComponent::Initialize のオーバーライド）
    /// </summary>
    void Initialize() override {
        Awake();
    }

    /// <summary>
    /// 毎フレームの更新処理
    /// </summary>
    void Update() override {
        CallStart();
        OnUpdate();
    }

    /// <summary>
    /// 毎フレームのスクリプト更新ロジック（派生クラスでオーバーライド）
    /// </summary>
    virtual void OnUpdate() {}

    /// <summary>
    /// コンポーネント破棄時の処理
    /// </summary>
    virtual void OnDestroy() {}

    /// <summary>
    /// インスペクターUI描画処理
    /// </summary>
    void DrawInspector() override;

    /// <summary>
    /// 派生スクリプト固有のインスペクター描画（必要に応じてオーバーライド）
    /// </summary>
    virtual void OnInspectorGUI() {}

    /// <summary>
    /// ゲーム画面上のHUD / UI描画処理（UnityのOnGUI相当、ImGuiフレーム内で実行）
    /// </summary>
    virtual void OnGUI() {}

    /// <summary>
    /// コンポーネントの型識別名（シリアライズ用）
    /// </summary>
    std::string GetComponentTypeName() const override { return "ScriptComponent"; }

    /// <summary>
    /// スクリプトクラス名の取得・設定
    /// </summary>
    const std::string& GetScriptName() const { return scriptName_; }
    void SetScriptName(const std::string& name) { scriptName_ = name; }

    /// <summary>
    /// シーンロード後の2パス目参照解決（UUIDから生ポインタを復元）
    /// </summary>
    virtual void ResolveReferences(const std::unordered_map<std::string, GameObject*>& uuidMap);

    /// <summary>
    /// インスペクター用：GameObject参照スロット描画ヘルパー
    /// Hierarchy等からオブジェクトをドラッグ＆ドロップしてバインド可能
    /// </summary>
    static bool DrawGameObjectSlot(const char* label, std::string& targetUUID, GameObject*& targetObj);

    /// <summary>
    /// 汎用ターゲットオブジェクトの取得・設定
    /// </summary>
    const std::string& GetTargetUUID() const { return targetUUID_; }
    void SetTargetUUID(const std::string& uuid) { targetUUID_ = uuid; }
    GameObject* GetTargetObject() const { return targetObject_; }
    void SetTargetObject(GameObject* obj);

protected:
    std::string scriptName_ = "ScriptComponent";
    bool hasStarted_ = false;

    // スクリプト同士の参照用（UUIDと生ポインタ）
    std::string targetUUID_{};
    GameObject* targetObject_ = nullptr;
};
