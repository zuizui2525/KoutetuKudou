#pragma once
#include <wrl.h>          // ComPtr
#include <dxgidebug.h>    // DXGI デバッグ
#include <dxgi1_3.h>      // DXGIGetDebugInterface1 のため
#include <d3d12.h>

#pragma comment(lib, "dxguid.lib") // 必要なライブラリをリンク

// D3D12 リソースリークチェッカー
struct D3DResourceLeakChecker {
    ~D3DResourceLeakChecker() {
        Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
            // アプリケーションが直接保持している未解放リソースのみを詳細出力（ドライバ内部オブジェクトの誤検知・例外を防止）
            debug->ReportLiveObjects(DXGI_DEBUG_ALL, static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
};
