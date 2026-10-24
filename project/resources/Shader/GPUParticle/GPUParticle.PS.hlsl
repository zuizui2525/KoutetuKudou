// ===================================================================
// GPUParticle.PS.hlsl - GPUパーティクル用ピクセルシェーダー
// ===================================================================

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

struct PixelOutput
{
    float4 color : SV_TARGET0;
};

PixelOutput main(VertexOutput input)
{
    PixelOutput output;

    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = texColor * input.color;

    // アルファが極小ならディスカード
    if (output.color.a <= 0.001f)
    {
        discard;
    }

    return output;
}
