#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/PostProcess/CopyImagePass.h"
#include "Engine/Graphics/PostProcess/GaussianBlurXPass.h"
#include "Engine/Graphics/PostProcess/GaussianBlurYPass.h"
#include "Engine/Graphics/PostProcess/GrayscalePass.h"
#include "Engine/Graphics/PostProcess/SepiaPass.h"
#include "Engine/Graphics/PostProcess/VignettePass.h"
#include "Engine/Graphics/PostProcess/BoxFilterPass.h"
#include "Engine/Graphics/PostProcess/UnderwaterPass.h"
#include "Engine/Graphics/PostProcess/DepthOutlinePass.h"
#include "Engine/Graphics/PostProcess/RadialBlurPass.h"
#include "Engine/Graphics/PostProcess/DissolvePass.h"
#include "Engine/Graphics/PostProcess/TVNoisePass.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Base/Log/Log.h"
#include <cassert>
#include <format>

namespace {
    // マジックナンバー排除のためのクリアカラー定数
    const Vector4 kClearColorBlue = { 0.1f, 0.25f, 0.5f, 1.0f }; // 元々の背景色
    const Vector4 kClearColorRed = { 1.0f, 0.0f, 0.0f, 1.0f };
    const Vector4 kClearColorBlack = { 0.1f, 0.1f, 0.1f, 1.0f };

    // 各パスのインデックス定義
    constexpr size_t kPassIndexCopy = 0;
    constexpr size_t kPassIndexGrayscale = 1;
    constexpr size_t kPassIndexSepia = 2;
    constexpr size_t kPassIndexVignette = 3;
    constexpr size_t kPassIndexBoxFilter = 4;
    constexpr size_t kPassIndexGaussianBlurX = 5;
    constexpr size_t kPassIndexGaussianBlurY = 6;
    constexpr size_t kPassIndexUnderwater = 7;
    constexpr size_t kPassIndexDepthOutline = 8;
    constexpr size_t kPassIndexRadialBlur = 9;
    constexpr size_t kPassIndexDissolve = 10;
    constexpr size_t kPassIndexTVNoise = 11;
}

void PostProcess::Initialize() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);

    renderTexture_ = std::make_unique<RenderTexture>();
    renderTextureTemp_ = std::make_unique<RenderTexture>();

    // 初期化時のクリアカラー（デフォルトは青）
    const Vector4 kClearColor = kClearColorBlue;

    // RTVは拡張したヒープのインデックス2および3を使用
    constexpr UINT kRtvIndex = 2;
    constexpr UINT kRtvIndexTemp = 3;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = engine->GetDxCommon()->GetRtvHandle(kRtvIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleTemp = engine->GetDxCommon()->GetRtvHandle(kRtvIndexTemp);

    // SRVヒープの末尾（インデックス127および126）を静的に割り当て
    ID3D12DescriptorHeap* srvHeap = engine->GetDxCommon()->GetSrvHeap();
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU = srvHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPUTemp = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPUTemp = srvHeap->GetGPUDescriptorHandleForHeapStart();
    
    UINT srvDescriptorSize = engine->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    constexpr UINT kReservedSrvIndex = 127;
    constexpr UINT kReservedSrvIndexTemp = 126;
    srvHandleCPU.ptr += kReservedSrvIndex * srvDescriptorSize;
    srvHandleGPU.ptr += kReservedSrvIndex * srvDescriptorSize;
    srvHandleCPUTemp.ptr += kReservedSrvIndexTemp * srvDescriptorSize;
    srvHandleGPUTemp.ptr += kReservedSrvIndexTemp * srvDescriptorSize;

    // メインのレンダーテクスチャ初期化
    renderTexture_->Initialize(
        engine->GetDevice(),
        WindowApp::kClientWidth,
        WindowApp::kClientHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        kClearColor,
        rtvHandle,
        srvHandleCPU,
        srvHandleGPU,
        L"PostProcessMain"
    );

    // ピンポン用中間テクスチャ初期化
    renderTextureTemp_->Initialize(
        engine->GetDevice(),
        WindowApp::kClientWidth,
        WindowApp::kClientHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        kClearColor,
        rtvHandleTemp,
        srvHandleCPUTemp,
        srvHandleGPUTemp,
        L"PostProcessTemp"
    );

    // 各個別パスの登録と初期化
    passes_.push_back(std::make_unique<CopyImagePass>());  // 0: Copy
    passes_.push_back(std::make_unique<GrayscalePass>());  // 1: Grayscale
    passes_.push_back(std::make_unique<SepiaPass>());      // 2: Sepia
    passes_.push_back(std::make_unique<VignettePass>());   // 3: Vignette
    passes_.push_back(std::make_unique<BoxFilterPass>());  // 4: BoxFilter
    passes_.push_back(std::make_unique<GaussianBlurXPass>()); // 5: GaussianBlurX
    passes_.push_back(std::make_unique<GaussianBlurYPass>()); // 6: GaussianBlurY
    passes_.push_back(std::make_unique<UnderwaterPass>()); // 7: Underwater
    passes_.push_back(std::make_unique<DepthOutlinePass>()); // 8: DepthOutline
    passes_.push_back(std::make_unique<RadialBlurPass>());  // 9: RadialBlur
    passes_.push_back(std::make_unique<DissolvePass>());    // 10: Dissolve
    passes_.push_back(std::make_unique<TVNoisePass>());     // 11: TVNoise

    // 全パスの初期化
    for (auto& pass : passes_) {
        pass->Initialize(engine->GetDevice());
    }
}

void PostProcess::PreDraw() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    assert(renderTexture_ != nullptr);

    ID3D12GraphicsCommandList* commandList = engine->GetDxCommon()->GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = engine->GetDxCommon()->GetDsvHeap()->GetCPUDescriptorHandleForHeapStart();

    // メインのレンダーテクスチャを描画ターゲットに設定してクリア
    renderTexture_->PreDraw(commandList, dsvHandle);
}

void PostProcess::PostDraw() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    assert(renderTexture_ != nullptr);

    ID3D12GraphicsCommandList* commandList = engine->GetDxCommon()->GetCommandList();
    renderTexture_->PostDraw(commandList);
}

D3D12_GPU_DESCRIPTOR_HANDLE PostProcess::GetFinalSrvGpuHandle() const {
    std::vector<IPostProcessPass*> activePasses;
    for (size_t i = 1; i < passes_.size(); ++i) {
        if (passes_[i]->IsActive()) {
            activePasses.push_back(passes_[i].get());
        }
    }
    if (activePasses.empty()) {
        // エフェクトがない場合はそのままメインのレンダーテクスチャSRVを返す
        return renderTexture_->GetSrvGpuHandle();
    }
    // エフェクトがある場合、最終結果は常に `renderTextureTemp_` に格納されます
    return renderTextureTemp_->GetSrvGpuHandle();
}

ID3D12Resource* PostProcess::GetFinalResource() const {
    std::vector<IPostProcessPass*> activePasses;
    for (size_t i = 1; i < passes_.size(); ++i) {
        if (passes_[i]->IsActive()) {
            activePasses.push_back(passes_[i].get());
        }
    }
    if (activePasses.empty()) {
        return renderTexture_->GetResource();
    }
    return renderTextureTemp_->GetResource();
}

RenderTexture* PostProcess::GetFinalRenderTexture() const {
    std::vector<IPostProcessPass*> activePasses;
    for (size_t i = 1; i < passes_.size(); ++i) {
        if (passes_[i]->IsActive()) {
            activePasses.push_back(passes_[i].get());
        }
    }
    if (activePasses.empty()) {
        return renderTexture_.get();
    }
    return renderTextureTemp_.get();
}

void PostProcess::PreDraw2D() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    RenderTexture* finalTarget = GetFinalRenderTexture();
    assert(finalTarget != nullptr);

    ID3D12GraphicsCommandList* commandList = engine->GetDxCommon()->GetCommandList();

    // 既存のポストエフェクト結果を保持したまま描画ターゲットに設定
    finalTarget->PreDrawWithoutClear(commandList);

    // 2D描画用ビューポートおよびシザー矩形の設定
    HWND hwnd = engine->GetWindow()->GetHWND();
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    float width = static_cast<float>(clientRect.right - clientRect.left);
    float height = static_cast<float>(clientRect.bottom - clientRect.top);

    constexpr float kMinDepth = 0.0f;
    constexpr float kMaxDepth = 1.0f;

    D3D12_VIEWPORT vp{};
    vp.Width = width;
    vp.Height = height;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.MinDepth = kMinDepth;
    vp.MaxDepth = kMaxDepth;

    D3D12_RECT sr{};
    sr.left = 0;
    sr.right = static_cast<LONG>(width);
    sr.top = 0;
    sr.bottom = static_cast<LONG>(height);

    commandList->RSSetViewports(1, &vp);
    commandList->RSSetScissorRects(1, &sr);
}

void PostProcess::PostDraw2D() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    RenderTexture* finalTarget = GetFinalRenderTexture();
    assert(finalTarget != nullptr);

    ID3D12GraphicsCommandList* commandList = engine->GetDxCommon()->GetCommandList();
    finalTarget->PostDraw(commandList);
}

void PostProcess::ProcessEffects() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    assert(renderTexture_ != nullptr);
    assert(renderTextureTemp_ != nullptr);

    ID3D12GraphicsCommandList* commandList = engine->GetDxCommon()->GetCommandList();

    // ポストプロセスのフルスクリーンパス描画用にプリミティブトポロジーを確実にTRIANGLELISTに設定
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // アクティブなパスを収集（CopyImagePassは除く）
    std::vector<IPostProcessPass*> activePasses;
    for (size_t i = 1; i < passes_.size(); ++i) {
        if (passes_[i]->IsActive()) {
            activePasses.push_back(passes_[i].get());
        }
    }

    if (activePasses.empty()) {
        renderTextureTemp_->PostDraw(commandList);
        return; // エフェクトがないなら何もしない
    }

    // レンダーターゲット遷移用の深度ステンシルハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = engine->GetDxCommon()->GetDsvHeap()->GetCPUDescriptorHandleForHeapStart();

    // ピンポンバッファのポインタ切り替えによるチェイン描画
    RenderTexture* currentInput = renderTexture_.get();
    RenderTexture* currentOutput = renderTextureTemp_.get();

    for (size_t i = 0; i < activePasses.size(); ++i) {
        // すべてテクスチャに出力する（深度バッファのクリアは行わない）
        currentOutput->PreDraw(commandList, dsvHandle, false);
        activePasses[i]->Draw(commandList, currentInput->GetSrvGpuHandle());
        currentOutput->PostDraw(commandList);

        // 次のパスに向けてインプットとアウトプットを反転
        currentInput = currentOutput;
        currentOutput = (currentInput == renderTextureTemp_.get()) ? renderTexture_.get() : renderTextureTemp_.get();
    }

    // もし最終結果が `renderTexture_` に戻ってしまっていた場合、
    // `renderTextureTemp_` にコピーして、常に `renderTextureTemp_` が最終結果のSRVになるように確定させます。
    if (currentInput == renderTexture_.get()) {
        renderTextureTemp_->PreDraw(commandList, dsvHandle);
        passes_[kPassIndexCopy]->Draw(commandList, renderTexture_->GetSrvGpuHandle());
        renderTextureTemp_->PostDraw(commandList);
    }
}

void PostProcess::CopyToSwapChain(D3D12_CPU_DESCRIPTOR_HANDLE targetRtv) {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);

    ID3D12GraphicsCommandList* commandList = engine->GetDxCommon()->GetCommandList();
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12_GPU_DESCRIPTOR_HANDLE finalSrv = GetFinalSrvGpuHandle();

    // 指定されたレンダーターゲット（スワップチェーン等）にコピー描画する
    commandList->OMSetRenderTargets(1, &targetRtv, FALSE, nullptr);

    // ウィンドウの現在のクライアント領域サイズに合わせてビューポートを動的に更新（アスペクト比崩れ・ずれ防止）
    HWND hwnd = engine->GetWindow()->GetHWND();
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    
    float width = static_cast<float>(clientRect.right - clientRect.left);
    float height = static_cast<float>(clientRect.bottom - clientRect.top);
    
    constexpr float kMinDepth = 0.0f;
    constexpr float kMaxDepth = 1.0f;

    D3D12_VIEWPORT vp{};
    vp.Width = width;
    vp.Height = height;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.MinDepth = kMinDepth;
    vp.MaxDepth = kMaxDepth;
    
    D3D12_RECT sr{};
    sr.left = 0;
    sr.right = static_cast<LONG>(width);
    sr.top = 0;
    sr.bottom = static_cast<LONG>(height);
    
    commandList->RSSetViewports(1, &vp);
    commandList->RSSetScissorRects(1, &sr);

    passes_[kPassIndexCopy]->Draw(commandList, finalSrv);
}

void PostProcess::CopyToSwapChain() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);

    // スワップチェーンの現在のバックバッファRTVを取得して描画
    UINT backBufferIndex = engine->GetDxCommon()->GetBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE swapchainRtv = engine->GetDxCommon()->GetRtvHandle(backBufferIndex);
    CopyToSwapChain(swapchainRtv);
}

void PostProcess::Draw(D3D12_CPU_DESCRIPTOR_HANDLE targetRtv) {
    // エフェクトをすべて処理し、レンダーテクスチャに確定させる
    ProcessEffects();

    // 確定したテクスチャを指定レンダーターゲットにコピー描画する
    CopyToSwapChain(targetRtv);
}

void PostProcess::Draw() {
    Zuizui* engine = EngineResource::GetEngine();
    assert(engine != nullptr);

    // スワップチェーンの現在のバックバッファRTVを取得して描画
    UINT backBufferIndex = engine->GetDxCommon()->GetBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE swapchainRtv = engine->GetDxCommon()->GetRtvHandle(backBufferIndex);
    Draw(swapchainRtv);
}

void PostProcess::SetClearColorMode(PostClearColorMode mode) {
    if (clearColorMode_ == mode) return;

    Vector4 oldColor = kClearColorBlue;
    switch (clearColorMode_) {
        case PostClearColorMode::Blue:  oldColor = kClearColorBlue; break;
        case PostClearColorMode::Red:   oldColor = kClearColorRed; break;
        case PostClearColorMode::Black: oldColor = kClearColorBlack; break;
    }

    Vector4 newColor = kClearColorBlue;
    switch (mode) {
        case PostClearColorMode::Blue:  newColor = kClearColorBlue; break;
        case PostClearColorMode::Red:   newColor = kClearColorRed; break;
        case PostClearColorMode::Black: newColor = kClearColorBlack; break;
    }

    clearColorMode_ = mode;

    const wchar_t* modeStr = L"Blue (Default)";
    if (mode == PostClearColorMode::Red) modeStr = L"Red (Debug)";
    else if (mode == PostClearColorMode::Black) modeStr = L"Black (Debug)";
    Log::Write(std::format(L" ├─ 【クリアカラー変更】 背景のクリアカラーモードを「{}」に変更しました。", modeStr));

    if (oldColor.x != newColor.x || oldColor.y != newColor.y || oldColor.z != newColor.z) {
        renderTexture_->Recreate(newColor);
        renderTextureTemp_->Recreate(newColor); // 中間テクスチャも再生成して色を一致させる
    }
}

void PostProcess::ImGuiControl() {
#ifdef _USEIMGUI
    if (!showWindow_) {
        return;
    }

    if (ImGui::Begin("ポストエフェクト###PostEffect", &showWindow_)) {
    
    if (ImGui::TreeNode("エフェクト選択###Select")) {
        bool boxActive = IsBoxFilterActive();
        if (ImGui::Checkbox("ボックスフィルター (BoxFilter)", &boxActive)) {
            SetBoxFilterActive(boxActive);
        }
        
        bool grayActive = IsGrayscaleActive();
        if (ImGui::Checkbox("グレースケール (Grayscale)", &grayActive)) {
            SetGrayscaleActive(grayActive);
        }
        
        bool sepiaActive = IsSepiaActive();
        if (ImGui::Checkbox("セピア (Sepia)", &sepiaActive)) {
            SetSepiaActive(sepiaActive);
        }
        
        bool vignetteActive = IsVignetteActive();
        if (ImGui::Checkbox("ビネット (Vignette)", &vignetteActive)) {
            SetVignetteActive(vignetteActive);
        }
        
        bool gaussActive = IsGaussianBlurActive();
        if (ImGui::Checkbox("ガウスぼかし (GaussianFilter)", &gaussActive)) {
            SetGaussianBlurActive(gaussActive);
        }

        bool underwaterActive = IsUnderwaterActive();
        if (ImGui::Checkbox("水中エフェクト (Underwater)", &underwaterActive)) {
            SetUnderwaterActive(underwaterActive);
        }
        
        bool depthOutlineActive = IsDepthOutlineActive();
        if (ImGui::Checkbox("深度輪郭線 (DepthOutline)", &depthOutlineActive)) {
            SetDepthOutlineActive(depthOutlineActive);
        }

        bool radialBlurActive = IsRadialBlurActive();
        if (ImGui::Checkbox("放射ブラー (RadialBlur)", &radialBlurActive)) {
            SetRadialBlurActive(radialBlurActive);
        }

        bool dissolveActive = IsDissolveActive();
        if (ImGui::Checkbox("ディゾルブ (Dissolve)", &dissolveActive)) {
            SetDissolveActive(dissolveActive);
        }

        bool tvNoiseActive = IsTVNoiseActive();
        if (ImGui::Checkbox("TVノイズ (TV Noise)", &tvNoiseActive)) {
            SetTVNoiseActive(tvNoiseActive);
        }
        
        ImGui::Separator();
        ImGui::Text("背景クリアカラー:");
        PostClearColorMode clearMode = GetClearColorMode();
        int colorVal = static_cast<int>(clearMode);
        if (ImGui::RadioButton("青色 (デフォルト)", &colorVal, 0)) {
            SetClearColorMode(PostClearColorMode::Blue);
        }
        if (ImGui::RadioButton("赤色 (デバッグ)", &colorVal, 1)) {
            SetClearColorMode(PostClearColorMode::Red);
        }
        if (ImGui::RadioButton("黒色 (デバッグ)", &colorVal, 2)) {
            SetClearColorMode(PostClearColorMode::Black);
        }

        ImGui::TreePop();
    }

    // GaussianFilter の個別パラメータ調整コントロール
    if (IsGaussianBlurActive()) {
        if (ImGui::TreeNode("GaussianFilter")) {
            int32_t radius = GetGaussianBlurKernelRadius();
            float sigma = GetGaussianBlurSigma();
            bool isChanged = false;
            
            // k (半径): 1 = 3x3, 2 = 5x5, 3 = 7x7, 4 = 9x9, 5 = 11x11... 20 = 41x41
            if (ImGui::SliderInt("k", &radius, 1, 20)) {
                isChanged = true;
            }
            
            // Sigma: 0.1 から 50.0
            if (ImGui::DragFloat("Sigma", &sigma, 0.05f, 0.1f, 50.0f, "%.3f")) {
                isChanged = true;
            }
            
            if (isChanged) {
                SetGaussianBlurParams(radius, sigma);
            }
            
            ImGui::TreePop();
        }
    }

    // 各アクティブなパスの固有パラメータのImGuiコントロールを順次呼び出す
    for (auto& pass : passes_) {
        if (pass->IsActive()) {
            pass->ImGuiControl();
        }
    }
    }
    
    ImGui::End();
#endif
}

// ==========================================
// 各個別パスのアクティブ状態制御アクセサ
// ==========================================

void PostProcess::SetGrayscaleActive(bool active) {
    if (passes_.size() > kPassIndexGrayscale) {
        if (passes_[kPassIndexGrayscale]->IsActive() != active) {
            passes_[kPassIndexGrayscale]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 グレースケール を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsGrayscaleActive() const {
    return (passes_.size() > kPassIndexGrayscale) ? passes_[kPassIndexGrayscale]->IsActive() : false;
}

void PostProcess::SetSepiaActive(bool active) {
    if (passes_.size() > kPassIndexSepia) {
        if (passes_[kPassIndexSepia]->IsActive() != active) {
            passes_[kPassIndexSepia]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 セピア を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsSepiaActive() const {
    return (passes_.size() > kPassIndexSepia) ? passes_[kPassIndexSepia]->IsActive() : false;
}

void PostProcess::SetVignetteActive(bool active) {
    if (passes_.size() > kPassIndexVignette) {
        if (passes_[kPassIndexVignette]->IsActive() != active) {
            passes_[kPassIndexVignette]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 ビネット を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsVignetteActive() const {
    return (passes_.size() > kPassIndexVignette) ? passes_[kPassIndexVignette]->IsActive() : false;
}

void PostProcess::SetBoxFilterActive(bool active) {
    if (passes_.size() > kPassIndexBoxFilter) {
        if (passes_[kPassIndexBoxFilter]->IsActive() != active) {
            passes_[kPassIndexBoxFilter]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 ボックスフィルタ（ぼかし） を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsBoxFilterActive() const {
    return (passes_.size() > kPassIndexBoxFilter) ? passes_[kPassIndexBoxFilter]->IsActive() : false;
}

void PostProcess::ClearEffects() {
    bool hasActive = false;
    for (auto& pass : passes_) {
        if (pass && pass->IsActive()) {
            pass->SetActive(false);
            hasActive = true;
        }
    }
    if (hasActive) {
        Log::Write(L" ├─ 【ポストエフェクトクリア】 すべてのポストエフェクトを無効化しました。");
    }
}

// ==========================================
// ビネットパラメータアクセサの転送
// ==========================================

void PostProcess::SetVignetteScale(float scale) {
    if (passes_.size() > kPassIndexVignette) {
        auto vignette = dynamic_cast<VignettePass*>(passes_[kPassIndexVignette].get());
        if (vignette) {
            vignette->SetScale(scale);
        }
    }
}

float PostProcess::GetVignetteScale() const {
    if (passes_.size() > kPassIndexVignette) {
        auto vignette = dynamic_cast<VignettePass*>(passes_[kPassIndexVignette].get());
        if (vignette) {
            return vignette->GetScale();
        }
    }
    return 0.0f;
}

void PostProcess::SetVignetteExponent(float exponent) {
    if (passes_.size() > kPassIndexVignette) {
        auto vignette = dynamic_cast<VignettePass*>(passes_[kPassIndexVignette].get());
        if (vignette) {
            vignette->SetExponent(exponent);
        }
    }
}

float PostProcess::GetVignetteExponent() const {
    if (passes_.size() > kPassIndexVignette) {
        auto vignette = dynamic_cast<VignettePass*>(passes_[kPassIndexVignette].get());
        if (vignette) {
            return vignette->GetExponent();
        }
    }
    return 0.0f;
}

void PostProcess::SetBoxFilterKernelRadius(int32_t radius) {
    if (passes_.size() > kPassIndexBoxFilter) {
        auto boxFilter = dynamic_cast<BoxFilterPass*>(passes_[kPassIndexBoxFilter].get());
        if (boxFilter) {
            boxFilter->SetKernelRadius(radius);
        }
    }
}

int32_t PostProcess::GetBoxFilterKernelRadius() const {
    if (passes_.size() > kPassIndexBoxFilter) {
        auto boxFilter = dynamic_cast<BoxFilterPass*>(passes_[kPassIndexBoxFilter].get());
        if (boxFilter) {
            return boxFilter->GetKernelRadius();
        }
    }
    return 1;
}

void PostProcess::SetGaussianBlurActive(bool active) {
    if (passes_.size() > kPassIndexGaussianBlurY) {
        bool current = passes_[kPassIndexGaussianBlurX]->IsActive();
        if (current != active) {
            passes_[kPassIndexGaussianBlurX]->SetActive(active);
            passes_[kPassIndexGaussianBlurY]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 ガウシアンフィルタ（ぼかし） を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsGaussianBlurActive() const {
    return (passes_.size() > kPassIndexGaussianBlurX) ? passes_[kPassIndexGaussianBlurX]->IsActive() : false;
}

void PostProcess::SetGaussianBlurParams(int32_t radius, float sigma) {
    if (passes_.size() > kPassIndexGaussianBlurY) {
        auto blurX = dynamic_cast<GaussianBlurXPass*>(passes_[kPassIndexGaussianBlurX].get());
        auto blurY = dynamic_cast<GaussianBlurYPass*>(passes_[kPassIndexGaussianBlurY].get());
        if (blurX && blurY) {
            blurX->SetParams(radius, sigma);
            blurY->SetParams(radius, sigma);
        }
    }
}

int32_t PostProcess::GetGaussianBlurKernelRadius() const {
    if (passes_.size() > kPassIndexGaussianBlurX) {
        auto blurX = dynamic_cast<GaussianBlurXPass*>(passes_[kPassIndexGaussianBlurX].get());
        if (blurX) {
            return blurX->GetKernelRadius();
        }
    }
    return 1;
}

float PostProcess::GetGaussianBlurSigma() const {
    if (passes_.size() > kPassIndexGaussianBlurX) {
        auto blurX = dynamic_cast<GaussianBlurXPass*>(passes_[kPassIndexGaussianBlurX].get());
        if (blurX) {
            return blurX->GetSigma();
        }
    }
    return 2.0f;
}

void PostProcess::Resize(uint32_t width, uint32_t height) {
    if (renderTexture_) {
        renderTexture_->Resize(width, height);
    }
    if (renderTextureTemp_) {
        renderTextureTemp_->Resize(width, height);
    }
}

void PostProcess::SetUnderwaterActive(bool active) {
    if (passes_.size() > kPassIndexUnderwater) {
        if (passes_[kPassIndexUnderwater]->IsActive() != active) {
            passes_[kPassIndexUnderwater]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 水中エフェクト を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsUnderwaterActive() const {
    return (passes_.size() > kPassIndexUnderwater) ? passes_[kPassIndexUnderwater]->IsActive() : false;
}

void PostProcess::SetDepthOutlineActive(bool active) {
    if (passes_.size() > kPassIndexDepthOutline) {
        if (passes_[kPassIndexDepthOutline]->IsActive() != active) {
            passes_[kPassIndexDepthOutline]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 深度アウトライン を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsDepthOutlineActive() const {
    return (passes_.size() > kPassIndexDepthOutline) ? passes_[kPassIndexDepthOutline]->IsActive() : false;
}

void PostProcess::SetDepthOutlineParams(float width, float threshold, float scale, const Vector3& color) {
    if (passes_.size() > kPassIndexDepthOutline) {
        auto depthOutline = dynamic_cast<DepthOutlinePass*>(passes_[kPassIndexDepthOutline].get());
        if (depthOutline) {
            depthOutline->SetEdgeWidth(width);
            depthOutline->SetThreshold(threshold);
            depthOutline->SetScale(scale);
            depthOutline->SetEdgeColor(color);
        }
    }
}

float PostProcess::GetDepthOutlineEdgeWidth() const {
    if (passes_.size() > kPassIndexDepthOutline) {
        auto depthOutline = dynamic_cast<DepthOutlinePass*>(passes_[kPassIndexDepthOutline].get());
        if (depthOutline) {
            return depthOutline->GetEdgeWidth();
        }
    }
    return 1.0f;
}

float PostProcess::GetDepthOutlineThreshold() const {
    if (passes_.size() > kPassIndexDepthOutline) {
        auto depthOutline = dynamic_cast<DepthOutlinePass*>(passes_[kPassIndexDepthOutline].get());
        if (depthOutline) {
            return depthOutline->GetThreshold();
        }
    }
    return 0.1f;
}

float PostProcess::GetDepthOutlineScale() const {
    if (passes_.size() > kPassIndexDepthOutline) {
        auto depthOutline = dynamic_cast<DepthOutlinePass*>(passes_[kPassIndexDepthOutline].get());
        if (depthOutline) {
            return depthOutline->GetScale();
        }
    }
    return 6.0f;
}

Vector3 PostProcess::GetDepthOutlineEdgeColor() const {
    if (passes_.size() > kPassIndexDepthOutline) {
        auto depthOutline = dynamic_cast<DepthOutlinePass*>(passes_[kPassIndexDepthOutline].get());
        if (depthOutline) {
            return depthOutline->GetEdgeColor();
        }
    }
    return Vector3{0.0f, 0.0f, 0.0f};
}

void PostProcess::SetRadialBlurActive(bool active) {
    if (passes_.size() > kPassIndexRadialBlur) {
        if (passes_[kPassIndexRadialBlur]->IsActive() != active) {
            passes_[kPassIndexRadialBlur]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 ラジアルブラー を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsRadialBlurActive() const {
    return (passes_.size() > kPassIndexRadialBlur) ? passes_[kPassIndexRadialBlur]->IsActive() : false;
}

void PostProcess::SetRadialBlurParams(const Vector2& center, float blurWidth) {
    if (passes_.size() > kPassIndexRadialBlur) {
        auto radialBlur = dynamic_cast<RadialBlurPass*>(passes_[kPassIndexRadialBlur].get());
        if (radialBlur) {
            radialBlur->SetParams(center, blurWidth);
        }
    }
}

Vector2 PostProcess::GetRadialBlurCenter() const {
    if (passes_.size() > kPassIndexRadialBlur) {
        auto radialBlur = dynamic_cast<RadialBlurPass*>(passes_[kPassIndexRadialBlur].get());
        if (radialBlur) {
            return radialBlur->GetCenter();
        }
    }
    return Vector2{0.5f, 0.5f};
}

float PostProcess::GetRadialBlurWidth() const {
    if (passes_.size() > kPassIndexRadialBlur) {
        auto radialBlur = dynamic_cast<RadialBlurPass*>(passes_[kPassIndexRadialBlur].get());
        if (radialBlur) {
            return radialBlur->GetBlurWidth();
        }
    }
    return 0.0f;
}

void PostProcess::SetDissolveActive(bool active) {
    if (passes_.size() > kPassIndexDissolve) {
        if (passes_[kPassIndexDissolve]->IsActive() != active) {
            passes_[kPassIndexDissolve]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 ディゾルブ を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsDissolveActive() const {
    return (passes_.size() > kPassIndexDissolve) ? passes_[kPassIndexDissolve]->IsActive() : false;
}

void PostProcess::SetDissolveParams(float threshold, float edgeWidth, const Vector3& edgeColor) {
    if (passes_.size() > kPassIndexDissolve) {
        auto dissolve = dynamic_cast<DissolvePass*>(passes_[kPassIndexDissolve].get());
        if (dissolve) {
            dissolve->SetThreshold(threshold);
            dissolve->SetEdgeWidth(edgeWidth);
            dissolve->SetEdgeColor(edgeColor);
        }
    }
}

float PostProcess::GetDissolveThreshold() const {
    if (passes_.size() > kPassIndexDissolve) {
        auto dissolve = dynamic_cast<DissolvePass*>(passes_[kPassIndexDissolve].get());
        if (dissolve) {
            return dissolve->GetThreshold();
        }
    }
    return 0.0f;
}

float PostProcess::GetDissolveEdgeWidth() const {
    if (passes_.size() > kPassIndexDissolve) {
        auto dissolve = dynamic_cast<DissolvePass*>(passes_[kPassIndexDissolve].get());
        if (dissolve) {
            return dissolve->GetEdgeWidth();
        }
    }
    return 0.0f;
}

Vector3 PostProcess::GetDissolveEdgeColor() const {
    if (passes_.size() > kPassIndexDissolve) {
        auto dissolve = dynamic_cast<DissolvePass*>(passes_[kPassIndexDissolve].get());
        if (dissolve) {
            return dissolve->GetEdgeColor();
        }
    }
    return Vector3{ 1.0f, 0.5f, 0.0f };
}

void PostProcess::SetDissolveActiveNoiseIndex(int32_t index) {
    if (passes_.size() > kPassIndexDissolve) {
        auto dissolve = dynamic_cast<DissolvePass*>(passes_[kPassIndexDissolve].get());
        if (dissolve) {
            dissolve->SetActiveNoiseIndex(index);
        }
    }
}

int32_t PostProcess::GetDissolveActiveNoiseIndex() const {
    if (passes_.size() > kPassIndexDissolve) {
        auto dissolve = dynamic_cast<DissolvePass*>(passes_[kPassIndexDissolve].get());
        if (dissolve) {
            return dissolve->GetActiveNoiseIndex();
        }
    }
    return 0;
}

void PostProcess::SetTVNoiseActive(bool active) {
    if (passes_.size() > kPassIndexTVNoise) {
        if (passes_[kPassIndexTVNoise]->IsActive() != active) {
            passes_[kPassIndexTVNoise]->SetActive(active);
            Log::Write(std::format(L" ├─ 【ポストエフェクト切替】 TVノイズ を {} にしました。", active ? L"有効" : L"無効"));
        }
    }
}

bool PostProcess::IsTVNoiseActive() const {
    return (passes_.size() > kPassIndexTVNoise) ? passes_[kPassIndexTVNoise]->IsActive() : false;
}

void PostProcess::SetTVNoiseStrength(float strength) {
    if (passes_.size() > kPassIndexTVNoise) {
        auto tvNoise = dynamic_cast<TVNoisePass*>(passes_[kPassIndexTVNoise].get());
        if (tvNoise) {
            tvNoise->SetNoiseStrength(strength);
        }
    }
}

float PostProcess::GetTVNoiseStrength() const {
    if (passes_.size() > kPassIndexTVNoise) {
        auto tvNoise = dynamic_cast<TVNoisePass*>(passes_[kPassIndexTVNoise].get());
        if (tvNoise) {
            return tvNoise->GetNoiseStrength();
        }
    }
    return 0.5f;
}

