#include "Engine/Input/Input.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Log/Log.h"
#include "App/Scene/Core/SceneManager.h"
#include <cassert>
#include <cstring>
#include <format>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {
    std::string GetKeyName(BYTE dikCode) {
        switch (dikCode) {
            case DIK_SPACE: return "SPACE";
            case DIK_RETURN: return "ENTER";
            case DIK_ESCAPE: return "ESCAPE";
            case DIK_UP: return "UP";
            case DIK_DOWN: return "DOWN";
            case DIK_LEFT: return "LEFT";
            case DIK_RIGHT: return "RIGHT";
            case DIK_A: return "A";
            case DIK_B: return "B";
            case DIK_C: return "C";
            case DIK_D: return "D";
            case DIK_E: return "E";
            case DIK_F: return "F";
            case DIK_G: return "G";
            case DIK_H: return "H";
            case DIK_I: return "I";
            case DIK_J: return "J";
            case DIK_K: return "K";
            case DIK_L: return "L";
            case DIK_M: return "M";
            case DIK_N: return "N";
            case DIK_O: return "O";
            case DIK_P: return "P";
            case DIK_Q: return "Q";
            case DIK_R: return "R";
            case DIK_S: return "S";
            case DIK_T: return "T";
            case DIK_U: return "U";
            case DIK_V: return "V";
            case DIK_W: return "W";
            case DIK_X: return "X";
            case DIK_Y: return "Y";
            case DIK_Z: return "Z";
            case DIK_0: return "0";
            case DIK_1: return "1";
            case DIK_2: return "2";
            case DIK_3: return "3";
            case DIK_4: return "4";
            case DIK_5: return "5";
            case DIK_6: return "6";
            case DIK_7: return "7";
            case DIK_8: return "8";
            case DIK_9: return "9";
            case DIK_LSHIFT: return "LSHIFT";
            case DIK_RSHIFT: return "RSHIFT";
            case DIK_LCONTROL: return "LCONTROL";
            case DIK_RCONTROL: return "RCONTROL";
            case DIK_LMENU: return "LALT";
            case DIK_RMENU: return "RALT";
            case DIK_TAB: return "TAB";
            default: return std::format("DIK_{}", static_cast<int>(dikCode));
        }
    }

    std::string GetMouseButtonName(int buttonIndex) {
        switch (buttonIndex) {
            case 0: return "左マウスボタン";
            case 1: return "右マウスボタン";
            case 2: return "中マウスボタン";
            default: return std::format("マウスボタン{}", buttonIndex);
        }
    }
}

Input::Input() {
    memset(key_, 0, sizeof(key_));
    memset(preKey_, 0, sizeof(preKey_));
    memset(&mouseState_, 0, sizeof(mouseState_));
    memset(&preMouseState_, 0, sizeof(preMouseState_));
}

Input::~Input() {
    if (keyboard_) keyboard_->Unacquire();
    if (mouse_) mouse_->Unacquire();
}

void Input::Initialize() {
    Log::Write(L" ├─ 【入力システム初期化開始】 DirectInput8 によるデバイス構築を開始します。");

    // 1. EngineResourceから必要な情報を取得
    auto engine = EngineResource::GetEngine();
    assert(engine != nullptr);

    // 2. DirectInput8Create (ComPtr of 扱いを修正)
    HRESULT hr = DirectInput8Create(
        engine->GetWindow()->GetInstance(),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        (void**)directInput_.GetAddressOf(), // &ではなくGetAddressOf()を使う
        nullptr);
    assert(SUCCEEDED(hr));

    // 3. キーボードデバイスの作成
    hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), NULL);
    assert(SUCCEEDED(hr));

    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    // engine経由でHWNDを取得
    hr = keyboard_->SetCooperativeLevel(
        engine->GetWindow()->GetHWND(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));

    Log::Write(L" │   ├─ キーボードデバイスの生成および設定に成功しました。");

    // 4. マウスデバイスの作成
    hr = directInput_->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), NULL);
    assert(SUCCEEDED(hr));

    hr = mouse_->SetDataFormat(&c_dfDIMouse2);
    assert(SUCCEEDED(hr));

    hr = mouse_->SetCooperativeLevel(
        engine->GetWindow()->GetHWND(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    assert(SUCCEEDED(hr));

    Log::Write(L" └─ 【入力システム初期化完了】 マウスデバイスの生成および設定に成功しました。");
}

void Input::Update() {
    // キーボード
    memcpy(preKey_, key_, sizeof(key_));
    if (keyboard_) {
        keyboard_->Acquire();
        keyboard_->GetDeviceState(sizeof(key_), key_);
    }

    // マウス
    preMouseState_ = mouseState_;
    if (mouse_) {
        mouse_->Acquire();
        // マウスの状態を取得
        mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_);
    }


}

// ==== キーボード ====
bool Input::Trigger(BYTE keyCode) const {
    return (key_[keyCode] & 0x80) && !(preKey_[keyCode] & 0x80);
}

bool Input::Press(BYTE keyCode) const {
    return (key_[keyCode] & 0x80);
}

bool Input::Release(BYTE keyCode) const {
    return !(key_[keyCode] & 0x80) && (preKey_[keyCode] & 0x80);
}

// ==== マウス ====
bool Input::MouseTrigger(int button) const {
    return (mouseState_.rgbButtons[button] & 0x80) && !(preMouseState_.rgbButtons[button] & 0x80);
}

bool Input::MousePress(int button) const {
    return (mouseState_.rgbButtons[button] & 0x80);
}

bool Input::MouseRelease(int button) const {
    return !(mouseState_.rgbButtons[button] & 0x80) && (preMouseState_.rgbButtons[button] & 0x80);
}
