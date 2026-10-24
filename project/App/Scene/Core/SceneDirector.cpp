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

    // --- AntiGravity風モダンUI インスペクター定数 (マジックナンバー排除) ---
    // カード背景色: ウィンドウ背景(0.06f)よりさらに深く沈み込んだ漆黒ダーク（引き締まった外観）
    constexpr ImVec4 kModernCardBg = { 0.035f, 0.035f, 0.045f, 0.98f };
    // カード外枠ボーダー: 繊細でシャープな1px暗色ボーダー
    constexpr ImVec4 kModernCardBorder = { 0.140f, 0.150f, 0.180f, 0.85f };
    // カード角丸
    constexpr float kModernCardRounding = 3.0f;
    // カード内部パディング
    constexpr float kCardPaddingX = 4.0f; // 内部インデントを詰めてコントロール有効幅を拡大
    constexpr float kCardPaddingY = 6.0f;
    constexpr float kCardSpacingY = 8.0f;
    constexpr float kCardExpandX = 3.0f;  // カード枠を左右へダイナミックにワイド化

    // コントロール文字飛び出し防止＆引き延ばし連動用プロポーショナル幅定数 (マジックナンバー排除)
    constexpr float kComponentItemWidthRatio = 0.45f; // 利用可能幅の45%をコントロールに割り当て、55%をラベル領域に確保
    constexpr float kComponentItemMinWidth = 100.0f;  // 最小スライダー幅
    constexpr float kComponentItemMaxWidth = 200.0f;  // 最大スライダー幅（引き延ばした際にラベルスペースが広大に広がる）

    // 一体化ヘッダー（タイトルバー）背景色: 白浮き・青灰色感を完全に排除した落ち着いた超ダーク
    constexpr ImVec4 kModernHeaderBg = { 0.070f, 0.075f, 0.090f, 1.0f };
    constexpr ImVec4 kModernHeaderHovered = { 0.110f, 0.115f, 0.135f, 1.0f };
    constexpr ImVec4 kModernHeaderActive = { 0.140f, 0.145f, 0.170f, 1.0f };

    // アクセントバー定数
    constexpr float kScriptAccentBarWidth = 2.5f;
    constexpr ImVec4 kScriptAccentBar = { 0.28f, 0.45f, 0.65f, 0.85f }; // スクリプト用控えめなスレートブルー



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
            script->CallStart();
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

        std::string sName = script->GetScriptName();
        if (sName.empty()) {
            sName = script->GetComponentTypeName();
        }
        std::string headerName = "[Script] " + sName;

        // 前景チャンネル（ヘッダーと内部コンテンツ）
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImVec2 cardStartPos = ImGui::GetCursorScreenPos();
        float availWidth = ImGui::GetContentRegionAvail().x;

        // 一体化ヘッダー背景色（落ち着いた超ダークに完全統一）
        ImGui::PushStyleColor(ImGuiCol_Header, kModernHeaderBg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kModernHeaderHovered);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, kModernHeaderActive);

        bool isScriptAlive = true;
        bool isHeaderOpen = ImGui::CollapsingHeader(headerName.c_str(), &isScriptAlive, ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::PopStyleColor(3);

        if (!isScriptAlive) {
            removeIdx = static_cast<int>(i);
        }

        if (isHeaderOpen && isScriptAlive) {
            ImGui::Spacing();
            ImGui::Indent(kCardPaddingX);

            ImGui::BeginGroup();

            bool active = script->IsActive();
            if (ImGui::Checkbox("Enabled##ScriptActive", &active)) {
                script->SetActive(active);
            }
            ImGui::Spacing();

            // スクリプト独自のインスペクターUI呼び出し（引き延ばしに連動してラベル領域が拡大し文字見切れを完全根絶）
            float compItemWidth = std::clamp(availWidth * kComponentItemWidthRatio, kComponentItemMinWidth, kComponentItemMaxWidth);
            ImGui::PushItemWidth(compItemWidth);
            script->DrawInspector();
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::EndGroup();

            ImGui::Unindent(kCardPaddingX);

            // ヘッダーの最上部（cardStartPos.y）からコンテンツ最下部まで一体のカード枠を描画
            ImVec2 cardEndPos = ImGui::GetItemRectMax();
            ImVec2 rectMin = ImVec2(cardStartPos.x - kCardExpandX, cardStartPos.y);
            ImVec2 rectMax = ImVec2(cardStartPos.x + availWidth + kCardExpandX, cardEndPos.y + kCardPaddingY * 0.5f);

            // 背景チャンネルに切り替えて、コントロールの背面に確実に描画
            drawList->ChannelsSetCurrent(0);

            drawList->AddRectFilled(rectMin, rectMax, ImGui::ColorConvertFloat4ToU32(kModernCardBg), kModernCardRounding);
            drawList->AddRect(rectMin, rectMax, ImGui::ColorConvertFloat4ToU32(kModernCardBorder), kModernCardRounding);
            drawList->AddRectFilled(
                rectMin,
                ImVec2(rectMin.x + kScriptAccentBarWidth, rectMax.y),
                ImGui::ColorConvertFloat4ToU32(kScriptAccentBar),
                kModernCardRounding,
                ImDrawFlags_RoundCornersLeft
            );

            ImGui::Spacing();
        }

        drawList->ChannelsMerge();
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
