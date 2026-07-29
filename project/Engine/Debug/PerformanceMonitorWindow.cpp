#ifdef _USEIMGUI
#include "Engine/Debug/PerformanceMonitorWindow.h"
#include "Engine/Zuizui.h"
#include "externals/imgui/imgui.h"
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

PerformanceMonitorWindow::PerformanceMonitorWindow() {
    lastFrameTime_ = std::chrono::steady_clock::now();
}

void PerformanceMonitorWindow::Draw(bool* show) {
    if (!ImGui::Begin("Performance Monitor", show)) {
        ImGui::End();
        return;
    }

    // パフォーマンス履歴描画用のローカル配列
    float fpsHistoryLocal[kHistorySize] = {};
    float memHistoryLocal[kHistorySize] = {};
    float midpointFpsHistoryLocal[kHistorySize] = {};
    int drawOffset = 0;

    float displayFps = 0.0f;
    float displayMs = 0.0f;
    float displayMem = 0.0f;

    float minFpsVal = -1.0f;
    float maxFpsVal = -1.0f;
    float midpointFpsVal = 0.0f;

    // --- エンジンの正確な DeltaTime を受領し、60.0 FPS ハードキャップを適用 ---
    float realDeltaTime = Zuizui::GetInstance()->GetDxCommon()->GetDeltaTime();

    if (realDeltaTime < 0.0001f) { realDeltaTime = 0.0001f; }
    if (realDeltaTime > 1.0f) { realDeltaTime = 1.0f; }

    constexpr float kMaxCapFps = 60.0f;
    float rawFps = 1.0f / realDeltaTime;
    // 60.0 FPS を 0.1 でも超えさせないハードキャップクランプ
    float currentFps = (rawFps > kMaxCapFps) ? kMaxCapFps : rawFps;
    float currentMs = realDeltaTime * 1000.0f;

    static float cachedMem = 0.0f;
    static float memTimer = 0.0f;
    memTimer -= realDeltaTime;
    if (memTimer <= 0.0f || cachedMem == 0.0f) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            cachedMem = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
        }
        memTimer = 0.5f; // 0.5秒ごとに更新
    }
    float currentMem = cachedMem;

    fpsHistory_[historyOffset_] = currentFps;
    memHistory_[historyOffset_] = currentMem;

    frameCount_++;

    // 直近 kHistorySize (100フレーム) 内での正確な Min / Max を動的算出 (過去スパイクの残存を完全防止)
    minFpsVal = currentFps;
    maxFpsVal = currentFps;
    if (frameCount_ > 10) {
        minFpsVal = fpsHistory_[0];
        maxFpsVal = fpsHistory_[0];
        for (int i = 1; i < kHistorySize; ++i) {
            if (fpsHistory_[i] > 0.0f) {
                if (fpsHistory_[i] < minFpsVal) minFpsVal = fpsHistory_[i];
                if (fpsHistory_[i] > maxFpsVal) maxFpsVal = fpsHistory_[i];
            }
        }
    }
    // Max FPS も 60.0 を超えさせない
    if (maxFpsVal > kMaxCapFps) { maxFpsVal = kMaxCapFps; }

    float midpointFps = (maxFpsVal + minFpsVal) * 0.5f;
    int lastWriteIdx = (historyOffset_ + kHistorySize - 1) % kHistorySize;
    midpointFpsHistory_[lastWriteIdx] = midpointFps;

    historyOffset_ = (historyOffset_ + 1) % kHistorySize;

    // ローカル配列へコピーして描画に使用
    std::copy(std::begin(fpsHistory_), std::end(fpsHistory_), std::begin(fpsHistoryLocal));
    std::copy(std::begin(memHistory_), std::end(memHistory_), std::begin(memHistoryLocal));
    std::copy(std::begin(midpointFpsHistory_), std::end(midpointFpsHistory_), std::begin(midpointFpsHistoryLocal));
    drawOffset = historyOffset_;

    displayFps = currentFps;
    displayMs = currentMs;
    displayMem = currentMem;
    midpointFpsVal = midpointFps;

    // テキスト表示（最高・最低・中央値の数値をカラーで文字表記）
    ImGui::Text("FPS: %.1f", displayFps);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), " (Min: %.1f)", minFpsVal);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), " Max: %.1f", maxFpsVal);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), " Mid: %.1f", midpointFpsVal);

    ImGui::Text("Latency: %.2f ms", displayMs);
    ImGui::Text("Memory: %.1f MB", displayMem);

    ImGui::Separator();
    
    // グラフの縦軸スケール上限を 90.0 FPS に固定して美しく真っ直ぐな横一直線に視覚化
    constexpr float graphMaxFps = 90.0f;

    float maxMem = kDefaultMaxMem;
    for (int i = 0; i < kHistorySize; ++i) {
        if (memHistoryLocal[i] > maxMem) {
            maxMem = memHistoryLocal[i];
        }
    }
    float graphMaxMem = maxMem + 50.0f; // 50MBの余白

    // グラフ描画開始
    ImGui::Text("Performance Graph");

    // マージンとサイズの定義（マジックナンバー排除）
    constexpr float kLeftMargin = 50.0f;
    constexpr float kRightMargin = 80.0f;
    constexpr float kMinGraphHeight = 120.0f;
    constexpr float kBottomMargin = 20.0f;

    // グラフ全体の描画領域を確保するダミー要素を配置
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float graphHeight = ImGui::GetContentRegionAvail().y - kBottomMargin;
    if (graphHeight < kMinGraphHeight) { graphHeight = kMinGraphHeight; }

    // ダミー要素を配置して領域を予約（アサーションエラー防止とレイアウト堅牢化）
    ImGui::Dummy(ImVec2(totalWidth, graphHeight));
    ImVec2 containerMin = ImGui::GetItemRectMin();
    ImVec2 containerMax = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // グラフ本体のサイズと描画位置を計算
    float graphWidth = totalWidth - (kLeftMargin + kRightMargin);
    if (graphWidth < 100.0f) { graphWidth = 100.0f; }
    ImVec2 kGraphSize = ImVec2(graphWidth, graphHeight);
    ImVec2 plotPos = ImVec2(containerMin.x + kLeftMargin, containerMin.y);

    // 1) 現在のFPS (緑) - 背景フレームあり
    ImGui::SetCursorScreenPos(plotPos);
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 1.0f, 0.2f, 1.0f));
    ImGui::PlotLines("##CurrentFPS", fpsHistoryLocal, kHistorySize, drawOffset, nullptr, 0.0f, graphMaxFps, kGraphSize);
    ImGui::PopStyleColor();

    // 2) 中央値FPS (黄) - 背景フレームを透明にして重ねて描画
    ImGui::SetCursorScreenPos(plotPos);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
    ImGui::PlotLines("##MidpointFPS", midpointFpsHistoryLocal, kHistorySize, drawOffset, nullptr, 0.0f, graphMaxFps, kGraphSize);
    ImGui::PopStyleColor(2);

    // 3) メモリ (水色) - 背景フレームを透明にして重ねて描画
    ImGui::SetCursorScreenPos(plotPos);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    ImGui::PlotLines("##MemGraph", memHistoryLocal, kHistorySize, drawOffset, nullptr, 0.0f, graphMaxMem, kGraphSize);
    ImGui::PopStyleColor(2);

    // グラフ本体のホバー判定と矩形情報を取得
    bool isGraphHovered = ImGui::IsItemHovered();
    ImVec2 rectMin = ImGui::GetItemRectMin();
    ImVec2 rectMax = ImGui::GetItemRectMax();

    // グラフ値から画面上のY座標を逆算するヘルパーラムダ
    auto GetValueYPos = [&](float val, float scaleMax) -> float {
        float t = val / scaleMax;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return rectMax.y - t * (rectMax.y - rectMin.y);
    };

    // 0FPS, 30FPS, 60FPS の左側目盛りテキストと水平補助線を描画
    const float fpsTickValues[] = { 0.0f, 30.0f, 60.0f };
    ImU32 tickTextColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    ImU32 tickLineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.08f)); // 極めて薄い白の補助線

    for (float val : fpsTickValues) {
        float yPos = GetValueYPos(val, graphMaxFps);
        
        // 目盛りテキストのフォーマット (例: "60FPS")
        char tickText[16];
        sprintf_s(tickText, "%.0fFPS", val);
        
        ImVec2 textSize = ImGui::CalcTextSize(tickText);
        // グラフの左端（rectMin.x）から少し左に寄せて、右揃えで配置する
        float textX = rectMin.x - textSize.x - 5.0f;
        // テキストの縦位置を線に合わせる
        float textY = yPos - textSize.y * 0.5f;

        // 左マージン内であれば描画する
        if (textX >= containerMin.x) {
            drawList->AddText(ImVec2(textX, textY), tickTextColor, tickText);
        }

        // グラフ内に極めて薄い水平グリッド線を描画
        drawList->AddLine(ImVec2(rectMin.x, yPos), ImVec2(rectMax.x, yPos), tickLineColor, 1.0f);
    }

    // 4) 最高・最低FPSの補助線をグラフエリア内に描画
    if (minFpsVal >= 0.0f && maxFpsVal >= 0.0f) {
        float yMinLine = GetValueYPos(minFpsVal, graphMaxFps);
        float yMaxLine = GetValueYPos(maxFpsVal, graphMaxFps);

        // 薄い赤（最低）と薄い緑（最高）の横線を引く
        ImU32 minColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.3f, 0.3f, 0.35f));
        ImU32 maxColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 1.0f, 0.3f, 0.35f));

        drawList->AddLine(ImVec2(rectMin.x, yMinLine), ImVec2(rectMax.x, yMinLine), minColor, 1.0f);
        drawList->AddLine(ImVec2(rectMin.x, yMaxLine), ImVec2(rectMax.x, yMaxLine), maxColor, 1.0f);

        // 線のすぐ上に識別テキストを小さく描画
        drawList->AddText(ImVec2(rectMin.x + 5.0f, yMinLine - 12.0f), minColor, "Min FPS");
        drawList->AddText(ImVec2(rectMin.x + 5.0f, yMaxLine - 12.0f), maxColor, "Max FPS");
    }

    // 5) 折れ線の右終端（最新値）のすぐ右側に識別名を描画（色はそれぞれの線と同色）
    // クリッピングを回避するため、Dummyの右端スペース（rectMax.x）の範囲内に描画する
    float yCurrent = GetValueYPos(displayFps, graphMaxFps);
    float yMidpoint = GetValueYPos(midpointFpsVal, graphMaxFps);
    float yMemory = GetValueYPos(displayMem, graphMaxMem);

    // 近接時の衝突回避
    if (std::abs(yCurrent - yMidpoint) < 12.0f) {
        if (yCurrent < yMidpoint) {
            yCurrent -= 6.0f;
            yMidpoint += 6.0f;
        } else {
            yCurrent += 6.0f;
            yMidpoint -= 6.0f;
        }
    }

    ImU32 colorCurrent = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.2f, 1.0f)); // 緑
    ImU32 colorMidpoint = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // 黄
    ImU32 colorMemory  = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.6f, 1.0f, 1.0f)); // 水色

    float labelX = rectMax.x + 5.0f;
    drawList->AddText(ImVec2(labelX, yCurrent - 6.0f), colorCurrent, "Current");
    drawList->AddText(ImVec2(labelX, yMidpoint - 6.0f), colorMidpoint, "Midpoint");
    drawList->AddText(ImVec2(labelX, yMemory - 6.0f), colorMemory, "Memory");

    // マウスホバー時にカーソル位置に対応する詳細データ（過去データ）をツールチップ表示
    if (isGraphHovered) {
        ImVec2 mousePos = ImGui::GetMousePos();

        float fraction = (mousePos.x - rectMin.x) / (rectMax.x - rectMin.x);
        int hoveredDataIdx = static_cast<int>(fraction * kHistorySize);
        if (hoveredDataIdx < 0) { hoveredDataIdx = 0; }
        if (hoveredDataIdx >= kHistorySize) { hoveredDataIdx = kHistorySize - 1; }

        int actualIdx = (drawOffset + hoveredDataIdx) % kHistorySize;

        float hoverFps = fpsHistoryLocal[actualIdx];
        float hoverMidFps = midpointFpsHistoryLocal[actualIdx];
        float hoverMem = memHistoryLocal[actualIdx];

        ImGui::BeginTooltip();
        int framesAgo = kHistorySize - 1 - hoveredDataIdx;
        ImGui::Text("Time: %d frames ago (approx. %.1fs ago)", framesAgo, static_cast<float>(framesAgo) * 0.016f);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.2f, 1.0f), "Current: %.1f FPS", hoverFps);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Midpoint: %.1f FPS", hoverMidFps);
        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Memory: %.1f MB", hoverMem);
        ImGui::EndTooltip();
    }

    ImGui::End();
}
#endif
