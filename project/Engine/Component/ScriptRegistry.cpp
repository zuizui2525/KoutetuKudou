#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/ScriptComponent.h"
#include <algorithm>

ScriptRegistry& ScriptRegistry::GetInstance() {
    static ScriptRegistry instance;
    return instance;
}

void ScriptRegistry::RegisterScript(const std::string& name, FactoryFunc factory) {
    factories_[name] = std::move(factory);
}

std::unique_ptr<ScriptComponent> ScriptRegistry::CreateScript(const std::string& name) {
    auto it = factories_.find(name);
    if (it != factories_.end() && it->second) {
        return it->second();
    }

    // 登録されていない場合はデフォルトのScriptComponentとして生成
    auto fallback = std::make_unique<ScriptComponent>(name);
    return fallback;
}

std::vector<std::string> ScriptRegistry::GetRegisteredScriptNames() const {
    std::vector<std::string> names;
    names.reserve(factories_.size());
    for (const auto& [name, _] : factories_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool ScriptRegistry::IsRegistered(const std::string& name) const {
    return factories_.contains(name);
}
