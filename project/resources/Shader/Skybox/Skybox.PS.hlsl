#include "Skybox.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 2Dテクスチャ（white.pngなど）をサンプリングし、頂点カラー（マテリアルカラー）と乗算
    float32_t4 textureColor = gTexture.Sample(gSampler, float2(0.5f, 0.5f));
    output.color = textureColor * input.color;
    
    return output;
}
