#pragma once
#include <cstdint>

namespace ZuizuiPerf {
    /* 共有メモリの識別名 */
    constexpr const char* kSharedMemoryName = "Local\\ZuizuiEngine_PerfSharedMemory";
    /* 構造体バージョン（相互互換性チェック用） */
    constexpr uint32_t kSharedMemoryVersion = 1;

    /* 共有メモリデータ構造体 */
    struct SharedPerfData {
        uint32_t version;     /* 構造体バージョン */
        float time;           /* 経過時間 (秒) */
        float fps;            /* 平滑化された平均FPS */
        float rawFps;         /* 瞬時FPS */
        float cpuMemoryMb;    /* CPU メモリ (MB) */
        float vramMemoryMb;   /* GPU VRAM (MB) */
        uint64_t frameCount;  /* 累計フレーム数 */
        uint64_t lastUpdated; /* 最終更新タイムスタンプ (ms) */
    };
}
