#pragma once
#include <string>
#define NOMINMAX
#include <windows.h>
#include <objbase.h>

/// <summary>
/// 一意な識別子（UUID / GUID）の生成ユーティリティ
/// </summary>
namespace UUIDUtility {
    /// <summary>
    /// 新しいランダムGUID文字列（小文字・ハイフン区切り）を生成
    /// 例: "3f4a1b2c-8d5e-4f3a-9c2b-1a0e9d8c7b6a"
    /// </summary>
    inline std::string GenerateUUID() {
        GUID guid{};
        if (CoCreateGuid(&guid) == S_OK) {
            constexpr size_t kGuidBufferSize = 40;
            char buffer[kGuidBufferSize]{};
            snprintf(buffer, sizeof(buffer),
                "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1],
                guid.Data4[2], guid.Data4[3], guid.Data4[4],
                guid.Data4[5], guid.Data4[6], guid.Data4[7]
            );
            return std::string(buffer);
        }
        return "uuid-fallback";
    }
}
