#include "Engine/Graphics/Objects/3d/Skybox/Skybox.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/PSO/Manager/PSOManager.h"

void Skybox::Initialize() {
    auto device = EngineResource::GetEngine()->GetDevice();

    // 1. 頂点バッファの作成
    vertexResource_ = DxUtils::CreateBufferResource(device, sizeof(SkyboxVertex) * 8);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(SkyboxVertex) * 8;
    vbView_.StrideInBytes = sizeof(SkyboxVertex);

    SkyboxVertex* vertices = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertices));
    // 前面
    vertices[0].position = { -1.0f, -1.0f, -1.0f, 1.0f }; // 左下前
    vertices[1].position = { -1.0f,  1.0f, -1.0f, 1.0f }; // 左上前
    vertices[2].position = {  1.0f, -1.0f, -1.0f, 1.0f }; // 右下前
    vertices[3].position = {  1.0f,  1.0f, -1.0f, 1.0f }; // 右上前
    // 後面
    vertices[4].position = { -1.0f, -1.0f,  1.0f, 1.0f }; // 左下奥
    vertices[5].position = { -1.0f,  1.0f,  1.0f, 1.0f }; // 左上奥
    vertices[6].position = {  1.0f, -1.0f,  1.0f, 1.0f }; // 右下奥
    vertices[7].position = {  1.0f,  1.0f,  1.0f, 1.0f }; // 右上奥
    vertexResource_->Unmap(0, nullptr);

    // 2. インデックスバッファの作成
    uint16_t indices[] = {
        // 前面
        0, 1, 2, 2, 1, 3,
        // 右面
        2, 3, 6, 6, 3, 7,
        // 後面
        6, 7, 4, 4, 7, 5,
        // 左面
        4, 5, 0, 0, 5, 1,
        // 上面
        1, 5, 3, 3, 5, 7,
        // 下面
        4, 0, 6, 6, 0, 2
    };
    indexResource_ = DxUtils::CreateBufferResource(device, sizeof(indices));
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(indices);
    ibView_.Format = DXGI_FORMAT_R16_UINT;

    uint16_t* indexData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    std::copy(std::begin(indices), std::end(indices), indexData);
    indexResource_->Unmap(0, nullptr);

    // 3. WVP用の定数バッファ作成 (DirectX12の256バイトアライメントに準拠)
    constexpr size_t kConstantBufferSize = (sizeof(WVPData) + 0xff) & ~0xff;
    wvpResource_ = DxUtils::CreateBufferResource(device, kConstantBufferSize);
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->WVP = Math::MakeIdentity();
    wvpData_->World = Math::MakeIdentity();
    wvpData_->color = color_;
}

void Skybox::Update() {
    auto camera = CameraResource::GetCameraManager();
    Matrix4x4 view = camera->GetViewMatrix3D();
    Matrix4x4 projection = camera->GetProjectionMatrix3D();

    // 常にカメラの中心に追従させる
    transform_.translate = { 
        view.m[3][0] * -1.0f, // viewから直接取るより、カメラの座標を取るのが安全だが、今回はビュー行列に頼らず実装するため
        // ... と思ったが、ここはWVPで行うため厳密にはビュー行列を使って計算
        // 正確にはカメラのTransform.translateが必要（カメラクラス構成による）
    };
    
    // ZuiZuiEngineでは CameraManager 内部などで World->View を計算している。
    // 今回はビュー行列の平行移動成分を強引に無効にするアプローチ(より確実)を採用することもできるが、
    // シェーダー側で xyww を使い常に Z=1.0 にしているため、ここでは単純にカメラと同じ位置に置くのが簡単。
    // 仮にカメラに GetPosition() 相当が無ければ、ビュー逆行列から引っ張る等。
    // 一旦、カメラの逆行列（カメラ自身のワールド行列）から平行移動を抜き出す
    Matrix4x4 viewInv = Math::Inverse(view);
    transform_.translate = { viewInv.m[3][0], viewInv.m[3][1], viewInv.m[3][2] };

    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvp = Math::Multiply(Math::Multiply(world, view), projection);

    wvpData_->WVP = wvp;
    wvpData_->World = world;
    wvpData_->color = color_;
}

void Skybox::Draw(const std::string& textureKey) {
    if (textureKey.empty()) return;

    auto commandList = EngineResource::GetEngine()->GetDxCommon()->GetCommandList();
    auto psoMgr = EngineResource::GetEngine()->GetPSOManager();

    // PSOとRootSignatureの設定
    commandList->SetGraphicsRootSignature(psoMgr->GetRootSignature("Skybox"));
    commandList->SetPipelineState(psoMgr->GetPSO("Skybox"));

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vbView_);
    commandList->IASetIndexBuffer(&ibView_);

    // 定数バッファとテクスチャの設定
    commandList->SetGraphicsRootConstantBufferView(0, wvpResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, TextureResource::GetTextureManager()->GetGpuHandle(textureKey));

    // 描画
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
