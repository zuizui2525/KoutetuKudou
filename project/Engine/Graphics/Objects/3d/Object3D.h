#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/RenderStructs.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Debug/IGameObject.h"

class Zuizui;

class Object3D : public Base3D, public IGameObject {
public:
    virtual ~Object3D();
    virtual void Initialize(int lightingMode = 1);
    virtual void Update() override;

    /// @brief WVP(World, View, Projection)行列の定数バッファを取得します
    ID3D12Resource* GetWVPResource() const { return wvpResource_.Get(); }
    /// @brief マテリアル(材質)の定数バッファを取得します
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    
    /// @brief 3Dトランスフォーム(位置・回転・スケール)を取得します
    Transform& GetTransform() { return transform_; }
    /// @brief スケール(大きさ)を取得します
    Vector3& GetScale() { return transform_.scale; }
    /// @brief 回転(オイラー角: ラジアン)を取得します
    Vector3& GetRotate() { return transform_.rotate; }
    /// @brief 座標(位置)を取得します
    Vector3& GetPosition() { return transform_.translate; }
    
    /// @brief UVトランスフォーム(テクスチャのスクロールや拡縮用)を取得します
    Transform& GetUVTransform() { return uvTransform_; }
    /// @brief マテリアルデータ(色や光沢度などの実体)のポインタを取得します
    Material* GetMaterialData() { return materialData_; }
    /// @brief WVPデータ(行列情報の実体)のポインタを取得します
    TransformationMatrix* GetWvpData() { return wvpData_; }
    
    /// @brief 現在のライティング設定を取得します (0:無効, 1:Lambert, 2:HalfLambert)
    int GetLightingMode() const { return materialData_ ? materialData_->enableLighting : 0; }
    /// @brief オブジェクトの基本色(RGBA)を取得します
    Vector4 GetColor() const { return materialData_ ? materialData_->color : Vector4{1.0f, 1.0f, 1.0f, 1.0f}; }
    /// @brief オブジェクトの透明度(0.0f〜1.0f)を取得します
    float GetAlpha() const { return materialData_ ? materialData_->color.w : 1.0f; }
    /// @brief スペキュラ(ハイライト)の鋭さを取得します
    float GetShininess() const { return materialData_ ? materialData_->shininess : 30.0f; }
    /// @brief 環境マップの反射係数(0.0fで反射なし, 1.0fで完全反射)を取得します
    float GetEnvironmentCoefficient() const { return materialData_ ? materialData_->environmentCoefficient : 0.0f; }
    /// @brief オブジェクトが現在描画される状態かどうかを取得します
    bool GetIsVisible() const { return IsVisible(); }

    // ==========================================
    // Setter (情報設定)
    // ==========================================

    /// @brief 3Dトランスフォーム(位置・回転・スケール)をまとめて設定します
    void SetTransform(Transform transform) { transform_ = transform; }
    /// @brief スケール(大きさ)を設定します (例: Vector3(2,2,2) で2倍)
    void SetScale(Vector3 scale) { transform_.scale = scale; }
    /// @brief 回転(オイラー角: ラジアン)を設定します
    void SetRotate(Vector3 rotate) { transform_.rotate = rotate; }
    /// @brief 座標(位置)を設定します
    void SetPosition(Vector3 position) { transform_.translate = position; }
    
    /// @brief オブジェクトの基本色を設定します (R, G, B, A) 1.0fが最大値です
    void SetColor(const Vector4& color) { if (materialData_) { materialData_->color = color; } }
    /// @brief オブジェクトの透明度を設定します (0.0f:完全透明 〜 1.0f:不透明)
    void SetAlpha(float alpha) { if (materialData_) { materialData_->color.w = alpha; } }
    /// @brief スペキュラ(ハイライト)の鋭さを設定します (数値が大きいほど鋭いハイライトになります)
    void SetShininess(float shininess) { if (materialData_) { materialData_->shininess = shininess; } }
    /// @brief 環境マップの反射係数を設定します (0.0f:反射なし 〜 1.0f:完全反射(鏡面))
    void SetEnvironmentCoefficient(float coef) { if (materialData_) { materialData_->environmentCoefficient = coef; } }
    /// @brief オブジェクトの表示/非表示を切り替えます (falseで描画されなくなります)
    void SetIsVisible(bool isVisible) { SetVisible(isVisible); }
    /// @brief ライティングの種類を設定します (0:無効, 1:Lambert(通常), 2:HalfLambert(暗部を明るく))
    void SetLightingMode(int lightingMode) {
        if (materialData_) {
            materialData_->enableLighting = lightingMode;
        }
    }

    void SetParent(Object3D* parent) { parent_ = parent; }
    Object3D* GetParent() const { return parent_; }
    void SetParentMatrix(const Matrix4x4* parentMatrix) { parentMatrix_ = parentMatrix; }
    const Matrix4x4* GetParentMatrix() const { return parentMatrix_; }
    const Matrix4x4& GetWorldMatrix() const { return matWorld_; }
    Vector3 GetWorldPosition() const;

    /// @brief ヒエラルキーへの自動登録を制御（コンポーネント内部メッシュの場合はfalseに設定）
    void SetAutoRegisterHierarchy(bool autoRegister) { autoRegisterHierarchy_ = autoRegister; }
    bool IsAutoRegisterHierarchy() const { return autoRegisterHierarchy_; }

    void DrawInspector() override;

protected:
    // ImGui
    void ImGuiSRTControl(const std::string& name);
    void ImGuiLightingControl(const std::string& name);

    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

    // CPU 側 Transform
    Transform transform_{};
    Transform uvTransform_{};
    Material* materialData_ = nullptr;
    TransformationMatrix* wvpData_ = nullptr;

    // 親オブジェクトとワールド行列
    Object3D* parent_ = nullptr;
    const Matrix4x4* parentMatrix_ = nullptr;
    Matrix4x4 matWorld_ = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    bool autoRegisterHierarchy_ = true;
};
