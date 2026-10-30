#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <cstdint>
#include <random>

#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/PSO/Compute/ComputePipeline.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleSetting.h"

/// <summary>
/// GPUパーティクルのエミッターおよびシミュレーションパラメータ設定構造体
/// </summary>
struct GPUParticleEmitterSetting {
    Vector3 emitterPosition = { 0.0f, 0.0f, 0.0f };   // エミッター中心座標
    Vector3 startScale      = { 0.35f, 0.35f, 0.35f }; // 発生時スケール
    Vector3 endScale        = { 0.05f, 0.05f, 0.05f }; // 消滅時スケール

    Vector4 startColor      = { 1.0f, 0.6f, 0.2f, 1.0f }; // 発生時カラー (鮮やかなオレンジ)
    Vector4 endColor        = { 1.0f, 0.1f, 0.0f, 0.0f }; // 消滅時カラー (フェードアウト)

    Vector3 minVelocity     = { -2.5f, 1.0f, -2.5f };  // 最小初速
    Vector3 maxVelocity     = {  2.5f, 6.0f,  2.5f };  // 最大初速

    float minLifeTime       = 0.8f;   // 最小寿命 (秒)
    float maxLifeTime       = 2.0f;   // 最大寿命 (秒)

    float gravity           = 3.5f;   // 重力加速度
    float killAtY           = -999.0f;// 落下消滅高度 (-900.0f以下で無効)

    float spawnRatePerSec   = 0.0f;   // 1秒あたりの自動発生パーティクル数（初期状態は停止）
    bool  isAutoSpawning    = false;  // 自動連続発生を行うか（コンポーネント等からの明示設定までOFF）
};

/// <summary>
/// GPUパーティクルのブレンドモード列挙型
/// </summary>
enum class ParticleBlendMode {
    Normal = 0,    // 通常半透明合成 (SrcAlpha + InvSrcAlpha)
    Add = 1,       // 加算合成 (SrcAlpha + One) - 鮮やかな発光
    Subtract = 2,  // 減算合成 (Dest - Src) - 暗黒・煙
    Multiply = 3,  // 乗算合成 (DestColor * SrcColor) - 影・汚れ
    Count = 4
};

/// <summary>
/// DirectX 12 Compute Shader による完全GPU駆動パーティクルオブジェクト
/// 数万〜数十万個のパーティクルをCPU負荷ほぼゼロで超高速並列シミュレーション・描画します。
/// </summary>
class GPUParticleObject {
public:
    // マジックナンバー排除のための定数
    static constexpr uint32_t kDefaultMaxParticles = 65536; // 標準最大数 (6.5万個)
    static constexpr uint32_t kComputeThreadGroupSize = 256;// CSスレッドグループサイズ (256スレッド/グループ)
    static constexpr uint32_t kQuadVertexCount = 6;         // 四角形ビルボード1枚の頂点数
    static constexpr float kDefaultDeltaTime = 1.0f / 60.0f;
    static constexpr float kGravityMin = -20.0f;
    static constexpr float kGravityMax = 20.0f;

    GPUParticleObject();
    ~GPUParticleObject();

    /// <summary>
    /// GPUパーティクルシステムのリソース、UAV、SRV、Compute/Renderパイプラインを初期化します。
    /// </summary>
    /// <param name="maxParticles">シミュレーションする最大パーティクル数</param>
    /// <returns>初期化成功時 true</returns>
    bool Initialize(uint32_t maxParticles = kDefaultMaxParticles);

    /// <summary>
    /// エミッターパラメータの更新および定数バッファの同期を行います。
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過時間 (秒)</param>
    void Update(float deltaTime = kDefaultDeltaTime);

    /// <summary>
    /// Compute Shader によるパーティクルの並列シミュレーションをディスパッチします。
    /// </summary>
    /// <param name="cmdList">DirectX 12 コマンドリスト</param>
    void DispatchCompute(ID3D12GraphicsCommandList* cmdList);

    /// <summary>
    /// シミュレーションされたGPUパーティクルをビルボードとして高速インスタンシング描画します。
    /// </summary>
    /// <param name="cmdList">DirectX 12 コマンドリスト</param>
    /// <param name="textureKey">貼り付けるテクスチャ名 (例: "white", "circle")</param>
    /// <param name="blendMode">ブレンドモード (Normal / Add / Subtract / Multiply)</param>
    void Draw(ID3D12GraphicsCommandList* cmdList, const std::string& textureKey = "white", ParticleBlendMode blendMode = ParticleBlendMode::Add);

    /// <summary>
    /// エンジン終了時に全静的パイプラインリソースを安全に解放
    /// </summary>
    static void DestroyPipelines();

    ParticleBlendMode GetBlendMode() const { return blendMode_; }
    void SetBlendMode(ParticleBlendMode mode) { blendMode_ = mode; }

    /// <summary>
    /// ImGui によるリアルタイムパラメータ調整ウィンドウを描画します。
    /// </summary>
    /// <param name="label">ウィンドウ表示ラベル</param>
    void ImGuiControl(const std::string& label = "GPU Particle");

    /// <summary>
    /// 指定した個数のパーティクルを即座に一括発生（バースト）させます。
    /// </summary>
    /// <param name="count">発生個数</param>
    void EmitBurst(uint32_t count);

    // Getter / Setter
    GPUParticleEmitterSetting& GetSetting() { return setting_; }
    const GPUParticleEmitterSetting& GetSetting() const { return setting_; }
    void SetPosition(const Vector3& pos) { setting_.emitterPosition = pos; }
    uint32_t GetMaxParticles() const { return maxParticles_; }

    /// <summary>
    /// 現在の発生レートと平均寿命から推定されるアクティブ粒子数を取得
    /// </summary>
    uint32_t GetEstimatedActiveCount() const {
        if (!setting_.isAutoSpawning || setting_.spawnRatePerSec <= 0.0f) {
            return 0;
        }
        constexpr float kHalfFactor = 0.5f;
        float avgLife = (setting_.minLifeTime + setting_.maxLifeTime) * kHalfFactor;
        auto estimated = static_cast<uint32_t>(setting_.spawnRatePerSec * avgLife);
        return (std::min)(estimated, maxParticles_);
    }

    const std::string& GetTextureKey() const { return currentTextureKey_; }
    void SetTextureKey(const std::string& key) { currentTextureKey_ = key; }

    /// <summary>
    /// スタジオエディター用の GPUParticleSetting を適用
    /// </summary>
    void ApplySetting(const GPUParticleSetting& s);

    /// <summary>
    /// 現在のパラメータを GPUParticleSetting として取得
    /// </summary>
    GPUParticleSetting ToParticleSetting() const;

    /// <summary>
    /// スタジオプレビュー等で専用カメラ行列を適用
    /// </summary>
    void SetCustomCamera(const Matrix4x4& view, const Matrix4x4& proj, const Vector3& cameraPos);
    void ClearCustomCamera() { useCustomCamera_ = false; }

    /// <summary>
    /// パーティクルシミュレーションのリセット（全粒子消去＆オフセット初期化）
    /// </summary>
    void Reset();

    uint32_t GetRandomSeed() const { return randomSeedCounter_; }
    void SetRandomSeed(uint32_t seed) { randomSeedCounter_ = seed; }

    /// <summary>
    /// 文字花火用の形状点群を設定
    /// </summary>
    void SetShapePoints(const std::vector<Vector2>& points);
    void SetShapeType(uint32_t type) { shapeType_ = type; }
    uint32_t GetShapeType() const { return shapeType_; }
    void SetBurstSpeed(float speed) { burstSpeed_ = speed; }
    float GetBurstSpeed() const { return burstSpeed_; }

private:
    // 内部初期化ヘルパー
    bool CreatePipelines(ID3D12Device* device);
    bool CreateBuffersAndDescriptors(ID3D12Device* device);

    // リソース状態遷移ヘルパー
    void TransitionBuffer(
        ID3D12GraphicsCommandList* cmdList,
        D3D12_RESOURCE_STATES targetState);

private:
    uint32_t maxParticles_ = kDefaultMaxParticles;
    GPUParticleEmitterSetting setting_;
    std::string name_ = "Default";
    std::string currentTextureKey_ = "white";

    // カスタムカメラ設定
    bool useCustomCamera_ = false;
    Matrix4x4 customViewMatrix_{};
    Matrix4x4 customProjMatrix_{};
    Vector3 customCameraPos_{};

    // シミュレーション状態管理
    uint32_t currentEmitOffset_ = 0;
    uint32_t emitCountThisFrame_ = 0;
    float spawnAccumulator_ = 0.0f;
    uint32_t randomSeedCounter_ = 0;
    bool isInitialized_ = false;
    bool requestClearAll_ = false;

    // DirectX 12 リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;         // Defaultヒープ: RWStructuredBuffer
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterConstantBuffer_;   // Uploadヒープ: エミッターパラメータCBV
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraConstantBuffer_;    // Uploadヒープ: カメラCBV

    ParticleBlendMode blendMode_ = ParticleBlendMode::Add;

    // 全インスタンス共通の静的パイプライン（シェーダー再コンパイルをゼロにしてFPSを維持）
    static inline bool sPipelinesInitialized_ = false;
    static inline ComputePipeline sComputePipeline_;
    static inline Microsoft::WRL::ComPtr<ID3D12RootSignature> sRenderRootSignature_;
    static inline Microsoft::WRL::ComPtr<ID3D12PipelineState> sRenderPipelineStates_[static_cast<size_t>(ParticleBlendMode::Count)];

    // デスクリプタインデックス管理 (グローバルSRVヒープ内)
    UINT uavDescriptorIndex_ = 0;
    UINT srvDescriptorIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE uavCpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};

    // リソース状態追跡
    D3D12_RESOURCE_STATES currentBufferState_ = D3D12_RESOURCE_STATE_COMMON;

    // 文字花火用定数バッファと設定
    Microsoft::WRL::ComPtr<ID3D12Resource> textShapeConstantBuffer_;
    uint32_t shapeType_ = 0;
    uint32_t shapePointCount_ = 0;
    float burstSpeed_ = 0.0f;
    static constexpr uint32_t kMaxShapeConstantPoints = 1024;
};
