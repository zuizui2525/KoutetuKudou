// ===================================================================
// GPUParticle.VS.hlsl - GPUパーティクル用頂点シェーダー (頂点バッファレス・自動ビルボード展開)
// ===================================================================
#include "GPUParticle.hlsli"

struct CameraConstant
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3   cameraPos;
};

ConstantBuffer<CameraConstant> gCamera : register(b0);
StructuredBuffer<GPUParticle> gParticles : register(t0);

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

// 6頂点（2枚の三角形）によるクアッド展開用オフセットテーブル
static const float2 kQuadOffsets[6] =
{
    float2(-0.5f,  0.5f), // 左上
    float2( 0.5f,  0.5f), // 右上
    float2(-0.5f, -0.5f), // 左下
    float2(-0.5f, -0.5f), // 左下
    float2( 0.5f,  0.5f), // 右上
    float2( 0.5f, -0.5f)  // 右下
};

static const float2 kQuadUVs[6] =
{
    float2(0.0f, 0.0f), // 左上
    float2(1.0f, 0.0f), // 右上
    float2(0.0f, 1.0f), // 左下
    float2(0.0f, 1.0f), // 左下
    float2(1.0f, 0.0f), // 右上
    float2(1.0f, 1.0f)  // 右下
};

VertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VertexOutput output;

    GPUParticle p = gParticles[instanceId];

    // 非アクティブ（消滅済み）パーティクルは画面外へ縮退させてGPUカリング
    if (p.isActive == 0u || p.scale.x <= 0.0001f)
    {
        output.position = float4(0.0f, 0.0f, -2.0f, 0.0f); // クリップ領域外
        output.texcoord = float2(0.0f, 0.0f);
        output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }

    // カメラの右・上方向ベクトルをビュー行列から抽出（完全球体ビルボード）
    float3 camRight = float3(gCamera.view[0][0], gCamera.view[1][0], gCamera.view[2][0]);
    float3 camUp    = float3(gCamera.view[0][1], gCamera.view[1][1], gCamera.view[2][1]);

    float2 offset = kQuadOffsets[vertexId];
    float2 uv     = kQuadUVs[vertexId];

    // スケールを反映したビルボード四角形頂点
    float3 worldPos = p.position + (camRight * offset.x * p.scale.x) + (camUp * offset.y * p.scale.y);

    output.position = mul(float4(worldPos, 1.0f), gCamera.viewProjection);
    output.texcoord = uv;
    output.color = p.color;

    return output;
}
