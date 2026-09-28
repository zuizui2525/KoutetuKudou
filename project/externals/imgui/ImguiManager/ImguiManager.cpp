#ifdef _USEIMGUI
#include "ImguiManager.h"
#include <cassert>
#include <cstdint>
#include <filesystem>

namespace {
    // ImGui用のSRVスペース管理（インデックス100から20個確保：競合防止）
    static constexpr UINT kImGuiSrvStartIndex = 100;
    static constexpr UINT kImGuiSrvCount = 20;

    // 使用状況をビットマスクで管理（0:空き, 1:使用中）
    static uint32_t s_srvUsedMask = 0;
    static ID3D12DescriptorHeap* s_srvHeap = nullptr;
    static ID3D12Device* s_device = nullptr;

    // ディスクリプタの確保コールバック
    void ImGui_SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle) {
        (void)info;
        assert(s_srvHeap != nullptr);
        assert(s_device != nullptr);

        int freeIndex = -1;
        for (int i = 0; i < static_cast<int>(kImGuiSrvCount); ++i) {
            if ((s_srvUsedMask & (1 << i)) == 0) {
                freeIndex = i;
                s_srvUsedMask |= (1 << i); // 使用中にマーク
                break;
            }
        }

        assert(freeIndex != -1 && "ImGui SRV descriptor heap is full!");

        UINT descriptorSize = s_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        UINT srvIndex = kImGuiSrvStartIndex + freeIndex;

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = s_srvHeap->GetCPUDescriptorHandleForHeapStart();
        cpuHandle.ptr += srvIndex * descriptorSize;
        *out_cpu_desc_handle = cpuHandle;

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = s_srvHeap->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += srvIndex * descriptorSize;
        *out_gpu_desc_handle = gpuHandle;
    }

    // ディスクリプタの解放コールバック
    void ImGui_SrvDescriptorFree(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle) {
        (void)info;
        (void)cpu_desc_handle;
        assert(s_srvHeap != nullptr);
        assert(s_device != nullptr);

        UINT descriptorSize = s_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = s_srvHeap->GetGPUDescriptorHandleForHeapStart();
        
        SIZE_T offset = gpu_desc_handle.ptr - gpuStart.ptr;
        UINT srvIndex = static_cast<UINT>(offset / descriptorSize);

        if (srvIndex >= kImGuiSrvStartIndex && srvIndex < kImGuiSrvStartIndex + kImGuiSrvCount) {
            UINT indexInPool = srvIndex - kImGuiSrvStartIndex;
            s_srvUsedMask &= ~(1 << indexInPool); // 空きにリセット
        }
    }
}

void ImguiManager::Initialize(HWND hwnd, ID3D12Device* device, int backBufferCount,
    DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap* rtvHeap, ID3D12DescriptorHeap* srvHeap,
    ID3D12CommandQueue* commandQueue) {
    if (initialized_) return;

    // 静的変数のセットアップ
    s_srvHeap = srvHeap;
    s_device = device;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // --- フォントの読み込み ---
    // x16y32pxGridGazer に最適な整数フォントサイズ定数（マジックナンバー排除）
    static constexpr float kFontSize = 16.0f; 

    // 1. プライマリフォント
    const char* primaryFontPath = "resources/fonts/HigashiOme-Gothic-1.3i.ttf";
    ImFontConfig fontConfig;
    fontConfig.PixelSnapH = true; // ピクセル境界に吸着して輪郭の滲みを防止
    fontConfig.OversampleH = 1;   // ピクセルフォントのシャープなエッジを保持
    fontConfig.OversampleV = 1;

    ImFont* font = nullptr;
    if (std::filesystem::exists(primaryFontPath)) {
        // 英数字・記号をプライマリフォントから読み込み
        font = io.Fonts->AddFontFromFileTTF(primaryFontPath, kFontSize, &fontConfig, io.Fonts->GetGlyphRangesDefault());
    }

    // 2. 日本語グリフの合成（x16y32pxGridGazer に含まれない ひらがな・漢字 のためのマージ設定）
    const char* japaneseFontPaths[] = {
        "C:\\Windows\\Fonts\\YuGothM.ttc",       // 游ゴシック Medium（細身でモダン）
        "C:\\Windows\\Fonts\\meiryo.ttc",        // メイリオ
        "resources/fonts/MPLUS1p-Medium.ttf",    // M PLUS 1p Medium
        "C:\\Windows\\Fonts\\msgothic.ttc"       // ＭＳ ゴシック
    };

    ImFontConfig jpFontConfig;
    jpFontConfig.MergeMode = (font != nullptr); // プライマリフォントがある場合は合成、無ければ単体としてロード
    jpFontConfig.PixelSnapH = true;
    jpFontConfig.OversampleH = 2;
    jpFontConfig.OversampleV = 1;

    for (const char* jpPath : japaneseFontPaths) {
        if (std::filesystem::exists(jpPath)) {
            io.Fonts->AddFontFromFileTTF(jpPath, kFontSize, &jpFontConfig, io.Fonts->GetGlyphRangesJapanese());
            break;
        }
    }

    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = device;
    initInfo.CommandQueue = commandQueue;
    initInfo.NumFramesInFlight = backBufferCount;
    initInfo.RTVFormat = rtvFormat;
    initInfo.SrvDescriptorHeap = srvHeap;
    
    // ★修正: 1.92以降の新しいアロケータコールバックを登録
    initInfo.SrvDescriptorAllocFn = ImGui_SrvDescriptorAlloc;
    initInfo.SrvDescriptorFreeFn = ImGui_SrvDescriptorFree;

    ImGui_ImplDX12_Init(&initInfo);

    // --- スタイル（視認性を向上させたハイコントラスト・モダンダーク）の適用 ---
    ImGuiStyle& style = ImGui::GetStyle();

    // 丸みの設定（マジックナンバー排除のためのローカル定数定義）
    const float kWindowRounding = 6.0f;
    const float kFrameRounding = 4.0f;
    const float kGrabRounding = 4.0f;
    const float kTabRounding = 4.0f;
    const float kPopupRounding = 4.0f;
    const float kScrollbarRounding = 9.0f;

    style.WindowRounding = kWindowRounding;
    style.FrameRounding = kFrameRounding;
    style.GrabRounding = kGrabRounding;
    style.TabRounding = kTabRounding;
    style.PopupRounding = kPopupRounding;
    style.ScrollbarRounding = kScrollbarRounding;

    // パディングと余白の設定
    const ImVec2 kWindowPadding = ImVec2(8.0f, 8.0f);
    const ImVec2 kFramePadding = ImVec2(6.0f, 4.0f);
    const ImVec2 kItemSpacing = ImVec2(6.0f, 4.0f);
    const float kBorderSize = 1.0f;

    style.WindowPadding = kWindowPadding;
    style.FramePadding = kFramePadding;
    style.ItemSpacing = kItemSpacing;
    style.WindowBorderSize = kBorderSize;
    style.FrameBorderSize = kBorderSize;
    style.PopupBorderSize = kBorderSize;

    // カラー（配色の設定：文字を際立たせるピュアホワイトと漆黒ディープダーク）
    style.Colors[ImGuiCol_Text] = ImVec4(0.98f, 0.98f, 0.99f, 1.00f);        // 最も鮮明なピュアホワイト
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);    // 深い漆黒チャコール（コントラスト極大化）
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);      // 繊細で上品な枠線
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // フレーム（入力フィールド、スライダーなど：暗い背景に対して上品に浮き上がる明度）
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);

    // タイトルバー
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);

    // メニューバー
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);

    // スクロールバー
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.38f, 0.44f, 1.00f);

    // ボタン
    style.Colors[ImGuiCol_Button] = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.24f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.30f, 0.38f, 1.00f);

    // ヘッダー（選択項目、ツリーノードなど）
    style.Colors[ImGuiCol_Header] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);

    // ドッキング / タブ
    style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.50f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.16f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);

    initialized_ = true;
}

void ImguiManager::Begin() {
    if (!initialized_) return;
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImguiManager::End() {
    if (!initialized_) return;
    ImGui::Render();
}

void ImguiManager::Shutdown() {
    if (!initialized_) return;
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}
#endif
