#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

class ScriptComponent;

/// <summary>
/// スクリプトクラスの文字列登録・動的インスタンス化を管理するレジストリ
/// </summary>
class ScriptRegistry {
public:
    using FactoryFunc = std::function<std::unique_ptr<ScriptComponent>()>;

    /// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
    static ScriptRegistry& GetInstance();

    /// <summary>
    /// スクリプトファクトリの登録
    /// </summary>
    void RegisterScript(const std::string& name, FactoryFunc factory);

    /// <summary>
    /// スクリプト名からインスタンスを生成
    /// </summary>
    std::unique_ptr<ScriptComponent> CreateScript(const std::string& name);

    /// <summary>
    /// 登録済みスクリプト名の一覧を取得
    /// </summary>
    std::vector<std::string> GetRegisteredScriptNames() const;

    /// <summary>
    /// 指定名のスクリプトが登録されているか確認
    /// </summary>
    bool IsRegistered(const std::string& name) const;

private:
    ScriptRegistry() = default;
    ~ScriptRegistry() = default;
    ScriptRegistry(const ScriptRegistry&) = delete;
    ScriptRegistry& operator=(const ScriptRegistry&) = delete;

    std::unordered_map<std::string, FactoryFunc> factories_;
};

/// <summary>
/// 自動登録ヘルパークラス（静的初期化子でレジストリに登録）
/// </summary>
template <typename T>
class ScriptAutoRegister {
public:
    explicit ScriptAutoRegister(const std::string& name) {
        ScriptRegistry::GetInstance().RegisterScript(name, [name]() -> std::unique_ptr<ScriptComponent> {
            auto script = std::make_unique<T>();
            script->SetScriptName(name);
            return script;
        });
    }
};

/// <summary>
/// スクリプト登録マクロ（各スクリプトの.cppファイル末尾等に記述）
/// </summary>
#define REGISTER_SCRIPT(ClassName) \
    namespace { \
        const ScriptAutoRegister<ClassName> g_auto_reg_##ClassName(#ClassName); \
    }
