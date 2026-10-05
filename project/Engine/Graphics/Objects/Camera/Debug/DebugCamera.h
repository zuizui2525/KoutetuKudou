#pragma once
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/WindowApp/WindowApp.h"

class DebugCamera : public BaseCamera {
public:
    void Initialize() override;

    // デバッグカメラ特有の更新（Inputを受け取る）
    void Update(Input* input);
    void Update() override {}



    // カーソル表示・非表示を外部から強制的に変える（カメラ切り替え時用）
    void SetCursorVisible(bool isVisible);
    void SetActive(bool active);
private:
    float moveSpeed_ = 0.2f;
    const float rotateSpeed_ = 0.002f;
    HWND hwnd_ = nullptr;

    bool isActive_ = false;
    bool isCursorVisible_ = true;

    POINT startCursorPos_{};
    bool isRightClickOperating_ = false;
    bool isFirstOperatingFrame_ = false;
};
