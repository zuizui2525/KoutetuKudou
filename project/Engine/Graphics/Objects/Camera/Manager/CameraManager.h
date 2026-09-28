#pragma once
#include <map>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"

class CameraManager {
public:
    struct CameraForGPU {
        Matrix4x4 view;
        Matrix4x4 projection;
        Vector3 worldPosition;
        float padding;
    };

    void Initialize();
    void Update();
    void ImGuiControl();
    void Clear();

    // --- ウィンドウ表示制御 ---
    static bool* GetShowWindowPtr() { return &showWindow_; }
    static bool IsWindowOpen() { return showWindow_; }
    static void SetWindowOpen(bool open) { showWindow_ = open; }

    // --- カメラ管理 ---
    void AddCamera(const std::string& name, std::shared_ptr<BaseCamera> camera);
    void SetActiveCamera(const std::string& name, bool log = true);
    BaseCamera* GetActiveCamera() { return activeCamera_; }
    std::string GetActiveCameraName() const {
        for (const auto& [name, camera] : cameras_) {
            if (camera.get() == activeCamera_) {
                return name;
            }
        }
        return "";
    }
    bool HasCamera(const std::string& name) const {
        return cameras_.find(name) != cameras_.end();
    }
    BaseCamera* GetCamera(const std::string& name) {
        auto it = cameras_.find(name);
        return (it != cameras_.end()) ? it->second.get() : nullptr;
    }

    // --- 3D行列ゲッター (アクティブカメラから取得) ---
    const Matrix4x4& GetViewMatrix3D() const {
        if (!activeCamera_) {
            static const Matrix4x4 identity = {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1
            };
            return identity;
        }
        return activeCamera_->GetViewMatrix();
    }
    const Matrix4x4& GetProjectionMatrix3D() const {
        if (!activeCamera_) {
            static const Matrix4x4 identity = {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1
            };
            return identity;
        }
        return activeCamera_->GetProjectionMatrix();
    }

    // --- 2D行列ゲッター/セッター ---
    const Matrix4x4& GetViewMatrix2D() const { return viewMatrix2D_; }
    const Matrix4x4& GetProjectionMatrix2D() const { return projectionMatrix2D_; }
    void SetProjectionMatrix2D(const Matrix4x4& projection) { projectionMatrix2D_ = projection; }

    // GPUアドレス取得
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return resource_->GetGPUVirtualAddress(); }
    void UpdateAllProjection(float aspect);

private:
    static inline bool showWindow_ = true;

    std::map<std::string, std::shared_ptr<BaseCamera>> cameras_;
    BaseCamera* activeCamera_ = nullptr;

    // 2D用行列
    Matrix4x4 viewMatrix2D_;
    Matrix4x4 projectionMatrix2D_;

    // 定数バッファ関連
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    CameraForGPU* data_ = nullptr;
};
