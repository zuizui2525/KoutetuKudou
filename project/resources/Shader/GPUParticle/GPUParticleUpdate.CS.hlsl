// ===================================================================
// GPUParticleUpdate.CS.hlsl - GPUパーティクル並列シミュレーション Compute Shader
// ===================================================================
#include "GPUParticle.hlsli"

RWStructuredBuffer<GPUParticle> gParticles : register(u0);
ConstantBuffer<GPUEmitterConstant> gEmitter : register(b0);

// GPU向け高速ハッシュ型擬似乱数生成器 (PCG Hash)
uint Hash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// 0.0f 〜 1.0f の正規化浮動小数点乱数を取得
float RandFloat(uint seed, uint salt)
{
    uint h = Hash(seed ^ Hash(salt));
    return float(h & 0x00FFFFFFu) / float(0x01000000u);
}

// 3次元のランダムベクトル (各要素 0.0f 〜 1.0f)
float3 RandFloat3(uint seed, uint salt)
{
    return float3(
        RandFloat(seed, salt + 1u),
        RandFloat(seed, salt + 2u),
        RandFloat(seed, salt + 3u)
    );
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= gEmitter.maxParticles)
    {
        return;
    }

    GPUParticle p = gParticles[particleIndex];

    // -------------------------------------------------------------
    // 1. 今フレームでの新規発生 (Emit) 判定
    // -------------------------------------------------------------
    // リングバッファ方式: [emitOffset, emitOffset + emitCount) に該当するスロットを再生成
    bool shouldEmit = false;
    if (gEmitter.emitCount > 0u)
    {
        uint startIdx = gEmitter.emitOffset;
        uint endIdx = (gEmitter.emitOffset + gEmitter.emitCount);
        if (endIdx <= gEmitter.maxParticles)
        {
            shouldEmit = (particleIndex >= startIdx && particleIndex < endIdx);
        }
        else
        {
            // 最大値ラップアラウンド時
            shouldEmit = (particleIndex >= startIdx || particleIndex < (endIdx % gEmitter.maxParticles));
        }
    }

    if (shouldEmit)
    {
        uint seed = gEmitter.randomSeed + particleIndex * 17u;
        float rL = RandFloat(seed, 200u);
        float3 rPosOffset = (RandFloat3(seed, 300u) - 0.5f) * 0.3f;

        // --- 花火用 360度球面均等放射ベクトルの計算 ---
        float phi = RandFloat(seed, 101u) * 6.2831853f; // 0 〜 2π
        float cosTheta = RandFloat(seed, 102u) * 2.0f - 1.0f; // -1 〜 1
        float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        float3 sphereDir = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));

        float minSpeed = length(gEmitter.minVelocity);
        float maxSpeed = max(length(gEmitter.maxVelocity), minSpeed + 0.1f);
        float speed = lerp(minSpeed, maxSpeed, RandFloat(seed, 103u));

        // 花火のバーストカラー（シード値に応じてゴールド、シアン、マゼンタ、ルビー、ライムグリーン等に彩り分け）
        static const float4 kFireworkPalette[5] = {
            float4(1.0f, 0.75f, 0.2f, 1.0f),  // 眩い黄金 (Gold)
            float4(0.2f, 0.95f, 1.0f, 1.0f),  // ネオンシアン (Cyan)
            float4(1.0f, 0.25f, 0.6f, 1.0f),  // 鮮烈なマゼンタ (Magenta)
            float4(0.4f, 1.0f, 0.4f, 1.0f),   // エメラルドグリーン (Emerald)
            float4(0.9f, 0.4f, 1.0f, 1.0f)    // エレクトリックパープル (Purple)
        };
        uint paletteIdx = (gEmitter.randomSeed / 31u + particleIndex / 500u) % 5u;
        float4 baseColor = kFireworkPalette[paletteIdx];

        p.position = gEmitter.emitterPos + rPosOffset;
        p.velocity = sphereDir * speed;
        p.lifeTime = lerp(gEmitter.minLifeTime, gEmitter.maxLifeTime, rL);
        p.currentTime = 0.0f;
        p.scale = gEmitter.startScale;
        p.color = baseColor;
        p.isActive = 1u;
    }
    // -------------------------------------------------------------
    // 2. 生存中パーティクルのシミュレーション更新
    // -------------------------------------------------------------
    else if (p.isActive != 0u)
    {
        p.currentTime += gEmitter.deltaTime;

        // 寿命到達または落下高度判定 (killAtY)
        bool isDead = (p.currentTime >= p.lifeTime);
        if (gEmitter.killAtY > -900.0f && p.position.y <= gEmitter.killAtY)
        {
            isDead = true;
        }

        if (isDead)
        {
            p.isActive = 0u;
            p.scale = float3(0.0f, 0.0f, 0.0f);
        }
        else
        {
            // 空気抵抗による自然な減速（爆発直後は高速、次第にふわりと舞う）
            p.velocity *= (1.0f - saturate(1.6f * gEmitter.deltaTime));

            // 重力加速度の適用（下方向に優雅に垂れ落ちる）
            p.velocity.y -= gEmitter.gravity * gEmitter.deltaTime;

            // 位置更新
            p.position += p.velocity * gEmitter.deltaTime;

            // 寿命進行度 (0.0f 〜 1.0f)
            float t = saturate(p.currentTime / max(p.lifeTime, 0.0001f));

            // スケールおよび透明度フェード
            p.scale = lerp(gEmitter.startScale, gEmitter.endScale, t);
            p.color.a = (1.0f - t);

            // 寿命後半でのスパークル（チカチカきらめき）演出
            if (t > 0.45f)
            {
                float twinkle = sin(p.currentTime * 32.0f + float(particleIndex)) * 0.4f + 0.6f;
                p.color.rgb *= twinkle;
            }
        }
    }

    gParticles[particleIndex] = p;
}
