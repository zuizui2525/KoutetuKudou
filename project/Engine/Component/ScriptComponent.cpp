#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Debug/SceneHierarchy.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

void ScriptComponent::SetTargetObject(GameObject* obj) {
    targetObject_ = obj;
    targetUUID_ = obj ? obj->GetUUID() : "";
}

void ScriptComponent::ResolveReferences(const std::unordered_map<std::string, GameObject*>& uuidMap) {
    if (targetUUID_.empty()) {
        targetObject_ = nullptr;
        return;
    }
    auto it = uuidMap.find(targetUUID_);
    if (it != uuidMap.end()) {
        targetObject_ = it->second;
    } else {
        targetObject_ = nullptr;
    }
}

void ScriptComponent::DrawInspector() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.4f, 0.7f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "Script: %s", scriptName_.c_str());
    ImGui::Separator();

    // デフォルトで汎用ターゲットスロットを表示
    DrawGameObjectSlot("Target Object", targetUUID_, targetObject_);
    ImGui::Spacing();

    // 派生クラス独自のUIを描画
    OnInspectorGUI();
#endif
}

bool ScriptComponent::DrawGameObjectSlot(const char* label, std::string& targetUUID, GameObject*& targetObj) {
#ifdef _USEIMGUI
    ImGui::PushID(label);

    ImGui::Text("%s", label);
    ImGui::SameLine();

    std::string displayName = targetObj ? targetObj->GetName() : "(None / Drag GameObject Here)";
    if (targetObj) {
        displayName += " [GameObject]";
    }

    constexpr ImVec4 kSlotNormalBg = { 0.18f, 0.18f, 0.22f, 0.8f };
    constexpr ImVec4 kSlotHoveredBg = { 0.28f, 0.38f, 0.55f, 0.9f };
    constexpr ImVec4 kAssignedColor = { 0.4f, 0.85f, 0.5f, 1.0f };
    constexpr ImVec4 kUnassignedColor = { 0.6f, 0.6f, 0.6f, 0.8f };
    constexpr float kClearBtnWidth = 22.0f;
    constexpr float kSlotHeight = 22.0f;

    ImGui::PushStyleColor(ImGuiCol_Button, kSlotNormalBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSlotHoveredBg);
    ImGui::PushStyleColor(ImGuiCol_Text, targetObj ? kAssignedColor : kUnassignedColor);

    float availWidth = ImGui::GetContentRegionAvail().x;
    float slotWidth = (std::max)(80.0f, availWidth - kClearBtnWidth - 6.0f);

    bool changed = false;
    std::string btnId = displayName + "##Slot";
    if (ImGui::Button(btnId.c_str(), ImVec2(slotWidth, kSlotHeight))) {
        if (targetObj) {
            SceneHierarchy::GetInstance()->SetSelected(targetObj);
        }
    }

    ImGui::PopStyleColor(3);

    // ドラッグ＆ドロップ受け取り (SceneHierarchyからのDND)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT_UUID")) {
            const char* droppedUUID = static_cast<const char*>(payload->Data);
            targetUUID = droppedUUID;

            // 即時ポインタ解決
            targetObj = nullptr;
            for (auto* obj : SceneHierarchy::GetInstance()->GetObjects()) {
                if (auto* go = dynamic_cast<GameObject*>(obj)) {
                    if (go->GetUUID() == targetUUID) {
                        targetObj = go;
                        break;
                    }
                }
            }
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    // クリアボタン [x]
    ImGui::SameLine();
    if (ImGui::Button("x##ClearSlot", ImVec2(kClearBtnWidth, kSlotHeight))) {
        targetUUID.clear();
        targetObj = nullptr;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("参照を解除します");
    }

    ImGui::PopID();
    return changed;
#else
    (void)label; (void)targetUUID; (void)targetObj;
    return false;
#endif
}

#include "App/Scene/Core/SceneManager.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"

GameObject* ScriptComponent::Instantiate(
    const std::string& prefabName,
    const Vector3& position,
    const Vector3& rotation
) {
    return SceneManager::GetInstance()->Instantiate(prefabName, position, rotation);
}

void ScriptComponent::Destroy(GameObject* gameObject) {
    if (gameObject) {
        SceneManager::GetInstance()->Destroy(gameObject);
    }
}

void ScriptComponent::ShakeCamera(float intensity, float duration) {
    if (auto* cameraMgr = CameraResource::GetCameraManager()) {
        cameraMgr->TriggerShake(intensity, duration);
    }
}

