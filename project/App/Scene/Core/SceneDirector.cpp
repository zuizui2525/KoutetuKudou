#include "App/Scene/Core/SceneDirector.h"
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include <algorithm>
#include <format>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // スタイル定数 (マジックナンバー排除)
    constexpr float kAddBtnHeight = 26.0f;
    constexpr ImVec4 kHeaderColor = { 0.4f, 0.75f, 1.0f, 1.0f };
    constexpr ImVec4 kSubtextColor = { 0.7f, 0.7f, 0.7f, 1.0f };

    void SyncGPUForDeletion() {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
    }
}

SceneDirector::SceneDirector(Scene* ownerScene)
    : ownerScene_(ownerScene) {
}

SceneDirector::~SceneDirector() {
    SyncGPUForDeletion();
    scripts_.clear();
}

void SceneDirector::Initialize() {
    for (auto& script : scripts_) {
        if (script) {
            script->Awake();
        }
    }
    for (auto& script : scripts_) {
        if (script) {
            script->Start();
        }
    }
}

void SceneDirector::Update() {
    isUpdating_ = true;
    for (size_t i = 0; i < scripts_.size(); ++i) {
        if (scripts_[i] && scripts_[i]->IsActive()) {
            scripts_[i]->Update();
        }
    }
    isUpdating_ = false;

    // 遅延追加キューの処理
    if (!pendingAddScripts_.empty()) {
        for (auto& script : pendingAddScripts_) {
            if (script) {
                script->Awake();
                script->Start();
                scripts_.push_back(std::move(script));
            }
        }
        pendingAddScripts_.clear();
    }

    // 遅延削除キューの処理
    if (!pendingRemoveIndices_.empty()) {
        SyncGPUForDeletion();
        // 降順にソートしてインデックスズレを防止
        std::sort(pendingRemoveIndices_.rbegin(), pendingRemoveIndices_.rend());
        for (size_t idx : pendingRemoveIndices_) {
            if (idx < scripts_.size()) {
                scripts_.erase(scripts_.begin() + idx);
            }
        }
        pendingRemoveIndices_.clear();
    }
}

void SceneDirector::Draw() {
    for (auto& script : scripts_) {
        if (script && script->IsActive()) {
            script->Draw();
        }
    }
}

void SceneDirector::Draw2D() {
    for (auto& script : scripts_) {
        if (script && script->IsActive()) {
            script->Draw2D();
        }
    }
}

void SceneDirector::AddScript(std::unique_ptr<ScriptComponent> script) {
    if (!script) return;
    if (isUpdating_) {
        pendingAddScripts_.push_back(std::move(script));
    } else {
        script->Awake();
        script->Start();
        scripts_.push_back(std::move(script));
    }
}

void SceneDirector::RemoveScriptByIndex(size_t index) {
    if (index >= scripts_.size()) return;
    if (isUpdating_) {
        pendingRemoveIndices_.push_back(index);
    } else {
        SyncGPUForDeletion();
        scripts_.erase(scripts_.begin() + index);
    }
}

void SceneDirector::DrawInspector() {
#ifdef _USEIMGUI
    ImGui::TextColored(kHeaderColor, "Scene Director (Level Scripts)");
    ImGui::TextColored(kSubtextColor, "シーン全体のルール・フェーズ・進行管理スクリプト群");
    ImGui::Separator();

    int removeIdx = -1;
    for (size_t i = 0; i < scripts_.size(); ++i) {
        auto& script = scripts_[i];
        if (!script) continue;

        ImGui::PushID(static_cast<int>(i));

        std::string headerName = script->GetScriptName();
        if (headerName.empty()) {
            headerName = script->GetComponentTypeName();
        }

        bool isScriptAlive = true;
        bool isHeaderOpen = ImGui::CollapsingHeader(headerName.c_str(), &isScriptAlive, ImGuiTreeNodeFlags_DefaultOpen);

        if (!isScriptAlive) {
            removeIdx = static_cast<int>(i);
        }

        if (isHeaderOpen && isScriptAlive) {
            bool active = script->IsActive();
            if (ImGui::Checkbox("Active##ScriptActive", &active)) {
                script->SetActive(active);
            }
            ImGui::Spacing();

            // スクリプト独自のインスペクターUI呼び出し
            script->DrawInspector();
            ImGui::Spacing();
        }

        ImGui::PopID();
    }

    if (removeIdx >= 0) {
        RemoveScriptByIndex(static_cast<size_t>(removeIdx));
    }

    ImGui::Separator();
    ImGui::Spacing();

    // [+ Add Director Script] ボタン
    if (ImGui::Button("+ Add Director Script", ImVec2(-1.0f, kAddBtnHeight))) {
        ImGui::OpenPopup("AddDirectorScriptPopup");
    }

    if (ImGui::BeginPopup("AddDirectorScriptPopup")) {
        ImGui::TextDisabled("-- Select Script --");
        ImGui::Separator();

        auto registeredScripts = ScriptRegistry::GetInstance().GetRegisteredScriptNames();
        for (const auto& sName : registeredScripts) {
            if (ImGui::MenuItem(sName.c_str())) {
                auto newScript = ScriptRegistry::GetInstance().CreateScript(sName);
                if (newScript) {
                    AddScript(std::move(newScript));
                }
            }
        }
        ImGui::EndPopup();
    }
#endif
}
