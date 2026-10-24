#include "Engine/Graphics/PSO/InputLayout/InputLayoutBuilder.h"
#include <cassert>

namespace {
    constexpr uint32_t kSize16Bytes = 16;
    constexpr uint32_t kSize12Bytes = 12;
    constexpr uint32_t kSize8Bytes = 8;
    constexpr uint32_t kSize4Bytes = 4;
    constexpr uint32_t kInitialOffset = 0;
    constexpr UINT kSemanticIndexZero = 0;
    constexpr UINT kInstanceDataStepRateZero = 0;
}

InputLayoutBuilder::InputLayoutBuilder()
    : offset_(kInitialOffset), currentSlot_(0) {
}

void InputLayoutBuilder::Add(const char* semanticName, DXGI_FORMAT format, uint32_t slot) {
    D3D12_INPUT_ELEMENT_DESC desc{};

    // 文字列自体は保存しておく
    semanticNameStorage_.push_back(semanticName);

    // ★重要: ここでは SemanticName には一旦 nullptr や一時的な値を入れておく
    // (ポインタの解決は Build で行うため)
    desc.SemanticName = nullptr;

    desc.SemanticIndex = kSemanticIndexZero;
    desc.Format = format;
    desc.InputSlot = slot;

    // スロットが切り替わった場合はオフセットを0にリセット
    if (slot != currentSlot_) {
        currentSlot_ = slot;
        offset_ = kInitialOffset;
    }

    desc.AlignedByteOffset = offset_;
    desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    desc.InstanceDataStepRate = kInstanceDataStepRateZero;

    // Formatのバイトサイズ推定
    switch (format) {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
        offset_ += kSize16Bytes;
        break;
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
        offset_ += kSize12Bytes;
        break;
    case DXGI_FORMAT_R32G32_FLOAT:
        offset_ += kSize8Bytes;
        break;
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
        offset_ += kSize4Bytes;
        break;
    default:
        assert(false && "Unsupported DXGI_FORMAT in InputLayoutBuilder!");
    }

    elements_.push_back(desc);
}

const D3D12_INPUT_LAYOUT_DESC& InputLayoutBuilder::Build() {
    // ★重要: ここでポインタを紐付け直す
    // elements_ と semanticNameStorage_ の要素数は必ず一致しているはず
    for (size_t i = 0; i < elements_.size(); ++i) {
        elements_[i].SemanticName = semanticNameStorage_[i].c_str();
    }

    layoutDesc_.pInputElementDescs = elements_.data();
    layoutDesc_.NumElements = (UINT)elements_.size();

    return layoutDesc_;
}
