#ifdef _USEIMGUI
#include "Engine/Debug/DebugEditor.h"
#include "Engine/Debug/SceneViewWindow.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Debug/PerformanceMonitorWindow.h"
#include "Engine/Debug/ContentBrowserWindow.h"
#include "Engine/Debug/ParticleStudioWindow.h"
#include "Engine/Debug/EngineHelpData.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Debug/IGameObject.h"
#include "Engine/Zuizui.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Base/Log/Log.h"
#include "externals/imgui/imgui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/PrefabManager.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Component/Components/TextRenderer3DComponent.h"
#include "App/Scene/Core/SceneSerializer.h"
#include "App/Scene/Core/SceneMemoryCache.h"
#include "App/Scene/Core/Scene.h"
#include "App/Scene/Core/SceneDirector.h"
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Generic/GenericScene.h"
#include "App/Scene/Core/SceneTransitionManager.h"
#include "Engine/Debug/Command/CommandHistory.h"
#include "Engine/Debug/Command/CreateGameObjectCommand.h"
#include "Engine/Debug/Command/DeleteGameObjectCommand.h"
#include "Engine/Debug/EditorNotification.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <filesystem>
#include <format>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
    const std::string kScenesDirectory = "resources/Scenes";
    constexpr const char* kDedicatedSceneGame = "Game";
    constexpr const char* kTransitionsConfigFileName = "Transitions";
    constexpr const char* kWorkspacesConfigFilePath = "resources/Config/Workspaces.json";
    constexpr const char* kDockSpacePrefix = "Zuizui_WS_v2_";

    // JSONからキーの真偽値を安全に抽出
    bool ExtractJsonBool(const std::string& block, const std::string& key, bool defaultVal) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        size_t valStart = pos + searchKey.length();
        while (valStart < block.length() && (block[valStart] == ' ' || block[valStart] == '\t')) {
            ++valStart;
        }
        if (block.substr(valStart, 4) == "true" || block.substr(valStart, 1) == "1") return true;
        if (block.substr(valStart, 5) == "false" || block.substr(valStart, 1) == "0") return false;
        return defaultVal;
    }

    // JSONからキーの文字列を安全に抽出
    std::string ExtractJsonString(const std::string& block, const std::string& key, const std::string& defaultVal = "") {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;

        size_t quoteStart = block.find('\"', pos + searchKey.length());
        if (quoteStart == std::string::npos) return defaultVal;

        size_t quoteEnd = block.find('\"', quoteStart + 1);
        if (quoteEnd == std::string::npos) return defaultVal;

        return block.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }

    // JSONからキーの整数値を安全に抽出
    int ExtractJsonInt(const std::string& block, const std::string& key, int defaultVal = 0) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;

        size_t valStart = pos + searchKey.length();
        while (valStart < block.length() && (block[valStart] == ' ' || block[valStart] == '\t')) {
            ++valStart;
        }
        try {
            return std::stoi(block.substr(valStart));
        } catch (...) {
            return defaultVal;
        }
    }

    // トランジション設定（IN: 入場演出 / OUT: 退場演出）表示用定数 (マジックナンバー排除)
    constexpr ImVec4 kTransitionColorIn   = ImVec4(0.2f, 0.8f, 1.0f, 1.0f); // シアン（IN）
    constexpr ImVec4 kTransitionColorOut  = ImVec4(1.0f, 0.6f, 0.2f, 1.0f); // オレンジ（OUT）
    constexpr float  kTransitionStarSpacingX = 4.0f;                         // 星アイコンの水平余白
    constexpr const char* kTransitionStarSymbol = "*";                       // 星アイコン記号


    // シーン削除モーダルID、フォールバックシーン名、最低シーン保持数 (マジックナンバー・ストリング排除)
    constexpr const char* kDeleteSceneModalId = "DeleteSceneConfirmModal";
    const std::string kFallbackPrioritySceneName = "Title";
    constexpr size_t kMinSceneCount = 1;

    // Prefab ドラッグ＆ドロップ用定数 (マジックナンバー・ストリング排除)
    constexpr const char* kPayloadPrefab = "CONTENT_BROWSER_PREFAB";
    constexpr float kMinDropAreaHeight = 32.0f;

    // シーン削除モーダル用ステート変数
    static std::string sSceneNameToDelete = "";
    static bool sTriggerOpenDeleteSceneModal = false;

    // エディタ用アイコン定数 (マジックナンバー・ストリング排除)
    constexpr const char* kIconEyeOpenKey    = "sys_icon_eye_open";
    constexpr const char* kIconEyeClosedKey  = "sys_icon_eye_closed";
    constexpr const char* kIconLockClosedKey = "sys_icon_lock_closed";
    constexpr const char* kIconLockOpenKey   = "sys_icon_lock_open";

    constexpr const char* kIconEyeOpenPath    = "resources/Textures/Icons/icon_eye_open.png";
    constexpr const char* kIconEyeClosedPath  = "resources/Textures/Icons/icon_eye_closed.png";
    constexpr const char* kIconLockClosedPath = "resources/Textures/Icons/icon_lock_closed.png";
    constexpr const char* kIconLockOpenPath   = "resources/Textures/Icons/icon_lock_open.png";

    static bool sEditorIconsLoaded = false;
    void EnsureEditorIconsLoaded() {
        if (sEditorIconsLoaded) return;
        if (auto texMgr = TextureResource::GetTextureManager()) {
            texMgr->LoadTexture(kIconEyeOpenKey, kIconEyeOpenPath);
            texMgr->LoadTexture(kIconEyeClosedKey, kIconEyeClosedPath);
            texMgr->LoadTexture(kIconLockClosedKey, kIconLockClosedPath);
            texMgr->LoadTexture(kIconLockOpenKey, kIconLockOpenPath);
            sEditorIconsLoaded = true;
        }
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetEditorIconHandle(const char* key) {
        if (auto texMgr = TextureResource::GetTextureManager()) {
            if (texMgr->HasTexture(key)) {
                return texMgr->GetGpuHandle(key);
            }
        }
        D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{};
        nullHandle.ptr = 0;
        return nullHandle;
    }

    // 南京錠（レイアウト固定）トグルボタンのPNG画像＆ベクターフォールバック描画ヘルパー (マジックナンバー排除)
    bool RenderLockToggleButton(bool& isLocked) {
        EnsureEditorIconsLoaded();

        constexpr float kBtnWidth = 26.0f;
        constexpr float kRounding = 3.0f;
        constexpr ImVec4 kLockedBgColor   = { 0.22f, 0.44f, 0.72f, 1.0f }; // ロック時: アクセントブルー
        constexpr ImVec4 kUnlockedBgColor = { 0.16f, 0.17f, 0.19f, 0.85f }; // アンロック時: ダークグレー

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRounding);
        ImGui::PushStyleColor(ImGuiCol_Button, isLocked ? kLockedBgColor : kUnlockedBgColor);
        
        bool clicked = ImGui::Button("##LockLayoutBtn", ImVec2(kBtnWidth, 0.0f));
        if (clicked) {
            isLocked = !isLocked;
        }

        // ボタン領域にアイコンを描画
        ImVec2 rectMin = ImGui::GetItemRectMin();
        ImVec2 rectMax = ImGui::GetItemRectMax();
        ImVec2 center = { (rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f };
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        constexpr float kIconDisplaySize = 14.0f;
        auto handle = GetEditorIconHandle(isLocked ? kIconLockClosedKey : kIconLockOpenKey);
        if (handle.ptr != 0) {
            ImVec2 iconMin = { center.x - kIconDisplaySize * 0.5f, center.y - kIconDisplaySize * 0.5f };
            ImVec2 iconMax = { center.x + kIconDisplaySize * 0.5f, center.y + kIconDisplaySize * 0.5f };
            ImU32 tintColor = isLocked ? IM_COL32(255, 255, 255, 255) : IM_COL32(205, 210, 220, 255);
            drawList->AddImage((ImTextureID)handle.ptr, iconMin, iconMax, ImVec2(0, 0), ImVec2(1, 1), tintColor);
        } else {
            // フォールバックベクター描画
            constexpr float kBodyWidth = 11.5f;
            constexpr float kBodyHeight = 8.0f;
            constexpr float kBodyRounding = 1.5f;
            constexpr float kShackleHalfW = 3.4f;
            constexpr float kShackleRadius = 3.4f;
            constexpr float kShackleThickness = 1.8f;
            constexpr float kPi = 3.14159265358979323846f;
            constexpr int kArcSegments = 16;
            constexpr float kKeyHoleRadius = 1.2f;
            constexpr float kKeyHoleSlotW = 1.0f;
            constexpr float kKeyHoleSlotH = 2.0f;

            const ImU32 kIconColor = isLocked ? IM_COL32(255, 255, 255, 255) : IM_COL32(205, 210, 220, 255);
            const ImU32 kHoleColor = isLocked ? IM_COL32(35, 75, 135, 255) : IM_COL32(30, 32, 36, 255);

            ImVec2 bodyMin = { center.x - kBodyWidth * 0.5f, center.y - 0.5f };
            ImVec2 bodyMax = { center.x + kBodyWidth * 0.5f, center.y - 0.5f + kBodyHeight };

            drawList->PathClear();
            if (isLocked) {
                ImVec2 arcCenter = { center.x, center.y - 3.8f };
                drawList->PathLineTo({ center.x - kShackleHalfW, bodyMin.y + 1.2f });
                drawList->PathLineTo({ center.x - kShackleHalfW, arcCenter.y });
                drawList->PathArcTo(arcCenter, kShackleRadius, kPi, 0.0f, kArcSegments);
                drawList->PathLineTo({ center.x + kShackleHalfW, arcCenter.y });
                drawList->PathLineTo({ center.x + kShackleHalfW, bodyMin.y + 1.2f });
                drawList->PathStroke(kIconColor, 0, kShackleThickness);
            } else {
                constexpr float kLiftY = 3.0f;
                ImVec2 arcCenter = { center.x, center.y - 3.8f - kLiftY };
                drawList->PathLineTo({ center.x - kShackleHalfW, bodyMin.y - 0.5f });
                drawList->PathLineTo({ center.x - kShackleHalfW, arcCenter.y });
                drawList->PathArcTo(arcCenter, kShackleRadius, kPi, 0.0f, kArcSegments);
                drawList->PathLineTo({ center.x + kShackleHalfW, arcCenter.y });
                drawList->PathLineTo({ center.x + kShackleHalfW, arcCenter.y + 2.0f });
                drawList->PathStroke(kIconColor, 0, kShackleThickness);
            }

            drawList->AddRectFilled(bodyMin, bodyMax, kIconColor, kBodyRounding);
            ImVec2 holeCenter = { center.x, bodyMin.y + kBodyHeight * 0.42f };
            drawList->AddCircleFilled(holeCenter, kKeyHoleRadius, kHoleColor);
            drawList->AddRectFilled({ holeCenter.x - kKeyHoleSlotW * 0.5f, holeCenter.y },
                                    { holeCenter.x + kKeyHoleSlotW * 0.5f, holeCenter.y + kKeyHoleSlotH },
                                    kHoleColor);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(isLocked ? "レイアウト固定中 (境界サイズ変更をロック中 / クリックで解除)" : "レイアウト可動 (クリックで境界サイズを固定)");
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        return clicked;
    }

    // ヒエラルキー用：Blender風フレームレス目アイコン描画ボタン (マジックナンバー排除)
    bool RenderEyeVisibilityButton(const char* strId, bool isVisible) {
        EnsureEditorIconsLoaded();

        float frameHeight = ImGui::GetFrameHeight();
        // アイコンの横幅と描画サイズ (マジックナンバー排除)
        constexpr float kIconBtnWidth = 18.0f;
        constexpr float kIconDisplaySize = 14.0f;

        // ボタンのクリック幅は kIconBtnWidth、高さは行の高さ frameHeight に合わせる
        ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::InvisibleButton(strId, ImVec2(kIconBtnWidth, frameHeight));
        bool hovered = ImGui::IsItemHovered();

        // Selectable テキスト（Camera等）の視覚的中心とアイコンの天地中央を一致させるための垂直オフセット (マジックナンバー排除)
        constexpr float kVerticalAlignOffsetY = -1.5f;

        ImVec2 rectMin = ImGui::GetItemRectMin();
        ImVec2 rectMax = ImGui::GetItemRectMax();
        ImVec2 center = { (rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f + kVerticalAlignOffsetY };
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Blender風：アイコン自体をホバー時に高輝度発光させる（行全体の背景ハイライトと一体化）
        const ImU32 eyeTintColor = isVisible
            ? (hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 222, 235, 230)) // 表示時: 通常はソフトホワイト、ホバー時はクリアホワイトに発光
            : (hovered ? IM_COL32(190, 195, 205, 240) : IM_COL32(100, 105, 115, 160)); // 非表示時: 通常は控えめなダークグレー、ホバー時はライトグレー

        auto handle = GetEditorIconHandle(isVisible ? kIconEyeOpenKey : kIconEyeClosedKey);
        if (handle.ptr != 0) {
            ImVec2 iconMin = { center.x - kIconDisplaySize * 0.5f, center.y - kIconDisplaySize * 0.5f };
            ImVec2 iconMax = { center.x + kIconDisplaySize * 0.5f, center.y + kIconDisplaySize * 0.5f };
            drawList->AddImage((ImTextureID)handle.ptr, iconMin, iconMax, ImVec2(0, 0), ImVec2(1, 1), eyeTintColor);
        } else {
            // フォールバックベクター描画
            constexpr float kEyeWidthRatio = 0.58f;
            constexpr float kEyeHeightRatio = 0.28f;
            constexpr float kStrokeThickness = 1.5f;
            float eyeHalfW = kIconDisplaySize * kEyeWidthRatio * 0.5f;
            float eyeH = kIconDisplaySize * kEyeHeightRatio;

            if (isVisible) {
                ImVec2 pL = { center.x - eyeHalfW, center.y };
                ImVec2 pR = { center.x + eyeHalfW, center.y };
                ImVec2 cTop1 = { center.x - eyeHalfW * 0.5f, center.y - eyeH };
                ImVec2 cTop2 = { center.x + eyeHalfW * 0.5f, center.y - eyeH };
                ImVec2 cBot1 = { center.x - eyeHalfW * 0.5f, center.y + eyeH };
                ImVec2 cBot2 = { center.x + eyeHalfW * 0.5f, center.y + eyeH };

                drawList->AddBezierCubic(pL, cTop1, cTop2, pR, eyeTintColor, kStrokeThickness);
                drawList->AddBezierCubic(pL, cBot1, cBot2, pR, eyeTintColor, kStrokeThickness);
                drawList->AddCircleFilled(center, eyeH * 0.45f, eyeTintColor);
            } else {
                ImVec2 pL = { center.x - eyeHalfW, center.y };
                ImVec2 pR = { center.x + eyeHalfW, center.y };
                ImVec2 cBot1 = { center.x - eyeHalfW * 0.5f, center.y + eyeH * 0.5f };
                ImVec2 cBot2 = { center.x + eyeHalfW * 0.5f, center.y + eyeH * 0.5f };
                drawList->AddBezierCubic(pL, cBot1, cBot2, pR, eyeTintColor, kStrokeThickness);

                ImVec2 slashStart = { center.x - eyeHalfW * 0.65f, center.y + eyeH * 0.75f };
                ImVec2 slashEnd   = { center.x + eyeHalfW * 0.65f, center.y - eyeH * 0.75f };
                drawList->AddLine(slashStart, slashEnd, eyeTintColor, kStrokeThickness);
            }
        }

        if (hovered) {
            ImGui::SetTooltip(isVisible ? "表示中 (クリックで非表示)" : "非表示中 (クリックで表示)");
        }

        return clicked;
    }


    // 単一フェーズ（INまたはOUT）のGUI描画ヘルパー
    bool RenderSingleTransitionSettingUI(const char* idPrefix, SingleTransitionSetting& setting, bool isOut) {
        bool changed = false;

        // 1. 有効化チェックボックス
        std::string enableLabel = isOut ? "退場演出 (OUT) を有効にする##" : "入場演出 (IN) を有効にする##";
        enableLabel += idPrefix;
        if (ImGui::Checkbox(enableLabel.c_str(), &setting.enabled)) {
            changed = true;
        }

        if (!setting.enabled) {
            ImGui::TextDisabled(isOut ? "※退場演出は無効です（即座に切り替わります）" : "※入場演出は無効です（即座に出現します）");
            return changed;
        }

        // 2. 種類選択 (Combo)
        const char* effectTypes[] = {
            "なし (None)",
            "カラーフェード (Color Fade)",
            "ディゾルブ (Dissolve)",
            "ラジアルブラー (Radial Blur)",
            "ガウスぼかし (Gaussian Blur)",
            "TVノイズ (TV Noise)",
            "モノクロフェード (Grayscale)",
            "左ワイプ (Wipe Left)",
            "右ワイプ (Wipe Right)"
        };
        int currentType = static_cast<int>(setting.effectType);
        std::string comboLabel = "種類##";
        comboLabel += idPrefix;
        if (ImGui::Combo(comboLabel.c_str(), &currentType, effectTypes, IM_ARRAYSIZE(effectTypes))) {
            setting.effectType = static_cast<TransitionEffectType>(currentType);
            changed = true;
        }

        // 3. 所要秒数 (DragFloat)
        std::string durationLabel = "秒数##";
        durationLabel += idPrefix;
        if (ImGui::DragFloat(durationLabel.c_str(), &setting.duration, 0.05f, TransitionConstants::kMinDuration, TransitionConstants::kMaxDuration, "%.2f 秒")) {
            changed = true;
        }

        // 4. 遷移カラー & 最大透明度 (ColorEdit4)
        float col[4] = { setting.color.x, setting.color.y, setting.color.z, setting.color.w };
        std::string colorLabel = "色・透明度##";
        colorLabel += idPrefix;
        if (ImGui::ColorEdit4(colorLabel.c_str(), col, ImGuiColorEditFlags_AlphaBar)) {
            setting.color = { col[0], col[1], col[2], col[3] };
            changed = true;
        }

        // 5. エフェクト個別パラメータ
        if (setting.effectType == TransitionEffectType::Dissolve) {
            float edgeCol[3] = { setting.edgeColor.x, setting.edgeColor.y, setting.edgeColor.z };
            std::string edgeColorLabel = "発光色##";
            edgeColorLabel += idPrefix;
            if (ImGui::ColorEdit3(edgeColorLabel.c_str(), edgeCol)) {
                setting.edgeColor = { edgeCol[0], edgeCol[1], edgeCol[2] };
                changed = true;
            }
            std::string edgeWidthLabel = "境界幅##";
            edgeWidthLabel += idPrefix;
            if (ImGui::DragFloat(edgeWidthLabel.c_str(), &setting.edgeWidth, 0.01f, 0.01f, 0.2f, "%.2f")) {
                changed = true;
            }
        } else if (setting.effectType == TransitionEffectType::RadialBlur ||
                   setting.effectType == TransitionEffectType::GaussianBlur ||
                   setting.effectType == TransitionEffectType::TVNoise) {
            std::string strengthLabel = "エフェクト強度##";
            strengthLabel += idPrefix;
            if (ImGui::DragFloat(strengthLabel.c_str(), &setting.blurStrength, 0.05f, 0.1f, 3.0f, "%.2f 倍")) {
                changed = true;
            }
        }

        ImGui::Spacing();
        // 6. 単体プレビュー再生ボタン
        std::string previewBtnLabel = isOut ? "▶ OUT 単体プレビュー##" : "▶ IN 単体プレビュー##";
        previewBtnLabel += idPrefix;
        if (ImGui::Button(previewBtnLabel.c_str())) {
            if (isOut) {
                SceneTransitionManager::GetInstance()->TriggerPreviewOut(setting);
            } else {
                SceneTransitionManager::GetInstance()->TriggerPreviewIn(setting);
            }
        }

        return changed;
    }

    // 画面切り替え（トランジション）設定ポップアップの描画ヘルパー (右クリック)
    void RenderSceneTransitionContextMenu(const std::string& sceneName) {
        if (ImGui::BeginPopupContextItem("TransitionContextMenu")) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "画面切り替え設定: %s", sceneName.c_str());
            ImGui::Separator();

            auto transitionMgr = SceneTransitionManager::GetInstance();
            SceneTransitionConfig config = transitionMgr->GetConfig(sceneName);
            bool changed = false;

            // タブバーで OUT（退場）と IN（入場）を明確に分割
            if (ImGui::BeginTabBar("TransitionTabBar")) {
                // OUT タブ（退場演出）: 橙色
                if (ImGui::BeginTabItem("OUT (退場演出)###OutTab")) {
                    ImGui::TextColored(kTransitionColorOut, "【OUT】このシーンから抜ける時の演出");
                    ImGui::Separator();
                    if (RenderSingleTransitionSettingUI("OutSetting", config.outSetting, true)) {
                        changed = true;
                    }
                    ImGui::EndTabItem();
                }

                // IN タブ（入場演出）: 青色/シアン
                if (ImGui::BeginTabItem("IN (入場演出)###InTab")) {
                    ImGui::TextColored(kTransitionColorIn, "【IN】このシーンに入ってくる時の演出");
                    ImGui::Separator();
                    if (RenderSingleTransitionSettingUI("InSetting", config.inSetting, false)) {
                        changed = true;
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            if (changed) {
                transitionMgr->SetConfig(sceneName, config);
            }

            ImGui::Separator();

            // 通しプレビューボタン
            if (ImGui::Button("▶ 通しプレビュー (OUT → IN)")) {
                transitionMgr->TriggerPreviewFull(config.outSetting, config.inSetting);
            }
            ImGui::SameLine();
            if (ImGui::Button("閉じる")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();
            ImGui::Text("【起動シーン設定】");

            auto sceneMgr = SceneManager::GetInstance();
            std::string debugStartup = sceneMgr->GetDebugStartupSceneName();
            std::string releaseStartup = sceneMgr->GetReleaseStartupSceneName();

            bool isDebugStartup = (sceneName == debugStartup);
            bool isReleaseStartup = (sceneName == releaseStartup);

            std::string debugStartupLabel = isDebugStartup
                ? "★ デバッグ起動シーンに設定中"
                : "☆ デバッグ起動シーンに設定";
            if (ImGui::MenuItem(debugStartupLabel.c_str(), nullptr, isDebugStartup)) {
                sceneMgr->SetDebugStartupSceneName(sceneName);
                EditorNotification::ShowSuccess("デバッグ起動シーンを「" + sceneName + "」に設定しました");
            }

            std::string releaseStartupLabel = isReleaseStartup
                ? "★ 本番(Release)起動シーンに設定中"
                : "☆ 本番(Release)起動シーンに設定";
            if (ImGui::MenuItem(releaseStartupLabel.c_str(), nullptr, isReleaseStartup)) {
                sceneMgr->SetReleaseStartupSceneName(sceneName);
                EditorNotification::ShowSuccess("本番起動シーンを「" + sceneName + "」に設定しました");
            }

            ImGui::EndPopup();
        }
    }

    enum class CreationType {
        Empty,
        Cube,
        Sphere,
        Pyramid,
        TriangularPyramid,
        Triangle,
        Square,
        Cylinder,
        Cone,
        Ring,
        Hemisphere,
        Model,
        Line3D,
        FlatRing,
        CylinderEffect,
        Text3D,
        Sprite,
        Triangle2D,
        Circle2D,
        Ring2D,
        Line2D,
        Text2D,
        Camera,
        DirectionalLight,
        PointLight,
        SpotLight,
    };

    // 現在のシーン状態をディスクではなく「メモリ一時キャッシュ」に退避（シーン切り替え時用）
    void CacheCurrentSceneAction(const std::string& currentSceneName) {
        if (currentSceneName.empty() || currentSceneName == "None") return;

        // SceneHierarchy から GameObject を収集
        const auto& allObjects = SceneHierarchy::GetInstance()->GetObjects();
        std::vector<GameObject*> gameObjects;
        for (auto* obj : allObjects) {
            if (auto* go = dynamic_cast<GameObject*>(obj)) {
                gameObjects.push_back(go);
            }
        }

        std::string jsonStr = SceneSerializer::SerializeToString(currentSceneName, gameObjects);
        SceneMemoryCache::GetInstance()->SetCache(currentSceneName, jsonStr);
    }

    // 現在のシーン状態をディスク（.json）へ明示的に正式保存（Ctrl+S またはメニュー保存用）
    void SaveCurrentSceneAction(const std::string& currentSceneName) {
        if (currentSceneName.empty() || currentSceneName == "None") {
            Log::Write(L"[Scene] 有効なシーンが開かれていないため、保存をスキップしました。");
            return;
        }

        std::string scenePath = kScenesDirectory + "/" + currentSceneName + ".json";
        
        // SceneHierarchy から GameObject を収集
        const auto& allObjects = SceneHierarchy::GetInstance()->GetObjects();
        std::vector<GameObject*> gameObjects;
        for (auto* obj : allObjects) {
            if (auto* go = dynamic_cast<GameObject*>(obj)) {
                gameObjects.push_back(go);
            }
        }

        bool success = SceneSerializer::SaveScene(scenePath, currentSceneName, gameObjects);
        if (success) {
            // ディスク保存に成功したため、メモリキャッシュの未保存ダーティ状態を解消
            SceneMemoryCache::GetInstance()->ClearCache(currentSceneName);
            Log::Write(std::format(L"[Scene] 現在のシーン「{}」({} 個の GameObject) を「{}」に正常保存しました！",
                ConvertString(currentSceneName), gameObjects.size(), ConvertString(scenePath)));
            EditorNotification::ShowSuccess("シーン「" + currentSceneName + "」を保存しました");
        } else {
            Log::Write(std::format(L"[Scene] シーン「{}」の保存に失敗しました。",
                ConvertString(currentSceneName)));
            EditorNotification::ShowWarning("シーン「" + currentSceneName + "」の保存に失敗しました");
        }
    }

    // アプリケーション終了時のセーブ確認ダイアログ
    bool ConfirmExitAndSave(HWND hwnd) {
        std::string currentSceneName = SceneManager::GetInstance()->GetCurrentSceneName();

        std::wstring msg = L"エディタでの変更を保存してから終了しますか？\n\n"
                           L"【はい】: 変更を保存して終了\n"
                           L"【いいえ】: 保存せずに終了 (変更を破棄して元通りにする)\n"
                           L"【キャンセル】: 終了を取り消す";
        int result = MessageBoxW(hwnd, msg.c_str(), L"終了の確認 - ZuizuiEngine", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (result == IDYES) {
            // 現在のシーンも一時キャッシュに入れてから全変更をディスクへ保存
            if (!currentSceneName.empty() && currentSceneName != "None") {
                CacheCurrentSceneAction(currentSceneName);
            }
            SceneMemoryCache::GetInstance()->SaveAllDirtyToDisk(kScenesDirectory);
            return true;
        } else if (result == IDNO) {
            // 【いいえ】: 一時キャッシュを全て破棄！ディスクファイルには1文字も触らないため完全元通り！
            SceneMemoryCache::GetInstance()->ClearAll();
            return true;
        }
        return false; // キャンセル時は終了を中断
    }

    // 既存シーン用の静的フォールバック配列（シーン解放まで生存）
    static std::vector<std::unique_ptr<GameObject>> sFallbackObjects;

    void CreateNewGameObjectInCurrentScene(CreationType type = CreationType::Empty) {
        std::string defaultName = "GameObject";
        switch (type) {
        case CreationType::Cube: defaultName = "Cube"; break;
        case CreationType::Sphere: defaultName = "Sphere"; break;
        case CreationType::Pyramid: defaultName = "Pyramid"; break;
        case CreationType::TriangularPyramid: defaultName = "TriangularPyramid"; break;
        case CreationType::Triangle: defaultName = "Triangle"; break;
        case CreationType::Square: defaultName = "Square"; break;
        case CreationType::Cylinder: defaultName = "Cylinder"; break;
        case CreationType::Cone: defaultName = "Cone"; break;
        case CreationType::Ring: defaultName = "Ring"; break;
        case CreationType::Hemisphere: defaultName = "Hemisphere"; break;
        case CreationType::Model: defaultName = "Model"; break;
        case CreationType::Line3D: defaultName = "Line3D"; break;
        case CreationType::FlatRing: defaultName = "FlatRing"; break;
        case CreationType::CylinderEffect: defaultName = "CylinderEffect"; break;
        case CreationType::Text3D: defaultName = "Text3D"; break;
        case CreationType::Sprite: defaultName = "Sprite"; break;
        case CreationType::Triangle2D: defaultName = "Triangle2D"; break;
        case CreationType::Circle2D: defaultName = "Circle2D"; break;
        case CreationType::Ring2D: defaultName = "Ring2D"; break;
        case CreationType::Line2D: defaultName = "Line2D"; break;
        case CreationType::Text2D: defaultName = "Text2D"; break;
        case CreationType::Camera: defaultName = "Camera"; break;
        case CreationType::DirectionalLight: defaultName = "DirectionalLight"; break;
        case CreationType::PointLight: defaultName = "PointLight"; break;
        case CreationType::SpotLight: defaultName = "SpotLight"; break;
        default: defaultName = "GameObject"; break;
        }

        auto currentScene = SceneManager::GetInstance()->GetCurrentScene();
        GameObject* createdObj = nullptr;
        if (auto genericScene = dynamic_cast<GenericScene*>(currentScene)) {
            createdObj = genericScene->CreateGameObject(defaultName);
        } else {
            auto newObj = std::make_unique<GameObject>(defaultName);
            createdObj = newObj.get();
            sFallbackObjects.push_back(std::move(newObj));
        }

        if (createdObj) {
            if (type == CreationType::Sprite || type == CreationType::Triangle2D ||
                type == CreationType::Circle2D || type == CreationType::Ring2D ||
                type == CreationType::Line2D) {
                auto* sr = createdObj->AddComponent<SpriteRendererComponent>();
                if (type == CreationType::Sprite) sr->SetShapeType(SpriteRendererComponent::ShapeType::Sprite);
                else if (type == CreationType::Triangle2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Triangle);
                else if (type == CreationType::Circle2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Circle);
                else if (type == CreationType::Ring2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Ring);
                else if (type == CreationType::Line2D) sr->SetShapeType(SpriteRendererComponent::ShapeType::Line);
                createdObj->Ensure2DPosition();
            } else if (type == CreationType::Text2D) {
                createdObj->AddComponent<TextRenderer2DComponent>();
                createdObj->Ensure2DPosition();
            } else if (type == CreationType::Text3D) {
                createdObj->AddComponent<TextRenderer3DComponent>();
                if (auto cameraMgr = CameraResource::GetCameraManager()) {
                    if (auto* activeCam = cameraMgr->GetActiveCamera()) {
                        Vector3 camPos = activeCam->GetPosition();
                        Vector3 camRot = activeCam->GetRotation();
                        Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
                        Vector3 forward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
                        constexpr float kMinForwardLength = 0.001f;
                        if (Math::Length(forward) < kMinForwardLength) {
                            forward = { 0.0f, 0.0f, 1.0f };
                        }
                        constexpr float kSpawnDistanceForward = 8.0f;
                        Vector3 spawnPos = {
                            camPos.x + forward.x * kSpawnDistanceForward,
                            camPos.y + forward.y * kSpawnDistanceForward,
                            camPos.z + forward.z * kSpawnDistanceForward
                        };
                        createdObj->SetPosition(spawnPos);
                    }
                }
            } else if (type == CreationType::Camera) {
                createdObj->AddComponent<CameraComponent>();
            } else if (type == CreationType::DirectionalLight) {
                auto* light = createdObj->AddComponent<LightComponent>();
                light->SetLightType(LightComponent::LightType::Directional);
            } else if (type == CreationType::PointLight) {
                auto* light = createdObj->AddComponent<LightComponent>();
                light->SetLightType(LightComponent::LightType::Point);
            } else if (type == CreationType::SpotLight) {
                auto* light = createdObj->AddComponent<LightComponent>();
                light->SetLightType(LightComponent::LightType::Spot);
            } else if (type != CreationType::Empty) {
                auto* mr = createdObj->AddComponent<MeshRendererComponent>();
                switch (type) {
                case CreationType::Cube: mr->SetMeshType(MeshRendererComponent::MeshType::Cube); break;
                case CreationType::Sphere: mr->SetMeshType(MeshRendererComponent::MeshType::Sphere); break;
                case CreationType::Pyramid: mr->SetMeshType(MeshRendererComponent::MeshType::Pyramid); break;
                case CreationType::TriangularPyramid: mr->SetMeshType(MeshRendererComponent::MeshType::TriangularPyramid); break;
                case CreationType::Triangle: mr->SetMeshType(MeshRendererComponent::MeshType::Triangle); break;
                case CreationType::Square: mr->SetMeshType(MeshRendererComponent::MeshType::Square); break;
                case CreationType::Cylinder: mr->SetMeshType(MeshRendererComponent::MeshType::Cylinder); break;
                case CreationType::Cone: mr->SetMeshType(MeshRendererComponent::MeshType::Cone); break;
                case CreationType::Ring: mr->SetMeshType(MeshRendererComponent::MeshType::Ring); break;
                case CreationType::Hemisphere: mr->SetMeshType(MeshRendererComponent::MeshType::Hemisphere); break;
                case CreationType::Model: mr->SetMeshType(MeshRendererComponent::MeshType::Model); break;
                case CreationType::Line3D: mr->SetMeshType(MeshRendererComponent::MeshType::Line); break;
                case CreationType::FlatRing: mr->SetMeshType(MeshRendererComponent::MeshType::FlatRing); break;
                case CreationType::CylinderEffect: mr->SetMeshType(MeshRendererComponent::MeshType::CylinderEffect); break;
                default: break;
                }

                // 3Dオブジェクトをアクティブカメラの正面（マジックナンバー排除: kSpawnDistanceForward）に配置して見失いを防止
                if (auto cameraMgr = CameraResource::GetCameraManager()) {
                    if (auto* activeCam = cameraMgr->GetActiveCamera()) {
                        Vector3 camPos = activeCam->GetPosition();
                        Vector3 camRot = activeCam->GetRotation();
                        Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
                        Vector3 forward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
                        constexpr float kMinForwardLength = 0.001f;
                        if (Math::Length(forward) < kMinForwardLength) {
                            forward = { 0.0f, 0.0f, 1.0f };
                        }
                        constexpr float kSpawnDistanceForward = 8.0f;
                        Vector3 spawnPos = {
                            camPos.x + forward.x * kSpawnDistanceForward,
                            camPos.y + forward.y * kSpawnDistanceForward,
                            camPos.z + forward.z * kSpawnDistanceForward
                        };
                        createdObj->SetPosition(spawnPos);
                    }
                }
            }

            SceneHierarchy::GetInstance()->SetSelected(createdObj);
            CommandHistory::GetInstance()->PushCommand(std::make_unique<CreateGameObjectCommand>(createdObj));
            Log::Write(std::format(L"[Hierarchy] 新規 GameObject「{}」を作成しました。",
                ConvertString(createdObj->GetName())));
        }
    }

    void FocusOnSelectedObject() {
        IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
        if (!selected) return;

        Vector3 targetPos = { 0.0f, 0.0f, 0.0f };
        if (auto* go = dynamic_cast<GameObject*>(selected)) {
            targetPos = go->GetPosition();
        } else if (auto* baseCam = dynamic_cast<BaseCamera*>(selected)) {
            targetPos = baseCam->GetPosition();
        }

        auto cameraMgr = CameraResource::GetCameraManager();
        if (!cameraMgr) return;

        BaseCamera* activeCam = cameraMgr->GetActiveCamera();
        if (!activeCam) return;

        // カメラの現在の向きから前方ベクトルを算出
        Vector3 rot = activeCam->GetRotation();
        Matrix4x4 rotMat = Math::MakeRotateMatrix(rot.x, rot.y, rot.z);
        Vector3 forward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
        constexpr float kMinForwardLength = 0.001f;
        if (Math::Length(forward) < kMinForwardLength) {
            forward = { 0.0f, 0.0f, 1.0f };
        }

        constexpr float kFocusDistance = 6.0f;
        Vector3 newCamPos = {
            targetPos.x - forward.x * kFocusDistance,
            targetPos.y - forward.y * kFocusDistance,
            targetPos.z - forward.z * kFocusDistance
        };

        activeCam->SetPosition(newCamPos);

        // アクティブカメラが CameraComponent を持つ GameObject の場合はオーナーの位置も同期
        auto currentScene = SceneManager::GetInstance()->GetCurrentScene();
        std::string activeName = cameraMgr->GetActiveCameraName();
        if (auto genericScene = dynamic_cast<GenericScene*>(currentScene)) {
            for (const auto& obj : genericScene->GetGameObjects()) {
                if (obj && obj->GetName() == activeName && obj->GetComponent<CameraComponent>()) {
                    obj->SetPosition(newCamPos);
                    break;
                }
            }
        }

        Log::Write(std::format(L"[Editor] 選択オブジェクト「{}」にカメラをフォーカスしました (Fキー)。",
            ConvertString(selected->GetName())));
    }

    void DestroyGameObjectInCurrentScene(GameObject* gameObject) {
        if (!gameObject) return;
        std::string name = gameObject->GetName();

        // 削除前に確実に GPU 同期を実行
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }

        auto currentScene = SceneManager::GetInstance()->GetCurrentScene();
        std::unique_ptr<GameObject> detached;
        if (currentScene) {
            detached = currentScene->DetachGameObject(gameObject);
        }
        if (!detached) {
            SceneHierarchy::GetInstance()->Unregister(gameObject);
            auto it = std::find_if(sFallbackObjects.begin(), sFallbackObjects.end(),
                [gameObject](const std::unique_ptr<GameObject>& ptr) {
                    return ptr.get() == gameObject;
                });
            if (it != sFallbackObjects.end()) {
                detached = std::move(*it);
                sFallbackObjects.erase(it);
            }
        }

        if (detached) {
            CommandHistory::GetInstance()->PushCommand(std::make_unique<DeleteGameObjectCommand>(std::move(detached)));
        }

        Log::Write(std::format(L"[Hierarchy] GameObject「{}」を削除しました。",
            ConvertString(name)));
    }

    void RenderCreateGameObjectMenuItems() {
        if (ImGui::MenuItem("Create Empty")) {
            CreateNewGameObjectInCurrentScene(CreationType::Empty);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("3D Object")) {
            if (ImGui::MenuItem("Cube")) {
                CreateNewGameObjectInCurrentScene(CreationType::Cube);
            }
            if (ImGui::MenuItem("Sphere")) {
                CreateNewGameObjectInCurrentScene(CreationType::Sphere);
            }
            if (ImGui::MenuItem("Cylinder")) {
                CreateNewGameObjectInCurrentScene(CreationType::Cylinder);
            }
            if (ImGui::MenuItem("Cone")) {
                CreateNewGameObjectInCurrentScene(CreationType::Cone);
            }
            if (ImGui::MenuItem("Pyramid")) {
                CreateNewGameObjectInCurrentScene(CreationType::Pyramid);
            }
            if (ImGui::MenuItem("Triangular Pyramid")) {
                CreateNewGameObjectInCurrentScene(CreationType::TriangularPyramid);
            }
            if (ImGui::MenuItem("Square (Plane)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Square);
            }
            if (ImGui::MenuItem("Triangle")) {
                CreateNewGameObjectInCurrentScene(CreationType::Triangle);
            }
            if (ImGui::MenuItem("Ring")) {
                CreateNewGameObjectInCurrentScene(CreationType::Ring);
            }
            if (ImGui::MenuItem("Hemisphere")) {
                CreateNewGameObjectInCurrentScene(CreationType::Hemisphere);
            }
            if (ImGui::MenuItem("Line (3D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Line3D);
            }
            if (ImGui::MenuItem("Flat Ring")) {
                CreateNewGameObjectInCurrentScene(CreationType::FlatRing);
            }
            if (ImGui::MenuItem("Cylinder Effect")) {
                CreateNewGameObjectInCurrentScene(CreationType::CylinderEffect);
            }
            if (ImGui::MenuItem("Model")) {
                CreateNewGameObjectInCurrentScene(CreationType::Model);
            }
            if (ImGui::MenuItem("Text (3D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Text3D);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("2D Object")) {
            if (ImGui::MenuItem("Sprite")) {
                CreateNewGameObjectInCurrentScene(CreationType::Sprite);
            }
            if (ImGui::MenuItem("Triangle (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Triangle2D);
            }
            if (ImGui::MenuItem("Circle (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Circle2D);
            }
            if (ImGui::MenuItem("Ring (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Ring2D);
            }
            if (ImGui::MenuItem("Line (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Line2D);
            }
            if (ImGui::MenuItem("Text (2D)")) {
                CreateNewGameObjectInCurrentScene(CreationType::Text2D);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional Light")) {
                CreateNewGameObjectInCurrentScene(CreationType::DirectionalLight);
            }
            if (ImGui::MenuItem("Point Light")) {
                CreateNewGameObjectInCurrentScene(CreationType::PointLight);
            }
            if (ImGui::MenuItem("Spot Light")) {
                CreateNewGameObjectInCurrentScene(CreationType::SpotLight);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Camera")) {
            CreateNewGameObjectInCurrentScene(CreationType::Camera);
        }
    }
}

DebugEditor::DebugEditor()
    : showSceneView_(true),
      showGameView_(true),
      showParticleStudio_(false),
      showPerfMonitor_(true),
      showHierarchy_(true),
      showInspector_(true),
      showContentBrowser_(true),
      showShortcutsWindow_(false),
      helpWindowTargetTab_(-1),
      helpSearchBuffer_{},
      helpSearchExactMatch_(false),
      isGameViewVisible_(false),
      currentWorkspace_(EditorWorkspace::Layout),
      activeWorkspaceIndex_(0),
      isFullscreen_(false),
      currentAspect_(AspectType::Aspect16_9_Low),
      isPaused_(true) {
    wpPrev_.length = sizeof(wpPrev_);

    // 1. メイン ワークスペース (通常のシーン・ゲーム編集)
    WorkspaceDefinition layoutWs;
    layoutWs.id = "layout";
    layoutWs.name = "メイン";
    layoutWs.isParticleStudio = false;
    layoutWs.isCustom = false;
    layoutWs.windowState.showSceneView     = true;
    layoutWs.windowState.showGameView      = true;
    layoutWs.windowState.showHierarchy      = true;
    layoutWs.windowState.showInspector      = true;
    layoutWs.windowState.showConsole        = true;
    layoutWs.windowState.showParticleStudio = false;
    layoutWs.windowState.showPostProcess    = false;
    layoutWs.windowState.showEffectList     = false;
    layoutWs.windowState.showPerfMonitor    = true;
    layoutWs.windowState.showContentBrowser = true;
    workspaces_.push_back(layoutWs);

    // 2. パーティクルスタジオ ワークスペース (GPUパーティクル特化)
    WorkspaceDefinition studioWs;
    studioWs.id = "particle_studio";
    studioWs.name = "パーティクルスタジオ";
    studioWs.isParticleStudio = true;
    studioWs.isCustom = false;
    studioWs.windowState.showSceneView     = false;
    studioWs.windowState.showGameView      = false;
    studioWs.windowState.showHierarchy      = false;
    studioWs.windowState.showInspector      = false;
    studioWs.windowState.showConsole        = true;
    studioWs.windowState.showParticleStudio = true;
    studioWs.windowState.showPostProcess    = false;
    studioWs.windowState.showEffectList     = false;
    studioWs.windowState.showPerfMonitor    = false;
    studioWs.windowState.showContentBrowser = false;
    workspaces_.push_back(studioWs);

    // 初期ワークスペースのウィンドウ状態を適用
    ApplyWorkspaceWindowState(0);
}

void DebugEditor::ApplyWorkspaceWindowState(size_t index) {
    if (index >= workspaces_.size()) return;

    // 【安全防壁・自己修復】メインワークスペース（index 0）で主要ウィンドウがすべてOFFになっている場合、
    // 画面が真っ暗になる異常状態を防ぐため、自動的に標準の表示状態へ自己修復
    if (index == 0) {
        auto& s0 = workspaces_[0].windowState;
        if (!s0.showSceneView && !s0.showGameView && !s0.showHierarchy && !s0.showInspector) {
            s0.showSceneView     = true;
            s0.showGameView      = true;
            s0.showHierarchy      = true;
            s0.showInspector      = true;
            s0.showConsole        = true;
            s0.showPerfMonitor    = true;
            s0.showContentBrowser = true;
        }
    }

    const auto& s = workspaces_[index].windowState;
    showSceneView_      = s.showSceneView;
    showGameView_       = s.showGameView;
    showHierarchy_      = s.showHierarchy;
    showInspector_      = s.showInspector;
    showParticleStudio_ = s.showParticleStudio;
    showPerfMonitor_    = s.showPerfMonitor;
    showContentBrowser_ = s.showContentBrowser;

    if (auto* consolePtr = Log::GetShowConsolePtr()) {
        *consolePtr = s.showConsole;
    }
    if (auto* postProcessPtr = PostProcess::GetShowWindowPtr()) {
        *postProcessPtr = s.showPostProcess;
    }
    if (auto* effectListPtr = EffectManager::GetShowListWindowPtr()) {
        *effectListPtr = s.showEffectList;
    }
}

void DebugEditor::SyncCurrentWorkspaceWindowState() {
    if (activeWorkspaceIndex_ >= workspaces_.size()) return;
    auto& s = workspaces_[activeWorkspaceIndex_].windowState;
    s.showSceneView      = showSceneView_;
    s.showGameView       = showGameView_;
    s.showHierarchy      = showHierarchy_;
    s.showInspector      = showInspector_;
    s.showParticleStudio = showParticleStudio_;
    s.showPerfMonitor    = showPerfMonitor_;
    s.showContentBrowser = showContentBrowser_;

    if (auto* consolePtr = Log::GetShowConsolePtr()) {
        s.showConsole = *consolePtr;
    }
    if (auto* postProcessPtr = PostProcess::GetShowWindowPtr()) {
        s.showPostProcess = *postProcessPtr;
    }
    if (auto* effectListPtr = EffectManager::GetShowListWindowPtr()) {
        s.showEffectList = *effectListPtr;
    }
}

void DebugEditor::SwitchToWorkspace(size_t index) {
    if (index >= workspaces_.size()) return;
    // 1. 直前のワークスペースの状態を最新保存
    SyncCurrentWorkspaceWindowState();

    // 2. 切り替え先のワークスペースを選択
    activeWorkspaceIndex_ = index;
    const auto& ws = workspaces_[index];
    if (ws.isParticleStudio) {
        currentWorkspace_ = EditorWorkspace::ParticleStudio;
    } else if (ws.isCustom) {
        currentWorkspace_ = EditorWorkspace::Custom;
    } else {
        currentWorkspace_ = EditorWorkspace::Layout;
    }

    // 3. 切り替え先のウィンドウ開閉状態を復元
    ApplyWorkspaceWindowState(index);
}

void DebugEditor::SetWorkspace(EditorWorkspace ws) {
    for (size_t i = 0; i < workspaces_.size(); ++i) {
        if (ws == EditorWorkspace::ParticleStudio && workspaces_[i].isParticleStudio) {
            SwitchToWorkspace(i);
            return;
        }
        if (ws == EditorWorkspace::Layout && !workspaces_[i].isParticleStudio && !workspaces_[i].isCustom) {
            SwitchToWorkspace(i);
            return;
        }
    }
}

void DebugEditor::AddCustomWorkspace(const std::string& name) {
    if (name.empty()) return;
    SyncCurrentWorkspaceWindowState();

    std::string id = "custom_" + std::to_string(workspaces_.size());
    WorkspaceDefinition customWs;
    customWs.id = id;
    customWs.name = name;
    customWs.isParticleStudio = false;
    customWs.isCustom = true;

    // 新規作業ページは既存画面を引き継がず、独立したクリーンな状態（コンソールのみ表示）で初期化
    customWs.windowState = WorkspaceWindowState{}; // 全ウィンドウを非表示
    customWs.windowState.showConsole = true;       // コンソールのみ表示

    workspaces_.push_back(customWs);
    SwitchToWorkspace(workspaces_.size() - 1);
    SaveWorkspacesConfig();
}

void DebugEditor::RemoveCustomWorkspace(size_t index) {
    // 基本レイアウト (index 0) は安全のため削除不可
    if (index == 0 || index >= workspaces_.size()) {
        return;
    }

    // 削除前に現在の正しいアクティブページのウィンドウ状態を確実に保存
    SyncCurrentWorkspaceWindowState();

    // 削除対象ページを消去
    workspaces_.erase(workspaces_.begin() + index);

    // 切り替え先の目標インデックスを決定（削除されたページにいた場合はメインへ安全に遷移）
    size_t targetIndex = 0;
    if (activeWorkspaceIndex_ < workspaces_.size() && activeWorkspaceIndex_ != index) {
        targetIndex = (activeWorkspaceIndex_ > index) ? (activeWorkspaceIndex_ - 1) : activeWorkspaceIndex_;
    }

    // 新しいアクティブインデックスへ安全に切り替え
    activeWorkspaceIndex_ = targetIndex;
    ApplyWorkspaceWindowState(targetIndex);
    SaveWorkspacesConfig();
}

void DebugEditor::SaveWorkspacesConfig() {
    SyncCurrentWorkspaceWindowState();

    std::filesystem::path configPath(kWorkspacesConfigFilePath);
    if (configPath.has_parent_path()) {
        std::filesystem::create_directories(configPath.parent_path());
    }

    std::ofstream ofs(kWorkspacesConfigFilePath);
    if (!ofs.is_open()) return;

    ofs << "{\n";
    ofs << "  \"activeWorkspaceIndex\": " << activeWorkspaceIndex_ << ",\n";
    ofs << "  \"isLayoutLocked\": " << (isLayoutLocked_ ? "true" : "false") << ",\n";
    ofs << "  \"workspaces\": [\n";

    for (size_t i = 0; i < workspaces_.size(); ++i) {
        const auto& ws = workspaces_[i];
        ofs << "    {\n";
        ofs << "      \"id\": \"" << ws.id << "\",\n";
        ofs << "      \"name\": \"" << ws.name << "\",\n";
        ofs << "      \"isParticleStudio\": " << (ws.isParticleStudio ? "true" : "false") << ",\n";
        ofs << "      \"isCustom\": " << (ws.isCustom ? "true" : "false") << ",\n";
        ofs << "      \"windowState\": {\n";
        const auto& s = ws.windowState;
        ofs << "        \"showSceneView\": " << (s.showSceneView ? "true" : "false") << ",\n";
        ofs << "        \"showGameView\": " << (s.showGameView ? "true" : "false") << ",\n";
        ofs << "        \"showHierarchy\": " << (s.showHierarchy ? "true" : "false") << ",\n";
        ofs << "        \"showInspector\": " << (s.showInspector ? "true" : "false") << ",\n";
        ofs << "        \"showConsole\": " << (s.showConsole ? "true" : "false") << ",\n";
        ofs << "        \"showParticleStudio\": " << (s.showParticleStudio ? "true" : "false") << ",\n";
        ofs << "        \"showPostProcess\": " << (s.showPostProcess ? "true" : "false") << ",\n";
        ofs << "        \"showEffectList\": " << (s.showEffectList ? "true" : "false") << ",\n";
        ofs << "        \"showPerfMonitor\": " << (s.showPerfMonitor ? "true" : "false") << ",\n";
        ofs << "        \"showContentBrowser\": " << (s.showContentBrowser ? "true" : "false") << "\n";
        ofs << "      }\n";
        ofs << "    }" << (i + 1 < workspaces_.size() ? "," : "") << "\n";
    }

    ofs << "  ]\n";
    ofs << "}\n";
}

void DebugEditor::LoadWorkspacesConfig() {
    if (!std::filesystem::exists(kWorkspacesConfigFilePath)) {
        return; // 設定ファイルが存在しない場合はデフォルトのメイン＆スタジオを使用
    }

    std::ifstream ifs(kWorkspacesConfigFilePath);
    if (!ifs.is_open()) return;

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string jsonStr = buffer.str();
    if (jsonStr.empty()) return;

    try {
        int loadedActiveIndex = ExtractJsonInt(jsonStr, "activeWorkspaceIndex", 0);
        isLayoutLocked_ = ExtractJsonBool(jsonStr, "isLayoutLocked", false);

        size_t arrayStart = jsonStr.find("\"workspaces\":");
        if (arrayStart == std::string::npos) return;
        size_t openBracket = jsonStr.find('[', arrayStart);
        if (openBracket == std::string::npos) return;

        std::vector<WorkspaceDefinition> loadedWorkspaces;

        size_t curPos = openBracket + 1;
        while (curPos < jsonStr.length()) {
            size_t objStart = jsonStr.find('{', curPos);
            if (objStart == std::string::npos) break;

            int braceCount = 1;
            size_t objEnd = objStart + 1;
            while (objEnd < jsonStr.length() && braceCount > 0) {
                if (jsonStr[objEnd] == '{') ++braceCount;
                else if (jsonStr[objEnd] == '}') --braceCount;
                ++objEnd;
            }
            if (braceCount != 0) break;

            std::string block = jsonStr.substr(objStart, objEnd - objStart);

            WorkspaceDefinition ws;
            ws.id = ExtractJsonString(block, "id", "layout");
            ws.name = ExtractJsonString(block, "name", "メイン");
            ws.isParticleStudio = ExtractJsonBool(block, "isParticleStudio", false);
            ws.isCustom = ExtractJsonBool(block, "isCustom", false);

            size_t wsStatePos = block.find("\"windowState\":");
            std::string stateBlock = (wsStatePos != std::string::npos) ? block.substr(wsStatePos) : block;

            ws.windowState.showSceneView     = ExtractJsonBool(stateBlock, "showSceneView", true);
            ws.windowState.showGameView      = ExtractJsonBool(stateBlock, "showGameView", true);
            ws.windowState.showHierarchy      = ExtractJsonBool(stateBlock, "showHierarchy", true);
            ws.windowState.showInspector      = ExtractJsonBool(stateBlock, "showInspector", true);
            ws.windowState.showConsole        = ExtractJsonBool(stateBlock, "showConsole", true);
            ws.windowState.showParticleStudio = ExtractJsonBool(stateBlock, "showParticleStudio", false);
            ws.windowState.showPostProcess    = ExtractJsonBool(stateBlock, "showPostProcess", false);
            ws.windowState.showEffectList     = ExtractJsonBool(stateBlock, "showEffectList", false);
            ws.windowState.showPerfMonitor    = ExtractJsonBool(stateBlock, "showPerfMonitor", true);
            ws.windowState.showContentBrowser = ExtractJsonBool(stateBlock, "showContentBrowser", true);

            loadedWorkspaces.push_back(ws);
            curPos = objEnd;
        }

        // 最低1つのワークスペースが存在する場合のみ復元
        if (!loadedWorkspaces.empty()) {
            // 【安全防壁】メインワークスペースが全OFFで壊れて保存されていた場合は自動修復
            auto& s0 = loadedWorkspaces[0].windowState;
            if (!s0.showSceneView && !s0.showGameView && !s0.showHierarchy && !s0.showInspector) {
                s0.showSceneView     = true;
                s0.showGameView      = true;
                s0.showHierarchy      = true;
                s0.showInspector      = true;
                s0.showConsole        = true;
                s0.showPerfMonitor    = true;
                s0.showContentBrowser = true;
            }

            workspaces_ = std::move(loadedWorkspaces);
            activeWorkspaceIndex_ = (std::min)(static_cast<size_t>((std::max)(0, loadedActiveIndex)), workspaces_.size() - 1);
            ApplyWorkspaceWindowState(activeWorkspaceIndex_);
        }
    } catch (...) {
        // パースエラー時も安全にデフォルト継続
    }
}

void DebugEditor::ResetWorkspaceLayout(size_t index) {
    if (index >= workspaces_.size()) return;

    auto& ws = workspaces_[index];
    if (ws.isParticleStudio) {
        ws.windowState = WorkspaceWindowState{};
        ws.windowState.showParticleStudio = true;
        ws.windowState.showConsole = true;
    } else if (ws.isCustom) {
        ws.windowState = WorkspaceWindowState{};
        ws.windowState.showConsole = true;
    } else {
        // メインワークスペースの標準整列状態
        ws.windowState.showSceneView     = true;
        ws.windowState.showGameView      = true;
        ws.windowState.showHierarchy      = true;
        ws.windowState.showInspector      = true;
        ws.windowState.showConsole        = true;
        ws.windowState.showParticleStudio = false;
        ws.windowState.showPostProcess    = false;
        ws.windowState.showEffectList     = false;
        ws.windowState.showPerfMonitor    = true;
        ws.windowState.showContentBrowser = true;
    }

    ApplyWorkspaceWindowState(index);
    SaveWorkspacesConfig();
}

DebugEditor::~DebugEditor() {
    SaveWorkspacesConfig();
    sFallbackObjects.clear();
}

void DebugEditor::Initialize() {
    sceneViewWindow_ = std::make_unique<SceneViewWindow>();
    gameViewWindow_ = std::make_unique<GameViewWindow>();
    perfMonitorWindow_ = std::make_unique<PerformanceMonitorWindow>();
    contentBrowserWindow_ = std::make_unique<ContentBrowserWindow>();
    particleStudioWindow_ = std::make_unique<ParticleStudioWindow>();

    // ウィンドウ終了時（×ボタン、Alt+F4、終了メニュー）のセーブ確認ハンドラを登録
    HWND hwnd = Zuizui::GetInstance()->GetWindow()->GetHWND();
    Zuizui::GetInstance()->GetWindow()->SetCloseHandler([hwnd]() {
        return ConfirmExitAndSave(hwnd);
    });

    // 保存されているワークスペース設定をロードして完全復元
    LoadWorkspacesConfig();
}

void DebugEditor::Draw(ID3D12GraphicsCommandList* commandList) {
    // 描画開始時に可視性フラグを初期化
    HWND hwnd = Zuizui::GetInstance()->GetWindow()->GetHWND();

    // 右上の最大化（□）ボタンや復元ボタンによるウィンドウサイズ変更の監視
    static bool sInitializedZoomState = false;
    static bool sLastIsZoomed = false;
    bool isCurrentlyZoomed = (IsZoomed(hwnd) != FALSE);
    if (!isFullscreen_) {
        if (!sInitializedZoomState) {
            sLastIsZoomed = isCurrentlyZoomed;
            sInitializedZoomState = true;
        } else if (isCurrentlyZoomed != sLastIsZoomed) {
            sLastIsZoomed = isCurrentlyZoomed;
            if (isCurrentlyZoomed) {
                EditorNotification::ShowInfo("ウィンドウを最大化しました");
            } else {
                EditorNotification::ShowInfo("ウィンドウサイズを復元しました");
            }
        }
    } else {
        sLastIsZoomed = isCurrentlyZoomed;
    }

    // Ctrl+S ショートカットによる即時シーン保存 & ワークスペース設定保存
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        SaveCurrentSceneAction(SceneManager::GetInstance()->GetCurrentSceneName());
        SaveWorkspacesConfig();
    }

    // Ctrl+Z (Undo) / Ctrl+Y or Ctrl+Shift+Z (Redo) ショートカット (Unity / Blender 準拠)
    if (!io.WantTextInput && io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (io.KeyShift) {
                CommandHistory::GetInstance()->Redo();
            } else {
                CommandHistory::GetInstance()->Undo();
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            CommandHistory::GetInstance()->Redo();
        }
    }

    // F キーによるオブジェクトフォーカス (Focus on Selection)
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        FocusOnSelectedObject();
    }

    // F1 キーによるショートカット一覧の開閉
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        showShortcutsWindow_ = !showShortcutsWindow_;
    }

    // F11 キーによるフルスクリーンの切り替え
    if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
        ToggleFullscreen(hwnd);
    }

    // メインメニューバーの描画
    DrawMenuBar(hwnd);

    // ワークスペースごとに独立した DockSpace ID を使用（過去の壊れた ini キャッシュと完全分離・キメラ化防止）
    std::string wsId = (activeWorkspaceIndex_ < workspaces_.size()) ? workspaces_[activeWorkspaceIndex_].id : "layout";
    std::string dockspaceName = std::string(kDockSpacePrefix) + wsId;
    ImGuiID dockspaceId = ImGui::GetID(dockspaceName.c_str());

    ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    if (isLayoutLocked_) {
        dockFlags |= ImGuiDockNodeFlags_NoResize;
    }
    ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport(), dockFlags);

    // -------------------------------------------------------------
    // 各ウィンドウの描画 (ワークスペース別フラグに基づき排他制御・FPS最適化)
    // -------------------------------------------------------------
    // Particle Studio (GPUパーティクル専用メインスタジオ画面)
    if (showParticleStudio_ && particleStudioWindow_) {
        float deltaTime = Zuizui::GetInstance()->GetDxCommon()->GetDeltaTime();
        particleStudioWindow_->Update(deltaTime);
        particleStudioWindow_->Draw(&showParticleStudio_, commandList);
    }

    // Scene View (編集画面)
    if (showSceneView_ && sceneViewWindow_) {
        sceneViewWindow_->Draw(&showSceneView_);
    }

    // Game View (ゲーム画面)
    if (showGameView_ && gameViewWindow_) {
        gameViewWindow_->Draw(&showGameView_, &isGameViewVisible_);
    } else {
        isGameViewVisible_ = false;
    }

    // Console
    Log::DrawConsoleWindow(-1.0f);

    // Performance Monitor
    if (showPerfMonitor_ && perfMonitorWindow_) {
        perfMonitorWindow_->Draw(&showPerfMonitor_);
    }

    // Content Browser
    if (showContentBrowser_ && contentBrowserWindow_) {
        contentBrowserWindow_->Draw(&showContentBrowser_);
    }

    // エフェクト一覧 & GPUパーティクル設定ウィンドウ
    if (auto* effectMgr = EffectManager::GetInstance()) {
        effectMgr->ImGuiControl("エフェクト一覧");
    }

    // Hierarchy (左側)
    if (showHierarchy_) {
        if (ImGui::Begin("ヒエラルキー###Hierarchy", &showHierarchy_)) {
            // [+ Create GameObject] ボタン
            constexpr float kCreateBtnHeight = 24.0f;
            if (ImGui::Button("+ Create GameObject", ImVec2(-1.0f, kCreateBtnHeight))) {
                ImGui::OpenPopup("CreateGameObjectPopup");
            }

            if (ImGui::BeginPopup("CreateGameObjectPopup")) {
                RenderCreateGameObjectMenuItems();
                ImGui::EndPopup();
            }
            ImGui::Separator();

            // 🎬 シーン＆SceneDirector の特別ルートノード描画
            auto* currentSceneObj = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
            if (currentSceneObj) {
                constexpr float kSceneNodeHeight = 24.0f;
                constexpr ImVec4 kSceneActiveBgColor = { 0.18f, 0.32f, 0.52f, 1.0f };
                constexpr ImVec4 kSceneNormalBgColor = { 0.10f, 0.15f, 0.24f, 0.85f };
                constexpr ImVec4 kSceneActiveTextColor = { 0.45f, 0.85f, 1.0f, 1.0f };
                constexpr ImVec4 kSceneDirectorSubColor = { 0.35f, 0.75f, 0.95f, 0.8f };

                std::string sceneTitle = "[Scene] " + currentSceneObj->GetSceneName() + " [Active]";

                ImGui::PushStyleColor(ImGuiCol_Header, isSceneSelected_ ? kSceneActiveBgColor : kSceneNormalBgColor);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kSceneActiveBgColor);
                ImGui::PushStyleColor(ImGuiCol_Text, kSceneActiveTextColor);

                bool isNodeClicked = ImGui::Selectable(sceneTitle.c_str(), isSceneSelected_, 0, ImVec2(0.0f, kSceneNodeHeight));

                ImGui::PopStyleColor(3);

                if (isNodeClicked) {
                    isSceneSelected_ = true;
                    SceneHierarchy::GetInstance()->SetSelected(nullptr);
                }

                // シーン項目の右クリックコンテキストメニュー
                if (ImGui::BeginPopupContextItem("SceneContextMenu")) {
                    ImGui::TextColored(kSceneActiveTextColor, "[Scene] %s", currentSceneObj->GetSceneName().c_str());
                    ImGui::Separator();

                    if (ImGui::MenuItem("Inspect SceneDirector (ディレクターを開く)")) {
                        isSceneSelected_ = true;
                        SceneHierarchy::GetInstance()->SetSelected(nullptr);
                    }

                    // スクリプトのアタッチサブメニュー
                    auto* director = currentSceneObj->GetDirector();
                    if (director) {
                        auto registeredScripts = ScriptRegistry::GetInstance().GetRegisteredScriptNames();
                        if (ImGui::BeginMenu("+ Attach Script (スクリプトを追加)")) {
                            if (registeredScripts.empty()) {
                                ImGui::TextDisabled("登録済みスクリプトがありません");
                            } else {
                                for (const auto& sName : registeredScripts) {
                                    if (ImGui::MenuItem(sName.c_str())) {
                                        auto newScript = ScriptRegistry::GetInstance().CreateScript(sName);
                                        if (newScript) {
                                            director->AddScript(std::move(newScript));
                                        }
                                    }
                                }
                            }
                            ImGui::EndMenu();
                        }
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Save Scene (シーン保存)")) {
                        SaveCurrentSceneAction(currentSceneObj->GetSceneName());
                    }

                    if (ImGui::MenuItem("Reload Scene (シーン再読み込み)")) {
                        // メモリキャッシュを明示的にクリアし、ディスク上の正規ファイルを再読み込み
                        SceneMemoryCache::GetInstance()->ClearCache(currentSceneObj->GetSceneName());
                        SceneManager::GetInstance()->ChangeSceneImmediate(currentSceneObj->GetSceneName());
                    }

                    ImGui::EndPopup();
                }

                ImGui::Spacing();
                ImGui::TextDisabled("── GameObjects ──");
                ImGui::SameLine();
                ImGui::Checkbox("内部表示##ShowInternal", &showInternalObjects_);
            }

            const auto& objects = SceneHierarchy::GetInstance()->GetObjects();
            IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
            GameObject* objToDelete = nullptr;

            for (size_t i = 0; i < objects.size(); ++i) {
                auto* obj = objects[i];
                if (!obj) continue;
                if (!showInternalObjects_ && obj->IsHiddenInHierarchy()) {
                    continue; // 柱などの内部・装飾オブジェクトは非表示
                }

                ImGui::PushID(static_cast<int>(i));

                auto* go = dynamic_cast<GameObject*>(obj);
                bool isSelected = (obj == selected);

                if (go) {
                    // --- Blender風アウトライナー行レイアウト定数 (マジックナンバー排除) ---
                    constexpr float kEyeBtnWidth = 18.0f;       // 目アイコンボタンのクリック幅
                    constexpr float kDeleteBtnWidth = 14.0f;    // 削除「x」ボタンのクリック幅
                    constexpr float kItemSpacing = 4.0f;        // アイコン間の間隔
                    constexpr float kRightPadding = 4.0f;       // 右端の余白
                    const ImVec4 kNormalTextColor = { 0.5f, 0.5f, 0.5f, 0.7f };  // 削除ボタン通常色（控えめなグレー）
                    const ImVec4 kHoveredTextColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 削除ボタンホバー色（クリアホワイト）

                    float rowStartX = ImGui::GetCursorPosX();
                    float rowStartY = ImGui::GetCursorPosY();
                    float availWidth = ImGui::GetContentRegionAvail().x;
                    float frameHeight = ImGui::GetFrameHeight();

                    // 【一体ハイライト判定】マウスが文字・目・xのどこにあっても、行全体（横幅全域）を1本の帯として発光させる
                    ImVec2 rowScreenMin = ImGui::GetCursorScreenPos();
                    ImVec2 rowScreenMax = ImVec2(rowScreenMin.x + availWidth, rowScreenMin.y + frameHeight);
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    bool isMouseInRow = (mousePos.x >= rowScreenMin.x && mousePos.x <= rowScreenMax.x &&
                                         mousePos.y >= rowScreenMin.y && mousePos.y <= rowScreenMax.y) &&
                                        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

                    // 現在アクティブなカメラかどうか判定
                    bool isActiveCam = false;
                    if (auto cameraMgr = CameraResource::GetCameraManager()) {
                        if (go->GetComponent<CameraComponent>() && obj->GetName() == cameraMgr->GetActiveCameraName()) {
                            isActiveCam = true;
                        }
                    }

                    std::string label = obj->GetName();
                    if (isActiveCam) {
                        label += " [Active]";
                        constexpr ImVec4 kActiveCamColor = { 0.35f, 0.85f, 1.0f, 1.0f };
                        ImGui::PushStyleColor(ImGuiCol_Text, kActiveCamColor);
                    }

                    // 幅 0.0f（全幅）で Selectable を敷き、行内ホバー時は ImGuiSelectableFlags_Highlight を付与して一体発光
                    ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_AllowOverlap;
                    if (isMouseInRow && !isSelected) {
                        selectableFlags |= ImGuiSelectableFlags_Highlight;
                    }

                    ImGui::Selectable(label.c_str(), isSelected, selectableFlags, ImVec2(0.0f, 0.0f));
                    float nextRowY = ImGui::GetCursorPosY(); // 次の行の開始Y座標を保持

                    if (isActiveCam) {
                        ImGui::PopStyleColor();
                    }

                    // 行全体のクリック・ダブルクリック状態を取得
                    bool isRowHovered = ImGui::IsItemHovered();
                    bool isRowDoubleClicked = isRowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                    bool isRowClickedL = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                    bool isRowClickedM = ImGui::IsItemClicked(ImGuiMouseButton_Middle);
                    bool isRowClickedR = ImGui::IsItemClicked(ImGuiMouseButton_Right);

                    // GameObject参照用のドラッグ＆ドロップソース (インスペクターのスロットへD&D可能)
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        const std::string& uuid = go->GetUUID();
                        ImGui::SetDragDropPayload("DND_GAMEOBJECT_UUID", uuid.c_str(), (uuid.length() + 1) * sizeof(char));
                        ImGui::Text("GameObject: %s", go->GetName().c_str());
                        ImGui::EndDragDropSource();
                    }

                    // GameObject項目の右クリックコンテキストメニュー
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Save as Prefab (プレハブとして保存)")) {
                            std::string prefabFilePath = "resources/Prefabs/" + go->GetName() + ".prefab";
                            PrefabManager::GetInstance().SavePrefab(prefabFilePath, go);
                            EditorNotification::ShowSuccess("プレハブ「" + go->GetName() + "」を保存しました");
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("削除 (Delete)")) {
                            objToDelete = go;
                        }
                        ImGui::EndPopup();
                    }

                    // ── 右端の操作アイコン群を行ハイライトの上にオーバーレイ配置 ──
                    float iconsTotalWidth = kEyeBtnWidth + kItemSpacing + kDeleteBtnWidth + kRightPadding;
                    float iconsStartX = (std::max)(rowStartX, rowStartX + availWidth - iconsTotalWidth);
                    ImGui::SetCursorPos(ImVec2(iconsStartX, rowStartY));

                    // 👁 1. 表示フラグ用の目アイコンボタン (名前の右端に配置・フレームレス)
                    bool isVisible = obj->IsVisible();
                    std::string eyeBtnId = "##eye_vis_" + obj->GetName() + "_" + std::to_string(i);
                    bool eyeClicked = RenderEyeVisibilityButton(eyeBtnId.c_str(), isVisible);
                    bool isEyeHovered = ImGui::IsItemHovered();
                    if (eyeClicked) {
                        obj->SetVisible(!isVisible);
                    }

                    // ✖ 2. 削除ボタン (目アイコンの右隣に配置・フレームレス)
                    ImGui::SameLine();
                    ImGui::SetNextItemAllowOverlap();
                    ImVec2 btnPos = ImGui::GetCursorPos();
                    bool delClicked = false;
                    if (ImGui::InvisibleButton("##del_btn", ImVec2(kDeleteBtnWidth, ImGui::GetTextLineHeight()))) {
                        delClicked = true;
                        objToDelete = go;
                    }
                    bool isDelHovered = ImGui::IsItemHovered();

                    // テキスト単体描画（ホバー時のみ白く点灯）
                    ImGui::SetCursorPos(btnPos);
                    ImGui::TextColored(isDelHovered ? kHoveredTextColor : kNormalTextColor, "x");

                    // 【排他制御】アイコン（目やx）のクリック/ホバー時は、背後のオブジェクト選択処理をスキップして誤爆を防止
                    bool isIconActive = isEyeHovered || isDelHovered || eyeClicked || delClicked;

                    if (!isIconActive) {
                        if (isRowDoubleClicked) {
                            isSceneSelected_ = false;
                            SceneHierarchy::GetInstance()->SetSelected(obj);
                            FocusOnSelectedObject();
                        } else if (isRowClickedL) {
                            isSceneSelected_ = false;
                            SceneHierarchy::GetInstance()->SetSelected(obj);
                            if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpTranslate);
                        } else if (isRowClickedM) {
                            isSceneSelected_ = false;
                            SceneHierarchy::GetInstance()->SetSelected(obj);
                            if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpRotate);
                        } else if (isRowClickedR) {
                            isSceneSelected_ = false;
                            SceneHierarchy::GetInstance()->SetSelected(obj);
                            if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpScale);
                        }
                    }

                    // カーソル位置を次の行の先頭へ確実に進める
                    ImGui::SetCursorPos(ImVec2(rowStartX, nextRowY));
                } else {
                    ImGui::Selectable(obj->GetName().c_str(), isSelected);
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpTranslate);
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpRotate);
                    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        SceneHierarchy::GetInstance()->SetSelected(obj);
                        if (sceneViewWindow_) sceneViewWindow_->SetGizmoOperation(SceneViewWindow::kGizmoOpScale);
                    }
                    // オブジェクト行の上へのプレハブドロップ受け取り
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadPrefab)) {
                            const char* prefabPath = static_cast<const char*>(payload->Data);
                            if (auto* newObj = SceneManager::GetInstance()->Instantiate(prefabPath)) {
                                SceneHierarchy::GetInstance()->SetSelected(newObj);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                ImGui::PopID();
            }

            if (objToDelete) {
                DestroyGameObjectInCurrentScene(objToDelete);
            }

            // Prefab のドラッグ＆ドロップ受け取り (Hierarchy ウィンドウ全体の余白領域)
            ImVec2 remainingSpace = ImGui::GetContentRegionAvail();
            float dropHeight = (remainingSpace.y > kMinDropAreaHeight) ? remainingSpace.y : kMinDropAreaHeight;
            ImGui::Dummy(ImVec2(remainingSpace.x, dropHeight));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadPrefab)) {
                    const char* prefabPath = static_cast<const char*>(payload->Data);
                    if (auto* newObj = SceneManager::GetInstance()->Instantiate(prefabPath)) {
                        SceneHierarchy::GetInstance()->SetSelected(newObj);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // 空白エリアの右クリックコンテキストメニュー
            if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                RenderCreateGameObjectMenuItems();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    // Inspector (右側)
    if (showInspector_) {
        if (ImGui::Begin("インスペクター###Inspector", &showInspector_)) {
            IGameObject* selected = SceneHierarchy::GetInstance()->GetSelected();
            auto* currentSceneObj = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());

            if (isSceneSelected_ && currentSceneObj && currentSceneObj->GetDirector()) {
                currentSceneObj->GetDirector()->DrawInspector();
            } else if (selected) {
                // 名前の編集 & プレハブ保存ボタン（インスペクター利用可能幅に完全フィットさせてカード幅と一致）
                constexpr int kNameBufferSize = 128;
                char nameBuf[kNameBufferSize];
                strcpy_s(nameBuf, selected->GetName().c_str());

                auto* selectedGo = dynamic_cast<GameObject*>(selected);
                float availWidth = ImGui::GetContentRegionAvail().x;
                const char* kPrefabBtnText = "Save as Prefab";
                float prefabBtnWidth = ImGui::CalcTextSize(kPrefabBtnText).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                float nameFieldWidth = selectedGo
                    ? (availWidth - prefabBtnWidth - ImGui::GetStyle().ItemSpacing.x)
                    : availWidth;

                ImGui::SetNextItemWidth(nameFieldWidth);
                if (ImGui::InputText("##ObjectName", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    selected->SetName(nameBuf);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("オブジェクト名 (Enterで確定)");
                }

                if (selectedGo) {
                    ImGui::SameLine();
                    if (ImGui::Button(kPrefabBtnText, ImVec2(prefabBtnWidth, 0.0f))) {
                        std::string prefabFilePath = "resources/Prefabs/" + selectedGo->GetName() + ".prefab";
                        PrefabManager::GetInstance().SavePrefab(prefabFilePath, selectedGo);
                        EditorNotification::ShowSuccess("プレハブ「" + selectedGo->GetName() + "」を保存しました");
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("このオブジェクトをプレハブとして保存します (resources/Prefabs/)");
                    }
                }

                ImGui::Separator();

                // 各種オブジェクト固有のインスペクター描画
                selected->DrawInspector();
            } else {
                ImGui::Text("オブジェクトが選択されていません。");
            }
        }
        ImGui::End();
    }
    // ショートカットキー一覧ダイアログ
    if (showShortcutsWindow_) {
        DrawShortcutsWindow();
    }

    // 各ウィンドウの開閉状態（×ボタンでの閉鎖やメニュー切り替え）を現在の作業ページにリアルタイム同期
    SyncCurrentWorkspaceWindowState();

    // トースト通知の更新・最前面オーバーレイ描画
    EditorNotification::Draw();
}

void DebugEditor::ToggleFullscreen(HWND hwnd) {
    isFullscreen_ = !isFullscreen_;
    if (isFullscreen_) {
        EditorNotification::ShowInfo("フルスクリーン表示に切り替えました (F11で解除)");
    } else {
        EditorNotification::ShowInfo("ウィンドウ表示に戻しました");
    }
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

    // 通常時のウィンドウスタイル（最大化ボタンおよび枠ドラッグによる自由拡縮を許可・可視状態を維持）
    const DWORD kOriginalStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

    if (isFullscreen_) {
        // フルスクリーン化（ボーダーレス全画面化）
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &wpPrev_) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, (dwStyle & ~WS_OVERLAPPEDWINDOW) | WS_VISIBLE);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        // 元の画面サイズに戻す
        SetWindowLong(hwnd, GWL_STYLE, kOriginalStyle);

        wpPrev_.showCmd = SW_SHOWNORMAL;

        // 直前のアスペクト比設定に応じたクライアント解像度を決定（マジックナンバー排除）
        int32_t restoreWidth = 1280;
        int32_t restoreHeight = 720;
        switch (currentAspect_) {
        case AspectType::Aspect16_9_Low:
            restoreWidth = 1280;
            restoreHeight = 720;
            break;
        case AspectType::Aspect16_9_High:
            restoreWidth = 1920;
            restoreHeight = 1080;
            break;
        case AspectType::Aspect4_3:
            restoreWidth = 960;
            restoreHeight = 720;
            break;
        case AspectType::Aspect1_1:
            restoreWidth = 720;
            restoreHeight = 720;
            break;
        }

        RECT wr = { 0, 0, restoreWidth, restoreHeight };
        AdjustWindowRect(&wr, kOriginalStyle, FALSE);

        wpPrev_.rcNormalPosition.right = wpPrev_.rcNormalPosition.left + (wr.right - wr.left);
        wpPrev_.rcNormalPosition.bottom = wpPrev_.rcNormalPosition.top + (wr.bottom - wr.top);

        SetWindowPlacement(hwnd, &wpPrev_);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        ShowWindow(hwnd, SW_SHOWNORMAL);
    }
}

void DebugEditor::DrawMenuBar(HWND hwnd) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text("ZuizuiEngine");
        ImGui::Separator();
        
        if (ImGui::BeginMenu("ファイル###File")) {
            if (ImGui::MenuItem("保存", "Ctrl+S")) {
                std::string currentSceneName = SceneManager::GetInstance()->GetCurrentSceneName();
                SaveCurrentSceneAction(currentSceneName);
                SaveWorkspacesConfig();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("終了", "Alt+F4")) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("編集###Edit")) {
            auto cmdHistory = CommandHistory::GetInstance();
            std::string undoLabel = "元に戻す (Undo)";
            if (cmdHistory->CanUndo()) {
                undoLabel += " - " + cmdHistory->GetUndoName();
            }
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, cmdHistory->CanUndo())) {
                cmdHistory->Undo();
            }

            std::string redoLabel = "やり直す (Redo)";
            if (cmdHistory->CanRedo()) {
                redoLabel += " - " + cmdHistory->GetRedoName();
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, cmdHistory->CanRedo())) {
                cmdHistory->Redo();
            }
            ImGui::EndMenu();
        }

        // ----------------------------------------------------
        // シーン管理メニュー (Scene)
        // ----------------------------------------------------
        if (ImGui::BeginMenu("Scene###SceneMenu")) {
            std::string currentSceneName = SceneManager::GetInstance()->GetCurrentSceneName();

            // 1. JSONシーン一覧（resources/Scenes/ 内の .json ファイルを走査）
            if (!std::filesystem::exists(kScenesDirectory)) {
                std::filesystem::create_directories(kScenesDirectory);
            }

            std::vector<std::string> jsonScenes;
            for (const auto& entry : std::filesystem::directory_iterator(kScenesDirectory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string stemName = entry.path().stem().string();
                    // 設定用JSONは通常シーン一覧から除外
                    if (stemName == kTransitionsConfigFileName) {
                        continue;
                    }
                    jsonScenes.push_back(stemName);
                }
            }

            std::string currentDebugStartup = SceneManager::GetInstance()->GetDebugStartupSceneName();
            std::string currentReleaseStartup = SceneManager::GetInstance()->GetReleaseStartupSceneName();

            // シーン一覧表示
            for (const auto& sName : jsonScenes) {
                std::string itemId = "json_scene_" + sName;
                ImGui::PushID(itemId.c_str());
                bool isSelected = (sName == currentSceneName);

                // トランジション設定の有無を取得（IN/OUT独立バッジ）
                auto transCfg = SceneTransitionManager::GetInstance()->GetConfig(sName);
                bool hasOut = transCfg.HasOut();
                bool hasIn = transCfg.HasIn();

                bool isDebugStartup = (sName == currentDebugStartup);
                bool isReleaseStartup = (sName == currentReleaseStartup);

                // 起動シーンバッジ
                std::string startupBadge;
                if (isDebugStartup && isReleaseStartup) {
                    startupBadge = " [D/R]";
                } else if (isDebugStartup) {
                    startupBadge = " [Debug]";
                } else if (isReleaseStartup) {
                    startupBadge = " [Release]";
                }

                // 星を描画するための余白を確保したラベル
                std::string label = sName + startupBadge;
                if (hasIn && hasOut) {
                    label += "    ";
                } else if (hasIn || hasOut) {
                    label += "  ";
                }

                if (ImGui::MenuItem(label.c_str(), nullptr, isSelected)) {
                    if (!isSelected) {
                        // ゲーム実行中（非ポーズ中）の動的変化はキャッシュ保存せず、シーン遷移時に安全のため一時停止にする
                        if (isPaused_) {
                            CacheCurrentSceneAction(currentSceneName);
                        } else {
                            isPaused_ = true;
                        }
                        SceneManager::GetInstance()->ChangeScene(sName, false);
                        EditorNotification::ShowInfo("シーンを「" + sName + "」に切り替えました");
                    }
                }

                // シアン（IN）とオレンジ（OUT）の星を描画
                if (hasIn || hasOut) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 itemMin = ImGui::GetItemRectMin();
                    ImVec2 itemMax = ImGui::GetItemRectMax();
                    float textWidth = ImGui::CalcTextSize((sName + startupBadge).c_str()).x;
                    float lineHeight = ImGui::GetTextLineHeight();
                    float posY = itemMin.y + (itemMax.y - itemMin.y - lineHeight) * 0.5f;
                    float currentX = itemMin.x + textWidth + kTransitionStarSpacingX;

                    if (hasIn) {
                        drawList->AddText(ImVec2(currentX, posY), ImColor(kTransitionColorIn), kTransitionStarSymbol);
                        currentX += ImGui::CalcTextSize(kTransitionStarSymbol).x + kTransitionStarSpacingX;
                    }
                    if (hasOut) {
                        drawList->AddText(ImVec2(currentX, posY), ImColor(kTransitionColorOut), kTransitionStarSymbol);
                    }
                }

                // 各シーンの右クリックでトランジション設定・起動シーン設定
                RenderSceneTransitionContextMenu(sName);

                ImGui::PopID();
            }



            ImGui::Separator();
            ImGui::TextDisabled("※各シーン右クリック: 画面切り替え & 起動シーン設定");
            ImGui::TextColored(kTransitionColorIn, " * : 入場演出");
            ImGui::SameLine();
            ImGui::TextColored(kTransitionColorOut, " * : 退場演出");

            ImGui::Separator();

            // 起動シーン設定サブメニュー
            if (ImGui::BeginMenu("起動シーン設定 (Startup Scene)")) {
                std::string curDebug = SceneManager::GetInstance()->GetDebugStartupSceneName();
                std::string curRelease = SceneManager::GetInstance()->GetReleaseStartupSceneName();

                ImGui::Text("【現在の起動設定】");
                ImGui::BulletText("デバッグ起動: %s", curDebug.c_str());
                ImGui::BulletText("本番(Release)起動: %s", curRelease.c_str());
                ImGui::Separator();

                if (!currentSceneName.empty() && currentSceneName != "None") {
                    std::string setCurDebugLabel = "編集中シーン (" + currentSceneName + ") をデバッグ起動に設定";
                    if (ImGui::MenuItem(setCurDebugLabel.c_str())) {
                        SceneManager::GetInstance()->SetDebugStartupSceneName(currentSceneName);
                        EditorNotification::ShowSuccess("デバッグ起動シーンを「" + currentSceneName + "」に設定しました");
                    }
                    std::string setCurRelLabel = "編集中シーン (" + currentSceneName + ") を本番起動に設定";
                    if (ImGui::MenuItem(setCurRelLabel.c_str())) {
                        SceneManager::GetInstance()->SetReleaseStartupSceneName(currentSceneName);
                        EditorNotification::ShowSuccess("本番起動シーンを「" + currentSceneName + "」に設定しました");
                    }
                    ImGui::Separator();
                }

                if (ImGui::BeginMenu("デバッグ起動シーンを変更")) {
                    for (const auto& s : jsonScenes) {
                        bool sel = (s == curDebug);
                        if (ImGui::MenuItem(s.c_str(), nullptr, sel)) {
                            SceneManager::GetInstance()->SetDebugStartupSceneName(s);
                            EditorNotification::ShowSuccess("デバッグ起動シーンを「" + s + "」に設定しました");
                        }
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("本番(Release)起動シーンを変更")) {
                    for (const auto& s : jsonScenes) {
                        bool sel = (s == curRelease);
                        if (ImGui::MenuItem(s.c_str(), nullptr, sel)) {
                            SceneManager::GetInstance()->SetReleaseStartupSceneName(s);
                            EditorNotification::ShowSuccess("本番起動シーンを「" + s + "」に設定しました");
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            // 2. 新規シーン作成 (New Scene)
            static char newSceneNameBuf[64] = "";
            constexpr float kInputWidth = 110.0f;
            ImGui::SetNextItemWidth(kInputWidth);
            bool enterPressed = ImGui::InputTextWithHint("##NewSceneInput", "Scene name...", newSceneNameBuf, sizeof(newSceneNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            bool newBtnClicked = ImGui::Button("New Scene");

            if ((enterPressed || newBtnClicked) && strlen(newSceneNameBuf) > 0) {
                // ゲーム実行中の動的状態が混入しないよう、一時停止中のみキャッシュ保存
                if (isPaused_) {
                    CacheCurrentSceneAction(currentSceneName);
                } else {
                    isPaused_ = true;
                }

                std::string newName = newSceneNameBuf;
                std::string newPath = kScenesDirectory + "/" + newName + ".json";

                std::vector<std::unique_ptr<GameObject>> emptyObjects;
                SceneSerializer::SaveScene(newPath, newName, emptyObjects);

                SceneManager::GetInstance()->ChangeScene(newName, false);
                EditorNotification::ShowSuccess("新規シーン「" + newName + "」を作成しました");

                newSceneNameBuf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();

            // 3. 現在のシーン保存 (Save Current Scene)
            if (ImGui::MenuItem("Save Current Scene", "Ctrl+S")) {
                SaveCurrentSceneAction(currentSceneName);
            }

            // 4. シーン削除 (Delete Scene) - 最後の1つ以外はどのシーンでも削除可能
            if (ImGui::BeginMenu("Delete Scene")) {
                if (jsonScenes.size() <= kMinSceneCount) {
                    ImGui::TextDisabled("これ以上削除できません（最低1つのシーンが必要です）");
                } else {
                    for (const auto& sName : jsonScenes) {
                        std::string label = sName;
                        if (sName == currentSceneName) {
                            label += " (編集中)";
                        }
                        if (ImGui::MenuItem(label.c_str())) {
                            sSceneNameToDelete = sName;
                            sTriggerOpenDeleteSceneModal = true;
                        }
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("表示###View")) {
            if (ImGui::MenuItem("シーン (編集)", nullptr, showSceneView_)) {
                showSceneView_ = !showSceneView_;
                SyncCurrentWorkspaceWindowState();
            }
            if (ImGui::MenuItem("ゲーム画面", nullptr, showGameView_)) {
                showGameView_ = !showGameView_;
                SyncCurrentWorkspaceWindowState();
            }
            if (ImGui::MenuItem("ヒエラルキー", nullptr, showHierarchy_)) {
                showHierarchy_ = !showHierarchy_;
                SyncCurrentWorkspaceWindowState();
            }
            if (ImGui::MenuItem("インスペクター", nullptr, showInspector_)) {
                showInspector_ = !showInspector_;
                SyncCurrentWorkspaceWindowState();
            }
            if (auto* consolePtr = Log::GetShowConsolePtr()) {
                if (ImGui::MenuItem("コンソール", nullptr, *consolePtr)) {
                    *consolePtr = !*consolePtr;
                    SyncCurrentWorkspaceWindowState();
                }
            }
            if (ImGui::MenuItem("パーティクルスタジオ (GPU)", nullptr, showParticleStudio_)) {
                showParticleStudio_ = !showParticleStudio_;
                SyncCurrentWorkspaceWindowState();
            }
            if (auto* postProcessPtr = PostProcess::GetShowWindowPtr()) {
                if (ImGui::MenuItem("ポストエフェクト", nullptr, *postProcessPtr)) {
                    *postProcessPtr = !*postProcessPtr;
                    SyncCurrentWorkspaceWindowState();
                }
            }
            if (auto* effectListPtr = EffectManager::GetShowListWindowPtr()) {
                if (ImGui::MenuItem("エフェクト一覧", nullptr, *effectListPtr)) {
                    *effectListPtr = !*effectListPtr;
                    SyncCurrentWorkspaceWindowState();
                }
            }
            if (ImGui::MenuItem("パフォーマンス監視", nullptr, showPerfMonitor_)) {
                showPerfMonitor_ = !showPerfMonitor_;
                SyncCurrentWorkspaceWindowState();
            }
            if (ImGui::MenuItem("コンテンツブラウザ", nullptr, showContentBrowser_)) {
                showContentBrowser_ = !showContentBrowser_;
                SyncCurrentWorkspaceWindowState();
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("ウィンドウ###Window")) {
            if (ImGui::MenuItem("フルスクリーン", "F11", isFullscreen_)) {
                ToggleFullscreen(hwnd);
            }
            if (ImGui::MenuItem("ウィンドウ配置を初期化 (Reset Layout)")) {
                ResetWorkspaceLayout(activeWorkspaceIndex_);
                EditorNotification::ShowInfo("ウィンドウ配置を初期化しました");
            }
            if (ImGui::MenuItem("レイアウトを固定 (Lock Layout)", nullptr, isLayoutLocked_)) {
                isLayoutLocked_ = !isLayoutLocked_;
                EditorNotification::ShowInfo(isLayoutLocked_ ? "レイアウトを固定しました" : "レイアウト固定を解除しました");
            }
            
            ImGui::Separator();

            // アスペクト比変更メニュー
            if (!isFullscreen_ && ImGui::BeginMenu("アスペクト比###Aspect Ratio")) {
                const DWORD kOriginalStyle = WS_OVERLAPPEDWINDOW;
                
                auto ChangeWindowSize = [&](int32_t width, int32_t height) {
                    RECT wr = { 0, 0, width, height };
                    AdjustWindowRect(&wr, kOriginalStyle, FALSE);
                    
                    RECT rect{};
                    GetWindowRect(hwnd, &rect);
                    
                    SetWindowPos(hwnd, NULL,
                                 rect.left, rect.top,
                                 wr.right - wr.left,
                                 wr.bottom - wr.top,
                                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                };

                if (ImGui::MenuItem("16:9 (1280x720)", nullptr, currentAspect_ == AspectType::Aspect16_9_Low)) {
                    currentAspect_ = AspectType::Aspect16_9_Low;
                    ChangeWindowSize(1280, 720);
                    EditorNotification::ShowInfo("画面サイズを 16:9 (1280x720) に変更しました");
                }
                if (ImGui::MenuItem("16:9 (1920x1080)", nullptr, currentAspect_ == AspectType::Aspect16_9_High)) {
                    currentAspect_ = AspectType::Aspect16_9_High;
                    ChangeWindowSize(1920, 1080);
                    EditorNotification::ShowInfo("画面サイズを 16:9 (1920x1080) に変更しました");
                }
                if (ImGui::MenuItem("4:3 (960x720)", nullptr, currentAspect_ == AspectType::Aspect4_3)) {
                    currentAspect_ = AspectType::Aspect4_3;
                    ChangeWindowSize(960, 720);
                    EditorNotification::ShowInfo("画面サイズを 4:3 (960x720) に変更しました");
                }
                if (ImGui::MenuItem("1:1 (720x720)", nullptr, currentAspect_ == AspectType::Aspect1_1)) {
                    currentAspect_ = AspectType::Aspect1_1;
                    ChangeWindowSize(720, 720);
                    EditorNotification::ShowInfo("画面サイズを 1:1 (720x720) に変更しました");
                }
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("最大化")) {
                ShowWindow(hwnd, SW_MAXIMIZE);
            }
            if (ImGui::MenuItem("最小化")) {
                ShowWindow(hwnd, SW_MINIMIZE);
            }
            if (ImGui::MenuItem("元のサイズに戻す")) {
                ShowWindow(hwnd, SW_RESTORE);
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ヘルプ###Help")) {
            if (ImGui::MenuItem("総合ヘルプ＆操作マニュアル (User Guide)", "F1")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kShortcuts;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("  ○ ショートカットキー一覧")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kShortcuts;
            }
            if (ImGui::MenuItem("  ○ シーン＆ヒエラルキーの使い方")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kScene;
            }
            if (ImGui::MenuItem("  ○ プレハブ＆アセット管理")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kPrefab;
            }
            if (ImGui::MenuItem("  ○ コンポーネント機能一覧")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kComponents;
            }
            if (ImGui::MenuItem("  ○ スクリプト開発ガイド")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kScripting;
            }
            if (ImGui::MenuItem("  ○ アニメーション＆3D形状ガイド")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kAnimation;
            }
            if (ImGui::MenuItem("  ○ エフェクト＆パーティクルガイド")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kParticles;
            }
            if (ImGui::MenuItem("  ○ エンジン設計＆アーキテクチャ")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kArchitecture;
            }
            if (ImGui::MenuItem("  ○ 全ウィンドウ操作マニュアル")) {
                showShortcutsWindow_ = true;
                helpWindowTargetTab_ = HelpTabIndex::kWindows;
            }
            ImGui::EndMenu();
        }

        // -------------------------------------------------------------
        // Blender風 ワークスペース切り替えタブ (Workspace Tabs)
        // -------------------------------------------------------------
        ImGui::Separator();

        constexpr ImVec4 kWorkspaceActiveBtnColor    = { 0.22f, 0.44f, 0.72f, 1.0f };
        constexpr ImVec4 kWorkspaceActiveTextColor   = { 1.0f, 1.0f, 1.0f, 1.0f };
        constexpr ImVec4 kWorkspaceInactiveBtnColor  = { 0.16f, 0.17f, 0.19f, 0.85f };
        constexpr ImVec4 kWorkspaceInactiveTextColor = { 0.75f, 0.75f, 0.78f, 1.0f };
        constexpr float kWorkspaceTabRounding        = 3.0f;
        constexpr float kAddBtnWidth                 = 26.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kWorkspaceTabRounding);

        // 登録されているワークスペース一覧をループ描画（絵文字なし・純粋テキスト）
        size_t wsToDelete = (std::numeric_limits<size_t>::max)();
        for (size_t i = 0; i < workspaces_.size(); ++i) {
            const auto& ws = workspaces_[i];
            bool isActive = (activeWorkspaceIndex_ == i);

            ImGui::PushID(static_cast<int>(i));
            ImGui::PushStyleColor(ImGuiCol_Button, isActive ? kWorkspaceActiveBtnColor : kWorkspaceInactiveBtnColor);
            ImGui::PushStyleColor(ImGuiCol_Text, isActive ? kWorkspaceActiveTextColor : kWorkspaceInactiveTextColor);

            constexpr float kCloseIconAreaWidth  = 18.0f; // タブ枠内の×アイコン専有幅
            constexpr float kCloseBtnSize        = 14.0f; // ×アイコン領域のサイズ
            constexpr float kCloseBtnMarginRight = 4.0f;  // タブ右端からのマージン
            constexpr float kCloseHoverRounding  = 2.0f;  // ×ホバー時の角丸

            // 基本レイアウト(i == 0)は×ボタン不要のため自動フィット(0.0f)
            // i > 0 (パーティクルスタジオやカスタムページ)は×アイコン領域を含めた1つの幅広ボタン枠を生成
            float totalTabWidth = 0.0f;
            if (i > 0) {
                float textWidth = ImGui::CalcTextSize(ws.name.c_str()).x;
                totalTabWidth = textWidth + ImGui::GetStyle().FramePadding.x * 2.0f + kCloseIconAreaWidth;
            }

            // 1つの統合されたタブボタン枠を描画
            std::string tabBtnLabel = (i == 0) ? ws.name : ("##ws_tab_" + std::to_string(i));
            bool isTabClicked = ImGui::Button(tabBtnLabel.c_str(), ImVec2(totalTabWidth, 0.0f));

            // i > 0 の場合、タブ枠の内部にタイトルテキストと「×」を重ねて描画
            bool isCloseHovered = false;
            if (i > 0) {
                ImVec2 rectMin = ImGui::GetItemRectMin();
                ImVec2 rectMax = ImGui::GetItemRectMax();
                float itemHeight = rectMax.y - rectMin.y;
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                // 1. タブ枠内の左側にタイトルテキストを描画
                float textOffsetY = (itemHeight - ImGui::GetTextLineHeight()) * 0.5f;
                ImVec2 textPos = { rectMin.x + ImGui::GetStyle().FramePadding.x, rectMin.y + textOffsetY };
                drawList->AddText(textPos, ImColor(isActive ? kWorkspaceActiveTextColor : kWorkspaceInactiveTextColor), ws.name.c_str());

                // 2. タブ枠内の右端に「×」アイコン領域を配置
                ImVec2 closeMin = { rectMax.x - kCloseBtnMarginRight - kCloseBtnSize, rectMin.y + (itemHeight - kCloseBtnSize) * 0.5f };
                ImVec2 closeMax = { closeMin.x + kCloseBtnSize, closeMin.y + kCloseBtnSize };

                isCloseHovered = ImGui::IsMouseHoveringRect(closeMin, closeMax);
                if (isCloseHovered) {
                    constexpr ImU32 kCloseHoverBg = IM_COL32(255, 255, 255, 50); // 上品な白色薄ハイライト
                    drawList->AddRectFilled(closeMin, closeMax, kCloseHoverBg, kCloseHoverRounding);
                    ImGui::SetTooltip("この作業ページを閉じる");

                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        wsToDelete = i;
                    }
                }

                // 「x」文字の描画（ホバー時は白く点灯）
                ImVec2 xTextSize = ImGui::CalcTextSize("x");
                ImVec2 xPos = { closeMin.x + (kCloseBtnSize - xTextSize.x) * 0.5f,
                                closeMin.y + (kCloseBtnSize - ImGui::GetTextLineHeight()) * 0.5f };
                constexpr ImU32 kCloseNormalText = IM_COL32(200, 200, 200, 180);
                constexpr ImU32 kCloseHoverText  = IM_COL32(255, 255, 255, 255);
                drawList->AddText(xPos, isCloseHovered ? kCloseHoverText : kCloseNormalText, "x");
            }

            // ×ボタン以外のタブ本体領域クリックでページ切り替え
            if (isTabClicked && !isCloseHovered) {
                SwitchToWorkspace(i);
            }

            // カスタムワークスペースは右クリックでも削除可能
            if (ws.isCustom && ImGui::BeginPopupContextItem("WorkspaceContextMenu")) {
                if (ImGui::MenuItem("このページを削除")) {
                    wsToDelete = i;
                }
                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(2);
            ImGui::PopID();
            ImGui::SameLine();
        }

        if (wsToDelete != (std::numeric_limits<size_t>::max)()) {
            RemoveCustomWorkspace(wsToDelete);
        }

        // [＋] 新規作業ページ追加ボタン
        static bool sOpenNewWsModal = false;
        static char sNewWsNameBuf[64] = "";

        ImGui::PushStyleColor(ImGuiCol_Button, kWorkspaceInactiveBtnColor);
        ImGui::PushStyleColor(ImGuiCol_Text, kWorkspaceInactiveTextColor);
        if (ImGui::Button("+###AddNewWorkspaceBtn", ImVec2(kAddBtnWidth, 0.0f))) {
            sOpenNewWsModal = true;
            sNewWsNameBuf[0] = '\0';
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("新しい作業ページ（ワークスペース）を追加");
        }
        ImGui::PopStyleColor(2);

        ImGui::PopStyleVar();

        // 新規ワークスペース追加モーダルダイアログ
        if (sOpenNewWsModal) {
            ImGui::OpenPopup("新規作業ページの追加##NewWorkspaceModal");
            sOpenNewWsModal = false;
        }

        if (ImGui::BeginPopupModal("新規作業ページの追加##NewWorkspaceModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("新しい作業ページの名前を入力してください：");
            ImGui::SetNextItemWidth(200.0f);
            bool enterPressed = ImGui::InputTextWithHint("##NewWsNameInput", "例: デバッグ用, UI編集...", sNewWsNameBuf, sizeof(sNewWsNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);

            constexpr float kModalBtnWidth = 80.0f;
            if ((ImGui::Button("作成", ImVec2(kModalBtnWidth, 0.0f)) || enterPressed) && strlen(sNewWsNameBuf) > 0) {
                AddCustomWorkspace(sNewWsNameBuf);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(kModalBtnWidth, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // -------------------------------------------------------------
        // メインメニューバーの右端にレイアウト固定アイコンと再生/一時停止ボタンを配置 (マジックナンバー排除)
        // -------------------------------------------------------------
        float menuBarWidth = ImGui::GetWindowWidth();
        constexpr float kPlayPauseBtnWidth = 90.0f;
        constexpr float kLockBtnWidth = 26.0f;
        constexpr float kBtnSpacing = 6.0f;
        constexpr float kPlayPauseRightMargin = 16.0f;
        float rightAlignedPos = menuBarWidth - kPlayPauseBtnWidth - kLockBtnWidth - kBtnSpacing - kPlayPauseRightMargin;

        if (rightAlignedPos > ImGui::GetCursorPosX()) {
            ImGui::SameLine(rightAlignedPos);
        } else {
            ImGui::SameLine();
        }

        // 南京錠（レイアウト固定）トグルボタン
        RenderLockToggleButton(isLayoutLocked_);
        ImGui::SameLine();

        if (isPaused_) {
            if (ImGui::Button("再生 ▶", ImVec2(kPlayPauseBtnWidth, 0.0f))) {
                isPaused_ = false;
                if (gameViewWindow_) {
                    gameViewWindow_->TriggerPopAnimation(PopAnimation::Type::Play);
                }
            }
        } else {
            if (ImGui::Button("一時停止 ||", ImVec2(kPlayPauseBtnWidth, 0.0f))) {
                isPaused_ = true;
                if (gameViewWindow_) {
                    gameViewWindow_->TriggerPopAnimation(PopAnimation::Type::Pause);
                }
            }
        }

        ImGui::EndMainMenuBar();
    }

    // シーン削除確認モーダルのオープン要求
    if (sTriggerOpenDeleteSceneModal) {
        ImGui::OpenPopup(kDeleteSceneModalId);
        sTriggerOpenDeleteSceneModal = false;
    }

    // シーン削除確認モーダルダイアログ
    if (ImGui::BeginPopupModal(kDeleteSceneModalId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("シーン「%s」を削除しますか？", sSceneNameToDelete.c_str());
        constexpr ImVec4 kWarningTextColor = { 1.0f, 0.4f, 0.4f, 1.0f };
        ImGui::TextColored(kWarningTextColor, "※シーンファイル (.json) が完全に削除されます。この操作は取り消せません。");

        std::string currentScene = SceneManager::GetInstance()->GetCurrentSceneName();
        if (sSceneNameToDelete == currentScene) {
            ImGui::TextDisabled("（編集中シーンのため、削除後は残っているシーンへ自動的に遷移します）");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        constexpr float kConfirmBtnWidth = 100.0f;
        if (ImGui::Button("削除", ImVec2(kConfirmBtnWidth, 0.0f))) {
            std::string pathToRemove = kScenesDirectory + "/" + sSceneNameToDelete + ".json";
            std::error_code ec;
            bool removed = std::filesystem::remove(pathToRemove, ec);

            // 削除したシーンの一時キャッシュも破棄
            SceneMemoryCache::GetInstance()->ClearCache(sSceneNameToDelete);

            if (removed) {
                Log::Write(std::format(L"[Scene] シーンファイル「{}」を正常に削除しました。", ConvertString(pathToRemove)));
            } else {
                Log::Write(std::format(L"[Scene] シーンファイル「{}」の削除に失敗しました（理由: {}）。",
                    ConvertString(pathToRemove), ConvertString(ec.message())));
            }

            // 編集中シーンを削除した場合は、自動保存を行わずに安全に残りのシーンへフォールバック遷移
            if (sSceneNameToDelete == currentScene) {
                std::string fallbackTarget = "";

                // 残っているシーンの中からTitleがあればTitle優先、なければ先頭シーン
                for (const auto& entry : std::filesystem::directory_iterator(kScenesDirectory)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".json") {
                        std::string stem = entry.path().stem().string();
                        if (stem == kFallbackPrioritySceneName) {
                            fallbackTarget = kFallbackPrioritySceneName;
                            break;
                        } else if (fallbackTarget.empty()) {
                            fallbackTarget = stem;
                        }
                    }
                }

                if (fallbackTarget.empty()) {
                    fallbackTarget = "Game";
                }

                SceneManager::GetInstance()->ChangeScene(fallbackTarget, false);
            }

            sSceneNameToDelete.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(kConfirmBtnWidth, 0.0f))) {
            sSceneNameToDelete.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void DebugEditor::DrawShortcutsWindow() {
    constexpr float kDefaultWindowWidth = 720.0f;
    constexpr float kDefaultWindowHeight = 600.0f;
    constexpr float kColWidthKey = 0.36f;
    constexpr float kColWidthDesc = 0.64f;

    constexpr ImVec4 kAccentKeyColor = { 0.4f, 0.85f, 1.0f, 1.0f };   // シアン（キー・項目強調）
    constexpr ImVec4 kAccentTipColor = { 0.4f, 0.95f, 0.6f, 1.0f };   // エメラルドグリーン（機能・ヒント）
    constexpr ImVec4 kAccentWarnColor = { 1.0f, 0.8f, 0.3f, 1.0f };  // アンバー（注意・重要）
    constexpr ImVec4 kAccentApiColor = { 0.85f, 0.65f, 1.0f, 1.0f };  // パープル（API・クラス）

    // メインビューポートの作業領域全体を取得して全画面固定フィット
    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(mainViewport->WorkSize, ImGuiCond_Always);

    constexpr ImGuiWindowFlags kHelpWindowFlags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("ZuizuiEngine 総合ヘルプ＆操作マニュアル###HelpWindow", &showShortcutsWindow_, kHelpWindowFlags)) {
        // Escキーでも瞬時に閉じられる親切設計
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showShortcutsWindow_ = false;
        }

        ImGui::TextDisabled("ZuizuiEngine の基本操作、各種エディタ機能、プレハブシステム、スクリプト開発ガイドの総合マニュアルです。");
        ImGui::Separator();
        ImGui::Spacing();

        auto DrawHelpTable = [](const char* tableId, const char* col1Name, const char* col2Name, float col1Width,
                                const std::vector<std::pair<std::string, std::string>>& items,
                                const ImVec4& keyCol) {
            constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | 
                                                    ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

            if (ImGui::BeginTable(tableId, 2, kTableFlags)) {
                ImGui::TableSetupColumn(col1Name, ImGuiTableColumnFlags_WidthStretch, col1Width);
                ImGui::TableSetupColumn(col2Name, ImGuiTableColumnFlags_WidthStretch, 1.0f - col1Width);
                ImGui::TableHeadersRow();

                for (const auto& [key, desc] : items) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(keyCol, "%s", key.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", desc.c_str());
                }
                ImGui::EndTable();
            }
        };

        // アイコン描画ヘルパー (TextureManager から GPU ハンドルを取得してインライン表示)
        auto texMgr = TextureResource::GetTextureManager();
        auto DrawIcon = [texMgr](const std::string& iconKey, float size = 18.0f) {
            if (!texMgr) return;
            auto handle = texMgr->GetGpuHandle(iconKey);
            if (handle.ptr != 0) {
                ImGui::Image((ImTextureID)handle.ptr, ImVec2(size, size));
                ImGui::SameLine();
            }
        };

        // =============================================================
        // 検索ツールバー（完全一致チェックボックス ＆ 右寄せ閉じるボタン）
        // =============================================================
        constexpr float kSearchInputWidth = 380.0f;
        constexpr float kClearBtnWidth = 24.0f;
        constexpr float kCloseBtnWidth = 140.0f;
        constexpr ImVec4 kAccentTitleMatchCol   = { 0.3f, 0.85f, 1.0f, 1.0f }; // シアン
        constexpr ImVec4 kAccentHeadingMatchCol = { 0.4f, 0.95f, 0.6f, 1.0f }; // グリーン
        constexpr ImVec4 kAccentBodyMatchCol    = { 1.0f, 0.8f, 0.3f, 1.0f };  // オレンジ

        ImGui::SetNextItemWidth(kSearchInputWidth);
        ImGui::InputTextWithHint("##HelpSearchInput", "ヘルプ内を検索 (例: パーティクル, Fov, Radius, Center, 保存)...",
                                 helpSearchBuffer_, kHelpSearchBufferSize);
        ImGui::SameLine();
        if (helpSearchBuffer_[0] != '\0') {
            if (ImGui::Button("x##ClearHelpSearch", ImVec2(kClearBtnWidth, 0.0f))) {
                helpSearchBuffer_[0] = '\0';
            }
            ImGui::SameLine();
        }
        ImGui::Checkbox("完全一致 (Exact Match)", &helpSearchExactMatch_);

        // 右寄せで「[x] 閉じる (Close)」ボタンを配置
        float availX = ImGui::GetContentRegionAvail().x;
        if (availX > kCloseBtnWidth + 10.0f) {
            ImGui::SameLine(ImGui::GetWindowWidth() - kCloseBtnWidth - ImGui::GetStyle().WindowPadding.x);
            if (ImGui::Button("[x] 閉じる (Close)", ImVec2(kCloseBtnWidth, 0.0f))) {
                showShortcutsWindow_ = false;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const bool isSearching = (helpSearchBuffer_[0] != '\0');
        if (isSearching) {
            auto searchResults = EngineHelpDatabase::Search(helpSearchBuffer_, helpSearchExactMatch_);
            ImGui::TextColored(kAccentTipColor, "【検索結果】 %zu 件の候補が見つかりました（優先順位: タイトル ＞ 見出し ＞ 本文）", searchResults.size());
            ImGui::Spacing();

            if (searchResults.empty()) {
                ImGui::TextDisabled("一致するヘルプ項目が見つかりませんでした。別のキーワードでお試しください。");
                if (helpSearchExactMatch_) {
                    ImGui::TextDisabled("※ 「完全一致」のチェックを外すと、部分一致でヒットする場合があります。");
                }
            } else {
                if (ImGui::BeginChild("HelpSearchResultsScroll", ImVec2(0.0f, 0.0f), false)) {
                    for (size_t i = 0; i < searchResults.size(); ++i) {
                        const auto& res = searchResults[i];
                        ImGui::PushID(static_cast<int>(i));

                        // マッチ種別バッジ
                        if (res.matchType == HelpMatchType::Title) {
                            ImGui::TextColored(kAccentTitleMatchCol, "[タイトル一致]");
                        } else if (res.matchType == HelpMatchType::Heading) {
                            ImGui::TextColored(kAccentHeadingMatchCol, "[見出し一致]");
                        } else {
                            ImGui::TextColored(kAccentBodyMatchCol, "[本文一致]");
                        }
                        ImGui::SameLine();

                        // 所属カテゴリバッジ
                        ImGui::TextDisabled("[%s]", res.article->category.c_str());
                        ImGui::SameLine();

                        // タイトル表示
                        ImGui::TextColored(kAccentKeyColor, "%s", res.article->title.c_str());
                        ImGui::SameLine();

                        // ジャンプボタン
                        if (ImGui::SmallButton("このタブを開く ▶")) {
                            helpWindowTargetTab_ = res.article->targetTabIndex;
                            helpSearchBuffer_[0] = '\0'; // 検索クリアして該当タブを開く
                        }

                        // 見出し・スニペット抜粋表示
                        ImGui::Indent(20.0f);
                        if (res.section) {
                            ImGui::TextColored(kAccentHeadingMatchCol, "◆ %s", res.section->heading.c_str());
                        }
                        ImGui::TextWrapped("%s", res.matchedSnippet.c_str());
                        ImGui::Unindent(20.0f);

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                }
            }
        } else {
        if (ImGui::BeginTabBar("HelpTabBar", ImGuiTabBarFlags_None)) {
            // -------------------------------------------------------------
            // Tab 1: ○ ショートカット (Shortcuts)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab1Flags = (helpWindowTargetTab_ == HelpTabIndex::kShortcuts) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ ショートカット一覧", nullptr, tab1Flags)) {
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("一般操作 (General)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> generalShortcuts = {
                        { "Ctrl + S", "現在のシーンを保存 (Save Current Scene)" },
                        { "Ctrl + Z", "直前の操作を元に戻す (Undo)" },
                        { "Ctrl + Y / Ctrl + Shift + Z", "取り消した操作をやり直す (Redo)" },
                        { "F", "選択中のオブジェクトへカメラをフォーカス" },
                        { "F1", "この総合ヘルプマニュアルの開閉" },
                        { "F11", "フルスクリーン / ウィンドウモードの切り替え" },
                        { "Alt + F4", "アプリケーションの終了（保存確認あり）" }
                    };
                    DrawHelpTable("GeneralTable", "操作 / キー", "機能・説明", kColWidthKey, generalShortcuts, kAccentKeyColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("シーン・カメラ操作 (Scene View & Camera)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> cameraShortcuts = {
                        { "右ドラッグ (Right Drag)", "カメラの視線回転（Look Around / FPS視点）" },
                        { "右ドラッグ + W / S", "カメラの前進 / 後退移動" },
                        { "右ドラッグ + A / D", "カメラの左 / 右平行移動" },
                        { "右ドラッグ + E / Q", "カメラの上昇 / 下降移動" },
                        { "マウスホイール回転", "前後ズーム (Zoom In / Out)" },
                        { "中クリックドラッグ", "カメラの平行移動 (Pan)" }
                    };
                    DrawHelpTable("CameraTable", "操作 / マウス", "機能・説明", kColWidthKey, cameraShortcuts, kAccentKeyColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("ギズモ・オブジェクト操作 (Gizmo & Manipulation)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> gizmoShortcuts = {
                        { "ヒエラルキー 左クリック", "平行移動ギズモ (Translate) で選択" },
                        { "ヒエラルキー 中クリック", "回転ギズモ (Rotate) で選択" },
                        { "ヒエラルキー 右クリック", "拡縮ギズモ (Scale) で選択" },
                        { "ヒエラルキー ダブルクリック", "オブジェクトを選択してカメラフォーカス (F)" },
                        { "ギズモ軸ドラッグ", "選択オブジェクトのTransform操作 (Undo/Redo対応)" },
                        { "ヒエラルキー [x] ボタン", "オブジェクトの削除 (Undo/Redo対応)" }
                    };
                    DrawHelpTable("GizmoTable", "操作 / 操作方法", "機能・説明", kColWidthKey, gizmoShortcuts, kAccentKeyColor);
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 2: ○ シーン＆ヒエラルキー (Scene & Hierarchy)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab2Flags = (helpWindowTargetTab_ == HelpTabIndex::kScene) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ シーン＆ヒエラルキー", nullptr, tab2Flags)) {
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("シーン管理 (Scene Management)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> sceneFeatures = {
                        { "シーン切り替え", "メニューバー「シーン」から、保存済みの各シーン (.json) をクリックして即座に遷移します。" },
                        { "新規シーン作成", "メニューバー「シーン」内の「New Scene」入力欄に名前を入力して Enter またはボタンクリックで空の新規シーンを生成します。" },
                        { "シーン保存 (Ctrl+S)", "現在のシーンに配置されているすべてのオブジェクトやコンポーネント設定を保存します。" },
                        { "シーン削除", "「Delete Scene」からシーンを完全に削除します。確認モーダルが表示され、誤操作を防ぎます。編集中シーンを削除した場合は自動で安全なシーンへフォールバック遷移します。" }
                    };
                    DrawHelpTable("SceneTable", "機能", "説明・使い方", 0.28f, sceneFeatures, kAccentTipColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("画面切り替え演出 (Scene Transition)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextWrapped("メニューバー「シーン」内のシーン一覧で、各シーン名を右クリックするとトランジション演出を設定できます。");
                    ImGui::Spacing();

                    const std::vector<std::pair<std::string, std::string>> transitionFeatures = {
                        { "入場演出 (IN) *", "シーン開始時に再生されるフェードイン演出。設定されたシーンにはシアンの星印 (*) が表示されます。" },
                        { "退場演出 (OUT) *", "シーン終了・遷移時に再生されるフェードアウト演出。設定されたシーンにはオレンジの星印 (*) が表示されます。" }
                    };
                    DrawHelpTable("TransitionTable", "演出タイプ", "演出内容と識別マーク", 0.28f, transitionFeatures, kAccentWarnColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("ヒエラルキー操作 (Hierarchy & Creation)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DrawIcon("sys_icon_scene");
                    ImGui::TextColored(kAccentTipColor, "【ゲームオブジェクトの新規作成手順】");
                    ImGui::BulletText("1. ヒエラルキーウィンドウの余白部分を【右クリック】します。");
                    ImGui::BulletText("2. 表示されたコンテキストメニューから【+ Create GameObject】にカーソルを合わせます。");
                    ImGui::BulletText("3. 作成したい種類（Empty, 3D Mesh, 2D Sprite, Camera, Light, Text 3D 等）をクリックします。");
                    ImGui::BulletText("4. シーン中央に即座にオブジェクトが追加され、インスペクターから名前・位置・形状を編集できます。");
                    ImGui::Spacing();

                    const std::vector<std::pair<std::string, std::string>> creationItems = {
                        { "+ Create Empty", "コンポーネントを持たない空のGameObjectを作成。フォルダ分けや親ノード、位置管理に便利です。" },
                        { "+ 3D Mesh (Cube等)", "MeshRendererコンポーネント付きの3D形状を作成。デフォルトはCubeで、インスペクターでSphereやCylinder等に変更可能。" },
                        { "+ 2D Sprite", "SpriteRendererコンポーネント付きの2D画像オブジェクトを作成。UIや2Dキャラクターに最適です。" },
                        { "+ Camera", "Cameraコンポーネント付きのカメラを作成。メインカメラやサブビュー、カットシーン用カメラに利用します。" },
                        { "+ Light", "LightComponent付きの光源を作成。DirectionalLight（太陽光）やPointLight（松明・ランプ）を設定できます。" },
                        { "+ Text 3D / 2D", "TextRendererコンポーネント付きの文字オブジェクトを作成。頭上ネームや案内看板を3D空間に配置できます。" }
                    };
                    DrawHelpTable("CreationTable", "作成メニュー項目", "生成されるオブジェクトと主な用途", 0.28f, creationItems, kAccentTipColor);

                    ImGui::Spacing();

                    const std::vector<std::pair<std::string, std::string>> hierarchyFeatures = {
                        { "検索フィルター", "ヒエラルキー上部のテキスト入力で、シーン内のオブジェクトを名前で即座に絞り込み検索できます。" },
                        { "オブジェクトの削除 [x]", "各項目の右側にある赤い [x] ボタンをクリックすると、確認なしで即座に安全削除（Undo対応）されます。" },
                        { "ドラッグ＆ドロップ参照", "ヒエラルキーのオブジェクト項目をドラッグして、インスペクターの「Target Object」等のスロットにドロップすると、そのオブジェクトの参照が自動バインドされます。" },
                        { "シーン保存 (Ctrl+S)", "作成・編集したすべてのオブジェクトの配置やコンポーネント設定を現在のシーンファイル (.json) に保存します。" }
                    };
                    DrawHelpTable("HierarchyTable", "機能", "説明・使い方", 0.28f, hierarchyFeatures, kAccentKeyColor);
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 3: ○ プレハブ＆アセット管理 (Prefab & Assets)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab3Flags = (helpWindowTargetTab_ == HelpTabIndex::kPrefab) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ プレハブ＆アセット管理", nullptr, tab3Flags)) {
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("プレハブ機能 (Prefab System)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextWrapped("作成したゲームオブジェクトをテンプレート（プレハブ）として保存し、何度でも再利用・動的生成できます。");
                    ImGui::Spacing();

                    const std::vector<std::pair<std::string, std::string>> prefabFeatures = {
                        { "Save as Prefab", "ヒエラルキーでオブジェクトを選択し、インスペクターの「Save as Prefab」ボタンを押すと、resources/Prefabs/{名前}.prefab に一発保存されます。" },
                        { "DND配置", "コンテンツブラウザ内の .prefab ファイルをシーンビューまたはヒエラルキーへドラッグ＆ドロップすると、その場に実体化配置されます。" },
                        { "スクリプトからの動的生成", "ScriptComponent::Instantiate(\"Prefab名\", pos, rot); を呼ぶことで、弾や敵キャラクターをゲーム実行中に動的生成できます。" },
                        { "動的生成オブジェクトの破棄", "ScriptComponent::Destroy(gameObject); を呼ぶことで、寿命を迎えた弾や倒された敵を安全にシーンから破棄します。" }
                    };
                    DrawHelpTable("PrefabTable", "操作・API", "機能・説明", 0.32f, prefabFeatures, kAccentTipColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("コンテンツブラウザ (Content Browser)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> browserFeatures = {
                        { "カテゴリ切り替え", "上部の「resources」と「Scripts」ボタンで、リソースフォルダとスクリプトフォルダをワンクリックで切り替えられます。" },
                        { "フォルダ履歴・移動", "「<」「>」ボタンでフォルダの閲覧履歴を行き来できます。アドレスバーのパスをクリックして上の階層に戻ることも可能です。" },
                        { "アセット認識", "テクスチャ(.png/.jpg)、3Dモデル(.obj/.gltf)、音声(.wav/.mp3)、シーン(.json)、スクリプト(.cpp/.h)、プレハブ(.prefab)をアイコン付きで視覚的に管理できます。" },
                        { "スクリプトのDNDアタッチ", "コンテンツブラウザ内のスクリプト (.cpp) をインスペクターへドラッグ＆ドロップすると、選択中オブジェクトへ直接アタッチできます。" }
                    };
                    DrawHelpTable("BrowserTable", "機能", "説明・使い方", 0.28f, browserFeatures, kAccentKeyColor);
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 4: ○ コンポーネント機能一覧 (Components)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab4Flags = (helpWindowTargetTab_ == HelpTabIndex::kComponents) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ コンポーネント機能一覧", nullptr, tab4Flags)) {
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("コンポーネントの基本操作", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> compOps = {
                        { "+ Add Component", "インスペクター下部のボタンから、レンダラー、コライダー、ライト、演出などの各種コンポーネントを追加します。" },
                        { "コンポーネントの削除 [x]", "各コンポーネントのヘッダー右上にある [x] ボタンをクリックすると、GPU処理を安全にフラッシュ同期しながら即座に削除されます。" },
                        { "Enabled チェックボックス", "コンポーネントの有効 / 無効を個別かつリアルタイムに切り替えます。" }
                    };
                    DrawHelpTable("CompOpsTable", "操作", "機能・説明", 0.28f, compOps, kAccentTipColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("2D / 3D 座標補正ヘルパー", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> posFixes = {
                        { "画面中央に配置 (2D)", "2Dオブジェクト（スプライト等）選択時に表示され、画面解像度の中央 (X: 640, Y: 360) にワンクリックで配置します。" },
                        { "3D原点 (0, 0, 0) にリセット", "3Dオブジェクトが誤って2D座標（X: 640等）にある場合、黄色の警告テキストと強調ボタンが表示され、即座に原点へ戻せます。" }
                    };
                    DrawHelpTable("PosFixTable", "ボタン・警告", "機能・説明", 0.32f, posFixes, kAccentWarnColor);
                }

                ImGui::Spacing();

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("標準コンポーネント一覧 (Built-in)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> builtInComps = {
                        { "MeshRenderer", "3Dモデル (.obj, .gltf) を描画。マテリアル色やテクスチャの変更、ライティング受光設定が可能です。" },
                        { "SpriteRenderer", "2D画像 (.png) をスプライトとして描画。カラー乗算やアンカーポイントの調整が可能です。" },
                        { "BoxCollider / SphereCollider", "直方体・球の当たり判定。SceneViewWindowでデバッグワイヤーフレーム（緑・黄）がリアルタイム描画されます。" },
                        { "AudioSource", "効果音やBGMの再生。音量、ピッチ、ループ再生のトグル、再生トリガーをインスペクターからテスト可能です。" },
                        { "LightComponent", "ポイントライトやディレクショナルライトなどの光源をオブジェクトに付与します。" },
                        { "ParticleEffect", "パーティクル演出をオブジェクトに付与し、追従・ループ・自動再生を制御します。" },
                        { "演出系コンポーネント (Rotator / SinOscillator / Blink / CameraOrbit)", "自動回転(Rotator)、浮遊往復(SinOscillator)、明滅(Blink)、対象周囲旋回(CameraOrbit)などの標準演出コンポーネントです。" }
                    };
                    DrawHelpTable("BuiltInTable", "コンポーネント", "役割・主な機能", 0.30f, builtInComps, kAccentKeyColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("インスペクター内部変数・プロパティ完全リファレンス", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(kAccentTipColor, "各コンポーネントのインスペクターに表示される具体的な設定変数と調整項目の一覧です。");
                    ImGui::Spacing();

                    const std::vector<std::pair<std::string, std::string>> inspectorVariables = {
                        { "Transform : Position / translate", "ワールド空間での座標(X, Y, Z)。2Dは画面中央(640, 360)、3Dは原点(0, 0, 0)基準。" },
                        { "Transform : Rotation / rotate", "各軸まわりの回転角度(Degree)。オブジェクトの向きを決定。" },
                        { "Transform : Scale / scale", "各軸の拡大縮小倍率(X, Y, Z)。1.0が等倍。" },
                        { "MeshRenderer : Mesh Type", "Cube, Sphere, Cylinder, Cone, Pyramid, Ring, Custom Model 等の幾何学形状切り替え。" },
                        { "MeshRenderer : Model / Texture Key", "外部モデル(.gltf/.obj)やテクスチャ(.png)のアセットパス。DND対応。" },
                        { "MeshRenderer : Material Color / Lighting", "ベースマテリアル乗算色(RGBA)と、光源の光を受けるかのトグル(Receive Lighting)。" },
                        { "SpriteRenderer : Size / Anchor Point", "幅と高さ(pixels)、および描画基準点(0.5, 0.5で中心、0, 0で左上)。" },
                        { "SpriteRenderer : Flip X / Flip Y", "スプライト画像の水平（左右）・垂直（上下）反転トグル。" },
                        { "BoxCollider : Center / Size", "オブジェクト中心からの相対中心オフセット(X,Y,Z)と、直方体の各軸大きさ。" },
                        { "SphereCollider : Center / Radius", "球体当たり判定の中心オフセット(X,Y,Z)と、球の半径(Radius)。" },
                        { "Collider : Is Trigger / Show Gizmo", "物理衝突するか、すり抜け検知(Trigger)にするか、およびワイヤーフレーム表示。" },
                        { "Light : Light Type / Intensity / Color", "Directional(太陽光)/Point(点光源)の切替、光の色、照度の強さ、減衰(Decay)。" },
                        { "Camera : Fov / Near / Far / ClearColor", "視野角(画角/Fov)、描画手前/奥の限界距離、背景クリアカラー、主カメラ化(Is Main)。" },
                        { "AudioSource : Sound Key / Volume / Pitch", "再生音声ファイル、音量(Volume: 0.0〜1.0)、音程・再生速度(Pitch: 0.5〜2.0)、ループ。" },
                        { "ParticleEffect : Effect Name / Offset", "再生エフェクト識別名、親からの相対発生オフセット、Loop、AutoPlay、テスト再生。" },
                        { "TextRenderer : Text / Font Size / Billboard", "表示文字列、フォントサイズ、文字色、カメラ正対トグル(Billboard)。" },
                        { "演出系 : RotationSpeed / Axis / Amplitude", "Rotatorの毎秒回転速度、SinOscillatorの振動軸・揺れ幅・周波数、Blinkの明滅速度。" }
                    };
                    DrawHelpTable("InspectorVarsTable", "コンポーネント : 変数名 (プロパティ)", "型・単位・役割と調整効果", 0.34f, inspectorVariables, kAccentWarnColor);
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 5: ○ スクリプト開発ガイド (Scripting Guide)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab5Flags = (helpWindowTargetTab_ == HelpTabIndex::kScripting) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ スクリプト開発ガイド", nullptr, tab5Flags)) {
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("新規スクリプトの作成手順", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(kAccentTipColor, "【手順】");
                    ImGui::BulletText("1. ヒエラルキーでスクリプトをアタッチしたい GameObject を選択");
                    ImGui::BulletText("2. インスペクター下部の「+ Add Component」ボタンをクリック");
                    ImGui::BulletText("3. メニュー最下部の「+ New Script... (新規スクリプト作成)」を選択");
                    ImGui::BulletText("4. スクリプト名（例: PlayerController）を入力し「作成してアタッチ」をクリック");
                    ImGui::BulletText("5. Scripts/ フォルダ内に丁寧なコメント入りの .h と .cpp が自動生成され、即アタッチされます");
                    ImGui::BulletText("6. Visual Studio 等でソリューションを開き、ロジックを実装します");
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("スクリプトの基本ライフサイクル", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> lifecycles = {
                        { "Awake()", "コンポーネント生成直後に1度だけ呼ばれます。オーナー情報の確認や自身のメンバ初期化を行います。" },
                        { "Start()", "初回更新フレームの直前に1度だけ呼ばれます。他オブジェクトの検索や配置完了後の初期値取得を行います。" },
                        { "OnUpdate()", "毎フレーム呼ばれる主更新ロジックです。キー入力の判定、移動処理、状態遷移、タイマー更新等を記述します。" },
                        { "OnInspectorGUI()", "インスペクター描画処理です。ImGuiのウィジェット（DragFloat, Checkbox等）を使って独自変数を公開・編集できます。" },
                        { "OnGUI()", "ゲーム画面 (GameView) 上へのHUD描画処理です。画面上にテキストやスコア、デバッグ情報を直接描画できます。" },
                        { "OnDestroy()", "コンポーネント破棄時に呼ばれる後始末処理です。動的確保したリソースの解放などを行います。" }
                    };
                    DrawHelpTable("LifecycleTable", "関数名", "呼び出しタイミングと主な役割", 0.28f, lifecycles, kAccentApiColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("最初から利用できる標準変数・便利API", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> scriptApis = {
                        { "GetOwner()", "スクリプトがアタッチされている GameObject* を取得。GetOwner()->transform_.translate で自身の座標操作が可能です。" },
                        { "targetObject_", "インスペクターの「Target Object」スロットから登録された外部の GameObject* です。" },
                        { "targetUUID_", "targetObject_ の識別UUID。シーン保存およびロード後の自動復元に使用されます。" },
                        { "Instantiate(prefab, pos, rot)", "プレハブから動的にGameObjectを生成します。弾の発射や敵の出現に使用します。" },
                        { "Destroy(gameObject)", "指定したGameObjectをシーンから安全に破棄します。" },
                        { "ShakeCamera(intensity, duration)", "指定した強さと時間でカメラシェイク（画面揺れ演出）を即座に発生させます。" },
                        { "DrawGameObjectSlot(label, uuid, obj)", "インスペクターにドラッグ＆ドロップ受け取り対応のオブジェクト参照スロットを描画します。" }
                    };
                    DrawHelpTable("ScriptApiTable", "変数・API", "機能・説明", 0.35f, scriptApis, kAccentKeyColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("コーディングの重要ルール＆Tips", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(kAccentWarnColor, "■ REGISTER_SCRIPT マクロの必須性:");
                    ImGui::TextWrapped("  生成された .cpp の上部に書かれている REGISTER_SCRIPT(クラス名) は削除しないでください。エディタがスクリプトを自動認識・生成するために必須です。");
                    ImGui::Spacing();

                    ImGui::TextColored(kAccentTipColor, "■ マジックナンバーの排除:");
                    ImGui::TextWrapped("  コード内に直接「5.0f」や「0.02f」などの数値を埋め込まず、メンバ変数（例: moveSpeed_）や constexpr 定数として定義し、OnInspectorGUI() で調整可能にすることをお勧めします。");
                    ImGui::Spacing();

                    ImGui::TextColored(kAccentKeyColor, "■ インスペクターでの色分け・カード表示:");
                    ImGui::TextWrapped("  作成したスクリプトは、インスペクター上でスクリプト固有のカラーヘッダーとカード枠線で囲まれます。末尾に「▲ [End of クラス名] ▲」が表示されるため、どこまでがそのスクリプトの設定なのか一目で把握できます。");
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 6: ○ アニメーション＆3D形状 (Animation & 3D Shapes)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab6Flags = (helpWindowTargetTab_ == HelpTabIndex::kAnimation) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ アニメーション＆3D形状", nullptr, tab6Flags)) {
                ImGui::Spacing();

                // ヘッダー情報（モデルアイコン付き）
                constexpr float kHeaderIconSize = 24.0f;
                constexpr float kSectionIconSize = 16.0f;
                DrawIcon("sys_icon_model", kHeaderIconSize);
                ImGui::TextColored(kAccentTipColor, "アニメーション機能（GPUスキニング対応）＆ 3D形状（プリミティブメッシュ）ガイド");
                ImGui::TextDisabled("Assimpによるキーフレーム補間、スケルトン可視化、GPUスキニング、およびエンジン標準の全3D形状の使い方です。");
                ImGui::Separator();
                ImGui::Spacing();

                // 1. アニメーション機能の使い方
                if (ImGui::CollapsingHeader("アニメーション機能の使い方 (Animation Guide)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DrawIcon("sys_icon_folder", kSectionIconSize);
                    ImGui::TextColored(kAccentKeyColor, "【対応フォーマット & ファイル配置】");
                    ImGui::BulletText("Assimp 対応の .gltf / .glb（推奨）および .obj をサポートしています。");
                    ImGui::BulletText("アニメーション付きモデルは resources/ 直下のフォルダ（例: resources/AnimationHuman/walk.gltf）に配置します。");
                    ImGui::Spacing();

                    DrawIcon("sys_icon_model", kSectionIconSize);
                    ImGui::TextColored(kAccentKeyColor, "【インスペクターでの操作手順】");
                    ImGui::BulletText("1. ヒエラルキーでアニメーションさせたい GameObject を選択します。");
                    ImGui::BulletText("2. MeshRenderer の「Mesh Type」を「10: Custom Model」に設定します。");
                    ImGui::BulletText("3. 「Model Key」および「Animation Key」に .gltf のパス（例: resources/AnimationHuman/walk.gltf）を入力します。");
                    ImGui::BulletText("4. アニメーション操作パネル（再生/一時停止/停止/シークバー）が表示され、リアルタイムに動作します。");
                    ImGui::Spacing();

                    const std::vector<std::pair<std::string, std::string>> animFeatures = {
                        { "再生 ▶ / 一時停止 ⏸", "アニメーションの再生進行・一時停止をワンクリックで切り替えます。" },
                        { "停止 ⏹", "アニメーションの進行を停止し、先頭（0.00 秒）に巻き戻します。" },
                        { "Loop (ループ再生)", "チェックをONにすると終了後に先頭から繰り返し再生。OFFにすると最後のポーズで停止します。" },
                        { "Timeline (シークバー)", "スライダーをマウスでドラッグして、任意のアニメーションフレーム・時間へ即座に移動できます。" },
                        { "Show Skeleton (Bones)", "チェックをONにすると、ボーン（関節の親子階層）が骨組みラインとしてリアルタイム可視化されます。" },
                        { "GPU スキニング (Skinning)", "ボーンの動きに合わせて外側のメッシュ（皮膚・衣服）が自然に曲がります。GPU頂点シェーダで超高速に変形計算されます。" }
                    };
                    DrawHelpTable("AnimTable", "機能・UI", "操作と動作内容", 0.28f, animFeatures, kAccentTipColor);
                }

                ImGui::Spacing();

                // 2. 3D形状（メッシュタイプ）一覧と特徴
                if (ImGui::CollapsingHeader("3D形状（プリミティブメッシュ）一覧と特徴 (3D Primitives)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextWrapped("MeshRenderer コンポーネントの「Mesh Type」で選択できる標準の幾何学形状（プリミティブ）一覧です。用途に合わせて自由に選択・配置できます。");
                    ImGui::Spacing();

                    const std::vector<std::pair<std::string, std::string>> meshTypeFeatures = {
                        { "0: Cube (立方体)", "地面、壁、足場、障害物、木箱、ブロック建築など汎用的な直方体オブジェクトに最適です。" },
                        { "1: Sphere (球体)", "弾丸、オーブ、ボール、惑星、全方位の当たり判定や魔法玉に最適です。" },
                        { "2: Cylinder (円柱)", "柱、支柱、パイプ、ビーム照射エフェクト、筒状のギミックやバリアに最適です。" },
                        { "3: Cone (円錐)", "スパイク（トゲ）、スポットライトコーン、マーカー、矢印の先端などに最適です。" },
                        { "4: Pyramid (四角錐)", "ピラミッド型オブジェクト、クリスタル、建造物の屋根、浮遊鉱石に最適です。" },
                        { "5: TriangularPyramid (三角錐)", "四面体結晶、鋭利なクリスタル、攻撃ヒット時の破片エフェクトなどに最適です。" },
                        { "6: Hemisphere (半球)", "ドーム状バリア、シェルター、丘陵、着弾時の衝撃波半球エフェクトに最適です。" },
                        { "7: Ring (立体リング)", "トーラス型ワープゲート、ポータル、回転リングエフェクトに最適です。" },
                        { "8: FlatRing (平面リング)", "足元の魔法陣、着弾エフェクトの波紋、サークルマーカーなどに最適です。" },
                        { "9: Square / Plane (平面板)", "2D看板、水面、影デカール、ガラス板、UI背景などに最適です。" },
                        { "10: Custom Model (外部モデル)", "Assimp対応の .obj / .gltf 外部モデル。AnimationHuman等のキャラクターや武器に使用します。" }
                    };
                    DrawHelpTable("MeshTypeTable", "メッシュタイプ (Mesh Type)", "形状の特徴と推奨用途", 0.30f, meshTypeFeatures, kAccentKeyColor);
                }

                ImGui::Spacing();

                // 3. アニメーション＆形状運用のTips
                if (ImGui::CollapsingHeader("アニメーション＆形状運用のTips", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(kAccentWarnColor, "■ シーン保存と復元の自動化:");
                    ImGui::TextWrapped("  MeshRenderer のアニメーション再生状態（is_animation_playing, show_skeleton, animation_key）はすべてシーン保存 (Ctrl+S) 時に自動で .json に記録され、次回起動時も同じ状態で復元されます。");
                    ImGui::Spacing();

                    DrawIcon("sys_icon_model", kSectionIconSize);
                    ImGui::TextColored(kAccentTipColor, "■ ボーン可視化の活用:");
                    ImGui::TextWrapped("  「Show Skeleton (Bones)」を有効にすると、キャラクターの内部骨格が動く様子を確認できるため、アニメーションが正しく適用されているかのデバッグやポーズ確認に非常に便利です。");
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 7: ○ エフェクト＆パーティクル (Effects & Particles)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab7Flags = (helpWindowTargetTab_ == HelpTabIndex::kParticles) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ エフェクト＆パーティクル", nullptr, tab7Flags)) {
                ImGui::Spacing();

                DrawIcon("sys_icon_model", 20.0f);
                ImGui::TextColored(kAccentTipColor, "GPUパーティクルスタジオ & エフェクトシステム統合ガイド");
                ImGui::TextDisabled("パーティクルのリアルタイム制作、GameObjectへのアタッチ、スクリプトからの再生APIの解説です。");
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("GPUパーティクルスタジオの使い方 (ParticleStudioWindow)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> studioFeatures = {
                        { "ワークスペース切り替え", "エディタ上部「Studio」タブを選択すると、パーティクル制作専用レイアウトが全画面で展開されます。" },
                        { "プレビューカメラ操作", "プレビュー画面上で、右ドラッグ(オービット回転)、中ドラッグ(パン)、ホイール(ズーム)が可能です。" },
                        { "放射形状 (Shape)", "Sphere(球状爆散)、Cone(扇状コーン)、Box(空間散布)、Circle(平面リング)から選択可能です。" },
                        { "物理パラメータ調整", "粒子発生数、寿命、初速、重力、空気抵抗、拡散角度をリアルタイムスライダーで調整できます。" },
                        { "カラー＆スケールアニメ", "発生時カラーから消滅時カラーへの滑らかなグラデーション、寿命に応じたサイズ拡縮変化を設定可能です。" },
                        { "JSONプリセット保存", "作成したパーティクル設定は resources/Config/Particles/ にJSON形式でワンクリック保存・読み込みできます。" }
                    };
                    DrawHelpTable("StudioTable", "機能・項目", "説明・操作手順", 0.30f, studioFeatures, kAccentTipColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("パーティクルコンポーネント運用 (ParticleEffectComponent)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> compFeatures = {
                        { "+ Add Component", "GameObjectを選択し、インスペクターから「ParticleEffect」を追加します。" },
                        { "EffectName", "再生するエフェクトの識別名（Hit, FireworksBurst等）を指定します。" },
                        { "Offset", "親オブジェクトの位置からの相対オフセット座標(X, Y, Z)を設定します。" },
                        { "Loop / AutoPlay", "Loop: 継続ループ再生(松明・煙など)。AutoPlay: シーンロード時の自動再生トリガー。" },
                        { "Play ▶ / Stop ⏹", "インスペクター上で即座にテスト再生・停止して挙動を確認できます。" },
                        { "スクリプト制御", "GetComponent<ParticleEffectComponent>()->Play(); で着弾やイベント時に呼び出します。" }
                    };
                    DrawHelpTable("ParticleCompTable", "プロパティ・操作", "機能・使い方", 0.30f, compFeatures, kAccentKeyColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("エフェクトマネージャ API (EffectManager)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> mgrFeatures = {
                        { "PlayEffect3D(name, pos)", "指定した3Dワールド座標に登録済みエフェクトを一発再生します。" },
                        { "PlayEffect2D(name, pos)", "指定した2Dスクリーン座標にスプライトエフェクトを一発再生します。" },
                        { "PlayGPUEffect(pos, count)", "Compute Shaderを用いた数千〜数万粒子のGPUパーティクルを低負荷で即時発生させます。" },
                        { "エフェクト一覧ウィンドウ", "メニュー「ウィンドウ」→「エフェクト一覧」から登録済み全エフェクトのテスト再生が可能です。" }
                    };
                    DrawHelpTable("EffectMgrTable", "API・機能", "説明・用途", 0.35f, mgrFeatures, kAccentApiColor);
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 8: ○ エンジン設計＆アーキテクチャ (Engine Architecture)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab8Flags = (helpWindowTargetTab_ == HelpTabIndex::kArchitecture) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ エンジン設計＆構造", nullptr, tab8Flags)) {
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("DirectX 12 高速レンダリング基盤", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> dxFeatures = {
                        { "DxCommon", "ID3D12Device, CommandQueue, SwapChain, DescriptorHeap(RTV/SRV/DSV)を一元管理する最基盤クラスです。" },
                        { "PSOManager", "グラフィックス＆コンピュートパイプラインステート(PSO)とシェーダーをキャッシュ管理し、描画切替を高速化します。" },
                        { "TextureManager", "テクスチャの非同期ロードとSRVヒープインデックスの自動管理を提供します。" }
                    };
                    DrawHelpTable("DxArchTable", "基盤クラス", "役割・設計概要", 0.30f, dxFeatures, kAccentKeyColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("メインゲームループ (Main Loop)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(kAccentTipColor, "【毎フレームの確定実行サイクル】");
                    ImGui::BulletText("1. ProcessMessage() : Windows メッセージ（ウィンドウ移動、キー入力等）の処理");
                    ImGui::BulletText("2. BeginFrame()      : DirectX 12 バックバッファのクリアと描画準備");
                    ImGui::BulletText("3. Update()          : Input入力更新 → SceneManager更新 → GameObjectツリー更新 → 物理・当たり判定");
                    ImGui::BulletText("4. ImGui/Editor      : DebugEditor ウィンドウ描画、ギズモ処理、UIロジック実行");
                    ImGui::BulletText("5. Render()          : 3D/2Dオブジェクト、エフェクト、ポストプロセスのGPUコマンド発行");
                    ImGui::BulletText("6. Present() / End() : スワップチェーンの画面フリップとフレーム同期");
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("シーンシリアライズとコンポーネント指向", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> compArchFeatures = {
                        { "SceneSerializer", "シーン内のすべてのGameObjectツリー、Transform、コンポーネント固有設定をJSON形式で完全永続化・復元します。" },
                        { "UUID システム", "各オブジェクトに一意のUUIDを割り当て、シーン再読み込み後もターゲット参照関係を壊さず自動解決します。" },
                        { "REGISTER_SCRIPT", "マクロを介してユーザー定義スクリプトの型情報をScriptRegistryへ自動登録し、エディタでの動的追加を実現します。" }
                    };
                    DrawHelpTable("CompArchTable", "アーキテクチャ", "機能・詳細", 0.30f, compArchFeatures, kAccentTipColor);
                }

                ImGui::EndTabItem();
            }

            // -------------------------------------------------------------
            // Tab 9: ○ 全ウィンドウ操作マニュアル (Windows & Tools)
            // -------------------------------------------------------------
            ImGuiTabItemFlags tab9Flags = (helpWindowTargetTab_ == HelpTabIndex::kWindows) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("○ 全ウィンドウ操作マニュアル", nullptr, tab9Flags)) {
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("主要エディタウィンドウ (Main Windows)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> mainWinFeatures = {
                        { "SceneView", "3D空間ビューポート。右ドラッグFPS視点、3Dギズモ(移動/回転/拡縮)、Fキーフォーカス、グリッドやコライダーワイヤーを描画。" },
                        { "GameView", "実際のゲーム実行画面。アスペクト比選択(16:9低解像度/高解像度)、解像度スケール、フルスクリーン表示に対応。" },
                        { "Hierarchy", "シーン内の全GameObject階層。右クリック生成メニュー、DND親子付け、削除[x](Undo対応)、リアルタイム名前検索バー。" },
                        { "Inspector", "選択中オブジェクトのTransform、コンポーネント設定、スクリプト公開変数、2D/3D座標補正ボタン、プレハブ保存ボタン。" }
                    };
                    DrawHelpTable("MainWinTable", "ウィンドウ名", "主な機能と操作方法", 0.28f, mainWinFeatures, kAccentKeyColor);
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("アセット＆デバッグツール (Tools & Diagnostics)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::vector<std::pair<std::string, std::string>> toolFeatures = {
                        { "ContentBrowser", "resources/ と Scripts/ のフォルダ階層ブラウザ。アイコン表示、履歴移動、DNDによるプレハブ配置やスクリプトアタッチ。" },
                        { "DebugConsole", "エンジンの全ログをリアルタイム表示。Info, Warning, Errorのレベル別トグルフィルタ、ログクリア機能。" },
                        { "PerformanceMonitor", "リアルタイムFPS、フレーム時間(ms)グラフ、CPU/GPU処理時間、メモリ使用量を視覚的に常時監視。" },
                        { "PostProcess", "ブルーム(光あふれ)、トーンマッピング、ヴィネット、被写界深度などの画面効果パラメータをリアルタイム微調整。" },
                        { "Workspaces", "上部タブから Layout(標準編集レイアウト) と Studio(パーティクル制作レイアウト) を瞬時に切り替え。" }
                    };
                    DrawHelpTable("ToolsTable", "ツール名", "主な機能と操作方法", 0.28f, toolFeatures, kAccentTipColor);
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
            helpWindowTargetTab_ = -1; // 指定タブの選択完了後にリセット
        }
        } // isSearching の else 節の閉じ
    }
    ImGui::End();
}
#endif

