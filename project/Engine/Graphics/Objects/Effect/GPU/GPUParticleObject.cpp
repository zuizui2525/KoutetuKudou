#define NOMINMAX
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Graphics/PSO/RootSignature/RootSignatureBuilder.h"
#include "Engine/Graphics/PSO/Shader/ShaderProgram.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <imgui.h>
#include <algorithm>
#include <cassert>

namespace {
    // HLSL側の GPUParticle と完全に同一のメモリ配置 (64バイト)
    struct HLSLParticle {
        Vector3 position;
        float   lifeTime;
        Vector3 velocity;
        float   currentTime;
        Vector3 scale;
        uint32_t isActive;
        Vector4 color;
    };

    // HLSL側の GPUEmitterConstant と同一のメモリ配置 (256バイト境界アライメント)
    struct alignas(256) HLSLEmitterConstant {
        Vector3 emitterPos;
        float   deltaTime;

        Vector3 startScale;
        float   gravity;

        Vector3 endScale;
        float   killAtY;

        Vector4 startColor;
        Vector4 endColor;

        Vector3 minVelocity;
        float   minLifeTime;

        Vector3 maxVelocity;
        float   maxLifeTime;

        uint32_t maxParticles;
        uint32_t emitOffset;
        uint32_t emitCount;
        uint32_t randomSeed;

        uint32_t padding[16]; // 256バイト安全パディング
    };

    // カメラ定数バッファ
    struct alignas(256) HLSLCameraConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
        Vector3   cameraPos;
        float     padding;
    };

}

GPUParticleObject::GPUParticleObject() = default;

GPUParticleObject::~GPUParticleObject() {
    // GPU処理の同期待機
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }

    // デスクリプタインデックスの返却（再利用可能化）
    if (auto texMgr = TextureResource::GetTextureManager()) {
        if (uavDescriptorIndex_ > 0) {
            texMgr->FreeDescriptorIndex(uavDescriptorIndex_);
            uavDescriptorIndex_ = 0;
        }
        if (srvDescriptorIndex_ > 0) {
            texMgr->FreeDescriptorIndex(srvDescriptorIndex_);
            srvDescriptorIndex_ = 0;
        }
    }
}

bool GPUParticleObject::Initialize(uint32_t maxParticles) {
    maxParticles_ = maxParticles;
    ID3D12Device* device = EngineResource::GetEngine()->GetDevice();
    assert(device != nullptr);

    if (!CreateBuffersAndDescriptors(device)) {
        return false;
    }

    if (!CreatePipelines(device)) {
        return false;
    }

    isInitialized_ = true;
    return true;
}

bool GPUParticleObject::CreateBuffersAndDescriptors(ID3D12Device* device) {
    auto dxCommon = EngineResource::GetEngine()->GetDxCommon();
    ID3D12DescriptorHeap* srvHeap = dxCommon->GetSrvHeap();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // デスクリプタ割り当て (TextureManager の一元アロケータを使用)
    auto texMgr = TextureResource::GetTextureManager();
    assert(texMgr != nullptr);
    uavDescriptorIndex_ = texMgr->AllocateDescriptorIndex();
    srvDescriptorIndex_ = texMgr->AllocateDescriptorIndex();

    uavCpuHandle_ = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, uavDescriptorIndex_);
    uavGpuHandle_ = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, uavDescriptorIndex_);
    srvCpuHandle_ = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, srvDescriptorIndex_);
    srvGpuHandle_ = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, srvDescriptorIndex_);

    // 1. パーティクル構造化バッファの生成 (Defaultヒープ, UAV対応)
    const size_t bufferSize = sizeof(HLSLParticle) * maxParticles_;
    particleBuffer_ = DxUtils::CreateUAVBufferResource(device, bufferSize, D3D12_RESOURCE_STATE_COMMON);
    currentBufferState_ = D3D12_RESOURCE_STATE_COMMON;

    // 2. UAV の生成
    DxUtils::CreateBufferUAV(
        device,
        particleBuffer_.Get(),
        maxParticles_,
        static_cast<uint32_t>(sizeof(HLSLParticle)),
        uavCpuHandle_);

    // 3. SRV の生成 (頂点シェーダーでの読み取り用)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = maxParticles_;
    srvDesc.Buffer.StructureByteStride = static_cast<uint32_t>(sizeof(HLSLParticle));
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(particleBuffer_.Get(), &srvDesc, srvCpuHandle_);

    // 4. エミッター定数バッファの生成 (Uploadヒープ)
    emitterConstantBuffer_ = DxUtils::CreateBufferResource(device, sizeof(HLSLEmitterConstant));

    // 5. カメラ定数バッファの生成 (Uploadヒープ)
    cameraConstantBuffer_ = DxUtils::CreateBufferResource(device, sizeof(HLSLCameraConstant));

    return true;
}

bool GPUParticleObject::CreatePipelines(ID3D12Device* device) {
    auto dxCommon = EngineResource::GetEngine()->GetDxCommon();

    // -----------------------------------------------------------------
    // 1. Compute Pipeline (シミュレーションCS) の生成
    // -----------------------------------------------------------------
    RootSignatureBuilder csRs;
    csRs.AddUAV(0, D3D12_SHADER_VISIBILITY_ALL); // u0: RWStructuredBuffer
    csRs.AddCBV(0, D3D12_SHADER_VISIBILITY_ALL); // b0: ConstantBuffer<GPUEmitterConstant>
    auto computeRootSig = csRs.Build(device, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    assert(computeRootSig != nullptr);

    ShaderProgram csProgram;
    bool csCompiled = csProgram.CompileCS(
        L"resources/Shader/GPUParticle/GPUParticleUpdate.CS.hlsl",
        dxCommon->GetDxcUtils(),
        dxCommon->GetDxcCompiler(),
        dxCommon->GetIncludeHandler());
    assert(csCompiled && "GPUParticleUpdate.CS.hlsl compile failed!");

    if (!computePipeline_.Initialize(device, computeRootSig.Get(), csProgram.GetCS())) {
        return false;
    }

    // -----------------------------------------------------------------
    // 2. Render Pipeline (頂点バッファレス・インスタンシング描画) の生成
    // -----------------------------------------------------------------
    RootSignatureBuilder vsRs;
    vsRs.AddCBV(0, D3D12_SHADER_VISIBILITY_VERTEX); // b0: カメラ定数
    vsRs.AddSRV(0, D3D12_SHADER_VISIBILITY_VERTEX); // t0: パーティクルバッファ (VS)
    vsRs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);  // t0: テクスチャ (PS)

    D3D12_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    vsRs.AddSampler(samplerDesc, 0);

    renderRootSignature_ = vsRs.Build(device, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    assert(renderRootSignature_ != nullptr);

    ShaderProgram renderProgram;
    bool vsCompiled = renderProgram.CompileVS(
        L"resources/Shader/GPUParticle/GPUParticle.VS.hlsl",
        dxCommon->GetDxcUtils(),
        dxCommon->GetDxcCompiler(),
        dxCommon->GetIncludeHandler());
    assert(vsCompiled && "GPUParticle.VS.hlsl compile failed!");

    bool psCompiled = renderProgram.CompilePS(
        L"resources/Shader/GPUParticle/GPUParticle.PS.hlsl",
        dxCommon->GetDxcUtils(),
        dxCommon->GetDxcCompiler(),
        dxCommon->GetIncludeHandler());
    assert(psCompiled && "GPUParticle.PS.hlsl compile failed!");

    // 加算合成（Additive Blend）
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE; // 加算
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // カリングなし（ビルボード）
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // 深度テスト有効、書き込み無効（半透明パーティクルの必須設定）
    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = renderRootSignature_.Get();
    psoDesc.VS = renderProgram.GetVS();
    psoDesc.PS = renderProgram.GetPS();
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.InputLayout = { nullptr, 0 }; // 頂点シェーダー内で自動展開するため頂点レイアウト不要
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = dxCommon->GetRtvFormat();
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderPipelineState_));
    assert(SUCCEEDED(hr) && "Failed to create GPU Particle Render PSO!");

    return true;
}

void GPUParticleObject::Update(float deltaTime) {
    if (!isInitialized_) return;

    randomSeedCounter_++;

    // 1. 自動発生分の加算
    if (setting_.isAutoSpawning && setting_.spawnRatePerSec > 0.0f) {
        spawnAccumulator_ += setting_.spawnRatePerSec * deltaTime;
    }

    // 2. 蓄積された放出要求（手動バーストおよび自動発生分）を今フレームの放出数として消費
    emitCountThisFrame_ = 0;
    auto toSpawn = static_cast<uint32_t>(spawnAccumulator_);
    if (toSpawn > 0) {
        emitCountThisFrame_ = (std::min)(toSpawn, maxParticles_);
        spawnAccumulator_ -= static_cast<float>(emitCountThisFrame_);
    }

    // 定数バッファの更新 (Uploadヒープへ書き込み)
    HLSLEmitterConstant cbData{};
    cbData.emitterPos   = setting_.emitterPosition;
    cbData.deltaTime    = deltaTime;
    cbData.startScale   = setting_.startScale;
    cbData.gravity      = setting_.gravity;
    cbData.endScale     = setting_.endScale;
    cbData.killAtY      = setting_.killAtY;
    cbData.startColor   = setting_.startColor;
    cbData.endColor     = setting_.endColor;
    cbData.minVelocity  = setting_.minVelocity;
    cbData.minLifeTime  = setting_.minLifeTime;
    cbData.maxVelocity  = setting_.maxVelocity;
    cbData.maxLifeTime  = setting_.maxLifeTime;
    cbData.maxParticles = maxParticles_;
    cbData.emitOffset   = currentEmitOffset_;
    cbData.emitCount    = emitCountThisFrame_;
    cbData.randomSeed   = randomSeedCounter_;

    // 全粒子即時消滅リクエストの処理
    if (requestClearAll_) {
        cbData.killAtY = 999999.0f;
        cbData.emitCount = 0;
        requestClearAll_ = false;
    }

    void* mappedEmitter = nullptr;
    if (SUCCEEDED(emitterConstantBuffer_->Map(0, nullptr, &mappedEmitter))) {
        memcpy(mappedEmitter, &cbData, sizeof(HLSLEmitterConstant));
        emitterConstantBuffer_->Unmap(0, nullptr);
    }

    // 次回発生オフセットの進捗
    if (emitCountThisFrame_ > 0) {
        currentEmitOffset_ = (currentEmitOffset_ + emitCountThisFrame_) % maxParticles_;
    }

    // カメラ定数バッファの更新
    HLSLCameraConstant camData{};
    bool hasCam = false;

    if (useCustomCamera_) {
        camData.view = customViewMatrix_;
        camData.projection = customProjMatrix_;
        camData.viewProjection = Math::Multiply(camData.view, camData.projection);
        camData.cameraPos = customCameraPos_;
        hasCam = true;
    } else {
        auto cameraMgr = CameraResource::GetCameraManager();
        if (cameraMgr && cameraMgr->GetActiveCamera()) {
            auto* cam = cameraMgr->GetActiveCamera();
            camData.view = cam->GetViewMatrix();
            camData.projection = cam->GetProjectionMatrix();
            camData.viewProjection = Math::Multiply(camData.view, camData.projection);
            camData.cameraPos = cam->GetPosition();
            hasCam = true;
        }
    }

    if (hasCam) {
        void* mappedCam = nullptr;
        if (SUCCEEDED(cameraConstantBuffer_->Map(0, nullptr, &mappedCam))) {
            memcpy(mappedCam, &camData, sizeof(HLSLCameraConstant));
            cameraConstantBuffer_->Unmap(0, nullptr);
        }
    }
}

void GPUParticleObject::TransitionBuffer(
    ID3D12GraphicsCommandList* cmdList,
    D3D12_RESOURCE_STATES targetState) {
    if (currentBufferState_ == targetState) {
        // 同じ状態なら UAV バリアを配置して直前の書き込み完了を保証
        if (targetState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = particleBuffer_.Get();
            cmdList->ResourceBarrier(1, &barrier);
        }
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = particleBuffer_.Get();
    barrier.Transition.StateBefore = currentBufferState_;
    barrier.Transition.StateAfter = targetState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    currentBufferState_ = targetState;
}

void GPUParticleObject::DispatchCompute(ID3D12GraphicsCommandList* cmdList) {
    if (!isInitialized_ || !cmdList) return;

    // 1. バッファを UAV 書き込み状態へ遷移
    TransitionBuffer(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // 2. ディスクリプタヒープの設定
    auto dxCommon = EngineResource::GetEngine()->GetDxCommon();
    ID3D12DescriptorHeap* heaps[] = { dxCommon->GetSrvHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    // 3. Compute Pipeline とルートシグネチャのバインド
    computePipeline_.Bind(cmdList);

    // 4. パラメータのバインド
    // param 0: Descriptor Table (u0: RWStructuredBuffer)
    cmdList->SetComputeRootDescriptorTable(0, uavGpuHandle_);
    // param 1: CBV (b0: GPUEmitterConstant)
    cmdList->SetComputeRootConstantBufferView(1, emitterConstantBuffer_->GetGPUVirtualAddress());

    // 5. ディスパッチ実行 (256スレッド/グループ)
    const uint32_t groupCountX = (maxParticles_ + kComputeThreadGroupSize - 1) / kComputeThreadGroupSize;
    cmdList->Dispatch(groupCountX, 1, 1);
}

void GPUParticleObject::Draw(ID3D12GraphicsCommandList* cmdList, const std::string& textureKey) {
    if (!isInitialized_ || !cmdList) return;

    // 1. バッファを 頂点シェーダー読み取り状態 (SRV) へ遷移
    TransitionBuffer(cmdList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // 2. ディスクリプタヒープの設定
    auto dxCommon = EngineResource::GetEngine()->GetDxCommon();
    ID3D12DescriptorHeap* heaps[] = { dxCommon->GetSrvHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    // 3. Render Pipeline とルートシグネチャのバインド
    cmdList->SetGraphicsRootSignature(renderRootSignature_.Get());
    cmdList->SetPipelineState(renderPipelineState_.Get());

    // 4. プリミティブトポロジーの設定 (TRIANGLELIST)
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 5. パラメータのバインド
    // param 0: CBV (b0, VS: カメラ定数)
    cmdList->SetGraphicsRootConstantBufferView(0, cameraConstantBuffer_->GetGPUVirtualAddress());
    // param 1: Descriptor Table (t0, VS: パーティクルバッファ)
    cmdList->SetGraphicsRootDescriptorTable(1, srvGpuHandle_);
    // param 2: Descriptor Table (t0, PS: テクスチャ)
    const std::string& actualTextureKey = (!textureKey.empty() && textureKey != "white") ? textureKey : currentTextureKey_;
    auto textureMgr = TextureResource::GetTextureManager();
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle = textureMgr->GetGpuHandle(actualTextureKey);
    cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

    // 6. 頂点バッファレス・インスタンシング描画
    // 四角形1枚(6頂点) × maxParticles_ インスタンス
    cmdList->DrawInstanced(kQuadVertexCount, maxParticles_, 0, 0);
}

void GPUParticleObject::EmitBurst(uint32_t count) {
    if (!isInitialized_ || count == 0) return;
    spawnAccumulator_ += static_cast<float>(count);
}

void GPUParticleObject::ImGuiControl(const std::string& label) {
#ifdef _USEIMGUI
    if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("GPU Particle (Compute Shader Active)");
        ImGui::Text("Total Allocated VRAM: %u particles (%.2f MB)",
            maxParticles_,
            static_cast<float>(sizeof(HLSLParticle) * maxParticles_) / (1024.0f * 1024.0f));

        ImGui::Separator();
        ImGui::DragFloat3("Emitter Position", &setting_.emitterPosition.x, 0.1f);
        ImGui::Checkbox("Auto Spawning", &setting_.isAutoSpawning);
        ImGui::DragFloat("Spawn Rate (/sec)", &setting_.spawnRatePerSec, 10.0f, 0.0f, 50000.0f);

        if (ImGui::Button("Burst 1,000")) {
            EmitBurst(1000);
        }
        ImGui::SameLine();
        if (ImGui::Button("Burst 10,000")) {
            EmitBurst(10000);
        }
        ImGui::SameLine();
        if (ImGui::Button("Burst 50,000")) {
            EmitBurst(50000);
        }

        ImGui::Separator();
        ImGui::DragFloat("Gravity", &setting_.gravity, 0.1f, kGravityMin, kGravityMax);
        ImGui::DragFloat2("LifeTime Range (Min/Max)", &setting_.minLifeTime, 0.05f, 0.1f, 10.0f);
        ImGui::DragFloat3("Min Velocity", &setting_.minVelocity.x, 0.1f, -50.0f, 50.0f);
        ImGui::DragFloat3("Max Velocity", &setting_.maxVelocity.x, 0.1f, -50.0f, 50.0f);

        ImGui::Separator();
        ImGui::DragFloat3("Start Scale", &setting_.startScale.x, 0.05f, 0.01f, 10.0f);
        ImGui::DragFloat3("End Scale", &setting_.endScale.x, 0.05f, 0.0f, 10.0f);
        ImGui::ColorEdit4("Start Color", &setting_.startColor.x);
        ImGui::ColorEdit4("End Color", &setting_.endColor.x);
    }
#endif
}

void GPUParticleObject::ApplySetting(const GPUParticleSetting& s) {
    name_ = s.name;
    currentTextureKey_ = s.textureKey;

    setting_.emitterPosition = s.emitterPosition;
    setting_.startScale = s.startScale;
    setting_.endScale = s.endScale;
    setting_.startColor = s.startColor;
    setting_.endColor = s.endColor;
    setting_.minVelocity = s.minVelocity;
    setting_.maxVelocity = s.maxVelocity;
    setting_.minLifeTime = s.minLifeTime;
    setting_.maxLifeTime = s.maxLifeTime;
    setting_.gravity = s.gravity;
    setting_.killAtY = s.killAtY;
    setting_.spawnRatePerSec = s.spawnRatePerSec;
    setting_.isAutoSpawning = s.isAutoSpawning;
}

GPUParticleSetting GPUParticleObject::ToParticleSetting() const {
    GPUParticleSetting s;
    s.name = name_;
    s.textureKey = currentTextureKey_;

    s.emitterPosition = setting_.emitterPosition;
    s.startScale = setting_.startScale;
    s.endScale = setting_.endScale;
    s.startColor = setting_.startColor;
    s.endColor = setting_.endColor;
    s.minVelocity = setting_.minVelocity;
    s.maxVelocity = setting_.maxVelocity;
    s.minLifeTime = setting_.minLifeTime;
    s.maxLifeTime = setting_.maxLifeTime;
    s.gravity = setting_.gravity;
    s.killAtY = setting_.killAtY;
    s.spawnRatePerSec = setting_.spawnRatePerSec;
    s.isAutoSpawning = setting_.isAutoSpawning;

    return s;
}

void GPUParticleObject::SetCustomCamera(const Matrix4x4& view, const Matrix4x4& proj, const Vector3& cameraPos) {
    useCustomCamera_ = true;
    customViewMatrix_ = view;
    customProjMatrix_ = proj;
    customCameraPos_ = cameraPos;
}

void GPUParticleObject::Reset() {
    currentEmitOffset_ = 0;
    emitCountThisFrame_ = 0;
    spawnAccumulator_ = 0.0f;
    randomSeedCounter_ = 0;
    requestClearAll_ = true;
}
