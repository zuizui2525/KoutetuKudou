#ifdef _USEIMGUI
#include "Engine/Debug/EditorNotification.h"
#include "externals/imgui/imgui.h"
#include <algorithm>

EditorNotification::ToastItem EditorNotification::sCurrentToast{};

namespace {
    // 定数定義 (マジックナンバー排除)
    constexpr float kFadeInDuration = 0.2f;       // フェードイン時間（秒）
    constexpr float kFadeOutDuration = 0.5f;      // フェードアウト時間（秒）
    constexpr float kToastOffsetY = 42.0f;        // 画面上部からのYオフセット
    constexpr float kRounding = 6.0f;             // 角丸半径
    constexpr float kPaddingX = 20.0f;            // 水平パディング
    constexpr float kPaddingY = 10.0f;            // 垂直パディング
    constexpr float kBorderThickness = 1.2f;      // 枠線太さ

    // 各種カラー設定
    constexpr ImVec4 kBgColor = ImVec4(0.12f, 0.13f, 0.15f, 0.94f);        // 背景色（ダークスレート）
    constexpr ImVec4 kSuccessColor = ImVec4(0.25f, 0.85f, 0.45f, 1.0f);     // 成功: エメラルドグリーン
    constexpr ImVec4 kInfoColor = ImVec4(0.30f, 0.70f, 1.0f, 1.0f);        // 情報: スカイブルー
    constexpr ImVec4 kWarningColor = ImVec4(1.0f, 0.75f, 0.20f, 1.0f);     // 警告: アンバーオレンジ
    constexpr ImVec4 kTextColor = ImVec4(0.95f, 0.96f, 0.98f, 1.0f);        // 文字色
}

void EditorNotification::Show(const std::string& message, NotificationType type, float duration) {
    sCurrentToast.message = message;
    sCurrentToast.type = type;
    sCurrentToast.remainingTime = (std::max)(duration, kFadeInDuration + kFadeOutDuration);
    sCurrentToast.totalDuration = sCurrentToast.remainingTime;
}

void EditorNotification::Draw() {
    if (sCurrentToast.remainingTime <= 0.0f || sCurrentToast.message.empty()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    float deltaTime = io.DeltaTime;
    sCurrentToast.remainingTime -= deltaTime;
    if (sCurrentToast.remainingTime <= 0.0f) {
        sCurrentToast.message.clear();
        return;
    }

    // 透明度 (Alpha) の計算
    float alpha = 1.0f;
    float elapsed = sCurrentToast.totalDuration - sCurrentToast.remainingTime;

    if (elapsed < kFadeInDuration) {
        // フェードイン
        alpha = elapsed / kFadeInDuration;
    } else if (sCurrentToast.remainingTime < kFadeOutDuration) {
        // フェードアウト
        alpha = sCurrentToast.remainingTime / kFadeOutDuration;
    }
    alpha = (std::clamp)(alpha, 0.0f, 1.0f);

    // 種別に応じたアクセントカラーとアイコン記号
    ImVec4 accentColor = kSuccessColor;
    const char* iconSymbol = "[OK]";
    switch (sCurrentToast.type) {
    case NotificationType::Success:
        accentColor = kSuccessColor;
        iconSymbol = "[OK]";
        break;
    case NotificationType::Info:
        accentColor = kInfoColor;
        iconSymbol = "[INFO]";
        break;
    case NotificationType::Warning:
        accentColor = kWarningColor;
        iconSymbol = "[WARN]";
        break;
    }

    // ウィンドウフラグ設定（オーバーレイ専用・操作を邪魔しない）
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoBackground;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;

    // 画面中央上部の位置計算 (横中央、縦は上から kToastOffsetY)
    ImVec2 targetPivot = ImVec2(0.5f, 0.0f);
    ImVec2 targetPos = ImVec2(workPos.x + workSize.x * 0.5f, workPos.y + kToastOffsetY);

    ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always, targetPivot);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPaddingX, kPaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding);

    if (ImGui::Begin("##EditorNotificationOverlay", nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 minPos = ImGui::GetWindowPos();
        ImVec2 maxPos = ImVec2(minPos.x + ImGui::GetWindowSize().x, minPos.y + ImGui::GetWindowSize().y);

        // 背景矩形を描画（半透明ダーク）
        ImU32 bgCol = ImColor(kBgColor.x, kBgColor.y, kBgColor.z, kBgColor.w * alpha);
        ImU32 borderCol = ImColor(accentColor.x, accentColor.y, accentColor.z, accentColor.w * alpha * 0.8f);
        drawList->AddRectFilled(minPos, maxPos, bgCol, kRounding);
        drawList->AddRect(minPos, maxPos, borderCol, kRounding, 0, kBorderThickness);

        // アイコンを描画
        ImVec4 iconCol = ImVec4(accentColor.x, accentColor.y, accentColor.z, accentColor.w * alpha);
        ImGui::TextColored(iconCol, "%s", iconSymbol);
        ImGui::SameLine();

        // メッセージ本文を描画
        ImVec4 textCol = ImVec4(kTextColor.x, kTextColor.y, kTextColor.z, kTextColor.w * alpha);
        ImGui::TextColored(textCol, "%s", sCurrentToast.message.c_str());
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}
#endif
