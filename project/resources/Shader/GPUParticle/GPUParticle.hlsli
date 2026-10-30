// ===================================================================
// GPUParticle.hlsli - GPUパーティクル共通構造体・定数定義
// ===================================================================

/// <summary>
/// GPU上で1つのパーティクルが保持するデータ構造体 (64バイト・16バイト境界アライメント)
/// </summary>
struct GPUParticle
{
    float3 position;    // 3D座標 (x, y, z)
    float  lifeTime;    // パーティクルの総寿命(秒)
    
    float3 velocity;    // 速度ベクトル (vx, vy, vz)
    float  currentTime; // 発生からの経過秒数
    
    float3 scale;       // 現在のスケール
    uint   isActive;    // 生存フラグ (1: 生存, 0: 消滅/非アクティブ)
    
    float4 color;       // 現在の色 (RGBA)
};

/// <summary>
/// エミッターパラメータ定数バッファ (CPU -> Compute Shader)
/// </summary>
struct GPUEmitterConstant
{
    float3 emitterPos;         // エミッターの中心ワールド座標
    float  deltaTime;          // 前フレームからの経過時間

    float3 startScale;         // 発生時のスケール
    float  gravity;            // 重力加速度 (Y軸マイナス方向)

    float3 endScale;           // 消滅時のスケール
    float  killAtY;            // 一定の高さ以下で消滅 (-999.0f以下で無効)

    float4 startColor;         // 発生時のカラー (RGBA)
    float4 endColor;           // 消滅時のカラー (RGBA)

    float3 minVelocity;        // 最小初速
    float  minLifeTime;        // 最小寿命

    float3 maxVelocity;        // 最大初速
    float  maxLifeTime;        // 最大寿命

    uint   maxParticles;       // 最大パーティクル数 (例: 65536)
    uint   emitOffset;         // 今フレーム発生させる先頭インデックス
    uint   emitCount;          // 今フレーム新規発生させる個数
    uint   randomSeed;         // 乱数シード (フレーム番号やタイマー値)

    uint   shapeType;          // 0: 通常球状放射, 1: 文字花火 (Text Firework)
    uint   shapePointCount;    // 文字点群数
    float  burstSpeed;         // 拡散初速 / 文字サイズスケール
    float  padding0;

    float3 emitterRight;       // 文字の横軸（カメラ右方向）
    float  padding1;

    float3 emitterUp;          // 文字の縦軸（カメラ上方向）
    float  padding2;
};

/// <summary>
/// 文字花火用ドット点群バッファ (最大1024点)
/// </summary>
cbuffer TextShapeBuffer : register(b1)
{
    float4 gShapePoints[1024]; // xy: 正規化オフセット (-1.0f 〜 1.0f)
};
