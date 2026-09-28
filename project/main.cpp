#include "App/App.h"
#include "Engine/Base/Utils/DumpExporter.h"
#include "Engine/Base/Utils/D3DResourceLeakChecker.h"
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Windowsの高精度タイマーを有効化 (1ms精度)
    timeBeginPeriod(1);

    // dxgidebug.dll が先にアンロードされてクラッシュするのを防ぐため、明示的にロードしておく
    HMODULE dxgiDebugModule = LoadLibraryEx(L"dxgidebug.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    {
        // リークチェック
        D3DResourceLeakChecker leakCheck;
        SetUnhandledExceptionFilter(ExportDump);

        // App生成
        std::unique_ptr<App> app = std::make_unique<App>();

        // 初期化
        app->Initialize();

        // メインループ: 終わるまでRunを繰り返す
        while (!app->IsEnd()) {
            app->Run();
        }

        // 終了
        app->Finalize();
    }

    if (dxgiDebugModule) {
        FreeLibrary(dxgiDebugModule);
    }

    // 高精度タイマーを終了
    timeEndPeriod(1);

    return 0;
}
