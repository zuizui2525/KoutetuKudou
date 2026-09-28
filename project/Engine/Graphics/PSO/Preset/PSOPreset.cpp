#include "Engine/Graphics/PSO/Preset/PSOPreset.h"
#include <cassert>
#include <iostream>

PSOPreset PSOPreset::CreateObject3DPreset(
    ID3D12Device* device,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {

    PSOPreset preset;

    // ---------------------------
    // 1. RootSignature
    // ---------------------------
    RootSignatureBuilder rs;
    // 行列 (b0, VS)
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
    // マテリアル(b0, PS)
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
    // カメラ (b1, PS)
    rs.AddCBV(1, D3D12_SHADER_VISIBILITY_PIXEL);
    // ライティング (b2, PS) directionalLight
    rs.AddCBV(2, D3D12_SHADER_VISIBILITY_PIXEL);
    // ライティング (b3, PS) pointLight
    rs.AddCBV(3, D3D12_SHADER_VISIBILITY_PIXEL);
    // ライティング (b4, PS) spotLight
    rs.AddCBV(4, D3D12_SHADER_VISIBILITY_PIXEL);
    // テクスチャ (t0, PS)
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 環境マップテクスチャ (t1, PS)
    rs.AddSRV(1, D3D12_SHADER_VISIBILITY_PIXEL);
    // サンプラー (s0)
    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rs.AddSampler(sampler, 0);

    preset.rootSignature = rs.Build(device);
    assert(preset.rootSignature && "RootSignature creation failed!");

    // ---------------------------
    // 2. InputLayout
    // ---------------------------
    preset.ilBuilder_.Add("POSITION", DXGI_FORMAT_R32G32B32A32_FLOAT);
    preset.ilBuilder_.Add("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    preset.ilBuilder_.Add("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);

    // ここでBuildするが、PSOManagerへのコピー後に無効化されるため、
    // Register時に再Buildが必要（PSOManager側で対応）
    preset.inputLayoutDesc = preset.ilBuilder_.Build();

    // ---------------------------
    // 3. Blend State
    // ---------------------------
    BlendStateBuilder blendBuilder;
    blendBuilder.SetBlendMode(kBlendModeNormal);
    preset.blendDesc = blendBuilder.Build();

    // ---------------------------
    // 4. Rasterizer State
    // ---------------------------
    RasterizerStateBuilder rsb;
    rsb.SetCullMode(CullMode::Back); // 通常はBackカリング推奨
    preset.rasterizerDesc = rsb.Build();

    // ---------------------------
    // 5. Depth Stencil State
    // ---------------------------
    DepthStencilStateBuilder dsb;
    dsb.SetDepthEnable(true);
    preset.depthStencilDesc = dsb.GetDesc();

    // ---------------------------
    // 6. Shader
    // ---------------------------
    // パスはプロジェクトの構成に合わせて正確に記述してください
    // VS
    bool vsResult = preset.shaderProgram.CompileVS(
        L"resources/Shader/Object3d/Object3d.VS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    // ★失敗したらここで止める
    assert(vsResult && "Vertex Shader Compile Failed! Check filepath or code.");

    // PS
    bool psResult = preset.shaderProgram.CompilePS(
        L"resources/Shader/Object3d/Object3d.PS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    // ★失敗したらここで止める
    assert(psResult && "Pixel Shader Compile Failed! Check filepath or code.");

    return preset;
}

PSOPreset PSOPreset::CreateObject3DAlphaPreset(
    ID3D12Device* device,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {

    PSOPreset preset;

    // 1. RootSignature (Object3Dと同じ)
    RootSignatureBuilder rs;
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
    rs.AddCBV(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rs.AddCBV(2, D3D12_SHADER_VISIBILITY_PIXEL);
    rs.AddCBV(3, D3D12_SHADER_VISIBILITY_PIXEL);
    rs.AddCBV(4, D3D12_SHADER_VISIBILITY_PIXEL);
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);
    rs.AddSRV(1, D3D12_SHADER_VISIBILITY_PIXEL);
    
    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_ANISOTROPIC;
    sampler.MaxAnisotropy = 16;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rs.AddSampler(sampler, 0);

    preset.rootSignature = rs.Build(device);
    assert(preset.rootSignature && "RootSignature creation failed!");

    // 2. InputLayout
    preset.ilBuilder_.Add("POSITION", DXGI_FORMAT_R32G32B32A32_FLOAT);
    preset.ilBuilder_.Add("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    preset.ilBuilder_.Add("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
    preset.inputLayoutDesc = preset.ilBuilder_.Build();

    // 3. Blend State (アルファブレンド有効)
    BlendStateBuilder blendBuilder;
    blendBuilder.SetBlendMode(kBlendModeNormal);
    preset.blendDesc = blendBuilder.Build();

    // 4. Rasterizer State
    RasterizerStateBuilder rsb;
    rsb.SetCullMode(CullMode::Back);
    preset.rasterizerDesc = rsb.Build();

    // 5. Depth Stencil State (深度テスト有効、深度書き込み無効)
    DepthStencilStateBuilder dsb;
    dsb.SetDepthEnable(true);
    dsb.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO); // 深度書き込み無効
    preset.depthStencilDesc = dsb.GetDesc();

    // 6. Shader (Object3dと同じ)
    bool vsResult = preset.shaderProgram.CompileVS(
        L"resources/Shader/Object3d/Object3d.VS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    assert(vsResult && "Vertex Shader Compile Failed! Check filepath or code.");

    bool psResult = preset.shaderProgram.CompilePS(
        L"resources/Shader/Object3d/Object3d.PS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    assert(psResult && "Pixel Shader Compile Failed! Check filepath or code.");

    return preset;
}

PSOPreset PSOPreset::CreateParticlePreset(
    ID3D12Device* device,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {

    PSOPreset preset;

    // --------------------------------------------------------
    // 1. RootSignature の構築 (Object3Dとの最大の違い)
    // --------------------------------------------------------
    RootSignatureBuilder rs;

    // ■ 行列データ (VS)
    // Object3Dでは CBV(b0) でしたが、
    // Instancingでは StructuredBuffer(t0) を使うため SRV に変更します。
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_VERTEX); // register(t0)

    // ■ マテリアル (PS)
    // これはObject3Dと同じく定数バッファ(b0)
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);  // register(b0)

    // ■ DirectionalLight (PS)
    // シェーダーに残っていたライティング用(b1)
    rs.AddCBV(1, D3D12_SHADER_VISIBILITY_PIXEL);  // register(b1)

    // ■ Texture (PS)
    // テクスチャ(t0)。VSのt0とはシェーダーステージが違うので被ってもOK
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);  // register(t0)

    // ■ Sampler
    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // RingなどのためにV方向をクランプ
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rs.AddSampler(sampler, 0);

    // RootSignatureをビルド
    preset.rootSignature = rs.Build(device);


    // --------------------------------------------------------
    // 2. Input Layout
    // --------------------------------------------------------
    preset.ilBuilder_.Add("POSITION", DXGI_FORMAT_R32G32B32A32_FLOAT);
    preset.ilBuilder_.Add("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    preset.ilBuilder_.Add("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
    preset.inputLayoutDesc = preset.ilBuilder_.Build();


    // --------------------------------------------------------
    // 3. Blend State
    // --------------------------------------------------------
    BlendStateBuilder blendBuilder;
    // パーティクルなので「加算合成(kBlendModeAdd)」がよく使われますが、
    // まずは動作確認のため「通常(Normal)」にしておきます。
    blendBuilder.SetBlendMode(kBlendModeAdd);
    preset.blendDesc = blendBuilder.Build();


    // --------------------------------------------------------
    // 4. Rasterizer State
    // --------------------------------------------------------
    RasterizerStateBuilder rsb;
    rsb.SetCullMode(CullMode::None); // 両面を表示する（エフェクト用なので）
    preset.rasterizerDesc = rsb.Build();


    // --------------------------------------------------------
    // 5. Depth Stencil State
    // --------------------------------------------------------
    DepthStencilStateBuilder dsb;
    dsb.SetDepthEnable(true);
    // 半透明パーティクルの場合、深度書き込み(DepthWrite)をOFF
    dsb.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO); 
    preset.depthStencilDesc = dsb.GetDesc();


    // --------------------------------------------------------
    // 6. Shader Compile (★ファイル名をParticle用に)
    // --------------------------------------------------------
    bool vsResult = preset.shaderProgram.CompileVS(
        L"resources/Shader/Particle/Particle.VS.hlsl", // ★Particle用のVS
        dxcUtils, dxcCompiler, includeHandler
    );
    // コンパイルエラーチェック
    assert(vsResult && "Particle VS Compile Failed!");

    bool psResult = preset.shaderProgram.CompilePS(
        L"resources/Shader/Particle/Particle.PS.hlsl", // ★Particle用のPS
        dxcUtils, dxcCompiler, includeHandler
    );
    assert(psResult && "Particle PS Compile Failed!");

    return preset;
}

PSOPreset PSOPreset::CreateSkyboxPreset(
    ID3D12Device* device,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {

    PSOPreset preset;

    // 1. RootSignature
    RootSignatureBuilder rs;
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_VERTEX); // gTransformationMatrix (b0)
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);  // gTexture (t0)

    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rs.AddSampler(sampler, 0);

    preset.rootSignature = rs.Build(device);
    assert(preset.rootSignature && "Skybox RootSignature creation failed!");

    // 2. Input Layout
    preset.ilBuilder_.Add("POSITION", DXGI_FORMAT_R32G32B32A32_FLOAT);
    preset.inputLayoutDesc = preset.ilBuilder_.Build();

    // 3. Blend State
    BlendStateBuilder blendBuilder;
    blendBuilder.SetBlendMode(kBlendModeNormal);
    preset.blendDesc = blendBuilder.Build();

    // 4. Rasterizer State
    RasterizerStateBuilder rsb;
    rsb.SetCullMode(CullMode::None); // 背面を通すか全面表示
    preset.rasterizerDesc = rsb.Build();

    // 5. Depth Stencil State
    DepthStencilStateBuilder dsb;
    dsb.SetDepthEnable(true);
    dsb.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO); // Skybox自身はデプスに書き込まない
    
    preset.depthStencilDesc = dsb.GetDesc();
    // 最奥に描画するため Z=1.0 と比較し、等しいか手前なら通す(LESS_EQUAL)設定にする
    preset.depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // 6. Shader
    bool vsResult = preset.shaderProgram.CompileVS(
        L"resources/Shader/Skybox/Skybox.VS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    assert(vsResult && "Skybox VS Compile Failed!");

    bool psResult = preset.shaderProgram.CompilePS(
        L"resources/Shader/Skybox/Skybox.PS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    assert(psResult && "Skybox PS Compile Failed!");

    return preset;
}

PSOPreset PSOPreset::CreateObject2DPreset(
    ID3D12Device* device,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {

    PSOPreset preset;

    // ---------------------------
    // 1. RootSignature
    // ---------------------------
    RootSignatureBuilder rs;
    // 行列 (b0, VS)
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
    // マテリアル(b0, PS)
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
    // テクスチャ (t0, PS)
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);
    // サンプラー (s0)
    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rs.AddSampler(sampler, 0);

    preset.rootSignature = rs.Build(device);
    assert(preset.rootSignature && "Object2D RootSignature creation failed!");

    // ---------------------------
    // 2. InputLayout
    // ---------------------------
    preset.ilBuilder_.Add("POSITION", DXGI_FORMAT_R32G32B32A32_FLOAT);
    preset.ilBuilder_.Add("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    preset.ilBuilder_.Add("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
    preset.inputLayoutDesc = preset.ilBuilder_.Build();

    // ---------------------------
    // 3. Blend State
    // ---------------------------
    BlendStateBuilder blendBuilder;
    blendBuilder.SetBlendMode(kBlendModeNormal);
    preset.blendDesc = blendBuilder.Build();

    // ---------------------------
    // 4. Rasterizer State
    // ---------------------------
    RasterizerStateBuilder rsb;
    rsb.SetCullMode(CullMode::None); // 2Dなのでカリングなし
    preset.rasterizerDesc = rsb.Build();

    // ---------------------------
    // 5. Depth Stencil State
    // ---------------------------
    DepthStencilStateBuilder dsb;
    dsb.SetDepthEnable(false); // 深度テスト無効
    dsb.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO); // 深度書き込み無効
    preset.depthStencilDesc = dsb.GetDesc();

    // ---------------------------
    // 6. Shader
    // ---------------------------
    // VS
    bool vsResult = preset.shaderProgram.CompileVS(
        L"resources/Shader/2d/Object2d.VS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    assert(vsResult && "Object2D VS Compile Failed!");

    // PS
    bool psResult = preset.shaderProgram.CompilePS(
        L"resources/Shader/2d/Object2d.PS.hlsl",
        dxcUtils, dxcCompiler, includeHandler
    );
    assert(psResult && "Object2D PS Compile Failed!");

    return preset;
}


