#include "Engine/Graphics/Texture/Texture.h"
#include "d3dx12.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Base/Log/Log.h"
#include <cassert>
#include <filesystem>
#include <format>

namespace {
    // デフォルト白テクスチャパス定数（マジックストリング排除）
    constexpr const char* kDefaultWhiteTexturePath = "resources/Textures/white.png";
}

Texture::Texture() {}
Texture::~Texture() {
    Release();
}

void Texture::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
    ID3D12DescriptorHeap* srvHeap, uint32_t descriptorIndex,
    const std::string& filePath) {
    // 空パスまたはファイルが存在しない場合は安全な白画像にフォールバック
    if (filePath.empty() || !std::filesystem::exists(filePath)) {
        filePath_ = kDefaultWhiteTexturePath;
    } else {
        filePath_ = filePath;
    }

    // テクスチャ読み込み（フォールバック済みのパスを使用）
    DirectX::ScratchImage mipImages = LoadTextureFile(filePath_);
    metadata_ = mipImages.GetMetadata();

    // テクスチャリソース作成
    textureResource_ = CreateTextureResource(device, metadata_);
    intermediateResource_ = UploadTextureData(textureResource_.Get(), mipImages, device, commandList);

    // SRV設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata_.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (metadata_.IsCubemap()) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = UINT(metadata_.mipLevels);
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = UINT(metadata_.mipLevels);
    }

    // ディスクリプタ計算
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    textureSrvHandleCPU_ = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, descriptorIndex);
    textureSrvHandleGPU_ = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, descriptorIndex);

    // SRV作成
    device->CreateShaderResourceView(textureResource_.Get(), &srvDesc, textureSrvHandleCPU_);
}

void Texture::Update() {
    // 今後の動的テクスチャ更新処理に利用可能
}

void Texture::Release() {
    textureResource_.Reset();
    intermediateResource_.Reset();
    textureSrvHandleCPU_ = {};
    textureSrvHandleGPU_ = {};
}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetGpuHandle() const {
    if (textureSrvHandleGPU_.ptr == 0) {
        D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{};
        nullHandle.ptr = 0;
        return nullHandle;
    }
    return textureSrvHandleGPU_;
}

DirectX::ScratchImage Texture::LoadTextureFile(const std::string& filePath) {
    DirectX::ScratchImage image{};
    std::string targetPath = filePath;

    if (targetPath.empty() || !std::filesystem::exists(targetPath)) {
        targetPath = kDefaultWhiteTexturePath;
    }

    std::wstring filePathW = ConvertString(targetPath);

    HRESULT hr = E_FAIL;
    if (targetPath.ends_with(".dds")) {
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }

    // 読み込み失敗時は白画像にフォールバック（assertクラッシュ完全防止）
    if (FAILED(hr) && targetPath != kDefaultWhiteTexturePath) {
        Log::Write(std::format(L" ├─ [警告] テクスチャ読み込み失敗:「{}」。デフォルト白テクスチャにフォールバックします。", ConvertString(targetPath)));
        std::wstring fallbackW = ConvertString(kDefaultWhiteTexturePath);
        hr = DirectX::LoadFromWICFile(fallbackW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }

    // 万が一白画像すらロードできない場合は1x1の白テクスチャをメモリ上で生成
    if (FAILED(hr)) {
        image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1, 1, 1, 1);
        uint32_t* pixel = reinterpret_cast<uint32_t*>(image.GetPixels());
        if (pixel) {
            *pixel = 0xFFFFFFFF; // RGBA(255, 255, 255, 255)
        }
        return std::move(image);
    }

    DirectX::ScratchImage mipImages{};
    
    // 圧縮フォーマットの場合はミップマップ生成ができないため、そのまま返す
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        return std::move(image);
    }

    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipImages);

    return SUCCEEDED(hr) ? std::move(mipImages) : std::move(image);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Texture::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
    // 1.metadataを基にResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width); // Textureの幅
    resourceDesc.Height = UINT(metadata.height); // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // MipMapの数
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // Textureの深さ
    resourceDesc.Format = metadata.format; // Textureのフォーマット
    resourceDesc.SampleDesc.Count = 1; // マルチサンプリングの数。1はマルチサンプリングなし
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元
    // 2.利用するheapの設定。
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    // 3.Resourceの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr; // ResourceをComPtrで宣言
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // ヒープの設定
        D3D12_HEAP_FLAG_NONE, // ヒープのフラグ。特になし
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_COPY_DEST, // Resourceの初期状態。
        nullptr, // テクスチャの初期化情報, 使わないのでnullptr
        IID_PPV_ARGS(&resource)); // ComPtrの&演算子オーバーロードを利用
    // Resourceの生成に失敗したので起動できない
    assert(SUCCEEDED(hr)); // 失敗したらassertで止める
    return resource; // ComPtr<ID3D12Resource>を返す
}

Microsoft::WRL::ComPtr<ID3D12Resource> Texture::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
    uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = DxUtils::CreateBufferResource(device, intermediateSize);
    // UpdateSubresourcesには生のポインタを渡すため.Get()を使用
    UpdateSubresources(commandList, texture, intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
    // Textureへの転送後は利用できるように、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更すること
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // Resourceの状態を変更する
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE; // フラグは特になし
    barrier.Transition.pResource = texture; // Resourceのポインタ
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; // 全てのサブリソースを変更
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; // 変更前の状態
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ; // 変更後の状態
    commandList->ResourceBarrier(1, &barrier); // Resourceの状態を変更する
    return intermediateResource; // 転送用のResource(ComPtr)を返す
}
