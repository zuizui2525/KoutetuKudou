#include "BaseParticleObject.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include <imgui.h>
#include <queue>

namespace {
    void InitializeTransform(Transform& transform) {
        transform.scale = { 1.0f, 1.0f, 1.0f };
        transform.rotate = { 0.0f, 0.0f, 0.0f };
        transform.translate = { 0.0f, 0.0f, 0.0f };
    }

    // SRVインデックス再利用のための静的キューとインデックス追跡変数
    std::queue<UINT> sAvailableSrvIndices;
    UINT sNextIndex = 50;                     // インデックス開始値
    constexpr UINT kMaxSrvIndex = 127;        // DescriptorHeapサイズ限界 (128未満)
}

/**
 * @brief デストラクタ
 * パーティクルオブジェクト消滅時に、使用していたSRVインデックスをプールに戻して再利用可能にします。
 */
BaseParticleObject::~BaseParticleObject() {
    // 破棄される際に使用していたインデックスをキューに戻す
    static constexpr UINT kMinValidSrvIndex = 50;
    if (mySrvIndex_ >= kMinValidSrvIndex) {
        sAvailableSrvIndices.push(mySrvIndex_);
    }
}

/**
 * @brief パーティクルオブジェクトの初期化処理
 */
void BaseParticleObject::Initialize(int lightingMode) {
    ID3D12Device* device = sEngine->GetDevice();

    InitializeTransform(emitter_.transform);
    emitter_.count = 10;
    emitter_.frequency = 0.5f;
    emitter_.frequencyTime = 0.0f;

    materialResource_ = DxUtils::CreateBufferResource(device, sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = lightingMode;
    materialData_->uvtransform = Math::MakeIdentity();

    randomEngine_ = std::mt19937(seedGenerator_());

    // SRVインデックスの割り当て（プールに空きがあれば再利用し、なければ新規インクリメント）
    if (!sAvailableSrvIndices.empty()) {
        mySrvIndex_ = sAvailableSrvIndices.front();
        sAvailableSrvIndices.pop();
    } else {
        // DescriptorHeapの限界（128未満）に達していないことを確認するアサート
        assert(sNextIndex <= kMaxSrvIndex && "SRV DescriptorHeap index out of bounds! Too many particle objects.");
        mySrvIndex_ = sNextIndex++;
    }
    
    numMaxInstance_ = setting_.maxParticles;
    particles_.reserve(numMaxInstance_);

    CreateInstanceResource();
}

void BaseParticleObject::CreateInstanceResource() {
    ID3D12Device* device = sEngine->GetDevice();
    ID3D12DescriptorHeap* srvHeap = sEngine->GetDxCommon()->GetSrvHeap();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    size_t bufferSize = sizeof(ParticleForGPU) * numMaxInstance_;
    instanceResource_ = DxUtils::CreateBufferResource(device, bufferSize);
    instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

    instanceSrvHandleCPU_ = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, mySrvIndex_);
    instanceSrvHandleGPU_ = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, mySrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    instancingSrvDesc.Buffer.FirstElement = 0;
    instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    instancingSrvDesc.Buffer.NumElements = numMaxInstance_;
    instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    device->CreateShaderResourceView(instanceResource_.Get(), &instancingSrvDesc, instanceSrvHandleCPU_);
}

void BaseParticleObject::Update() {
    numInstance_ = 0;

    // エミッター更新
    if (setting_.isEmitter) {
        emitter_.frequencyTime += kDeltaTime_;
        if (setting_.emitFrequency <= emitter_.frequencyTime) {
            std::vector<Particle> newParticles = Emit(emitter_, randomEngine_);
            size_t currentSize = particles_.size();
            if (currentSize < numMaxInstance_) {
                size_t space = numMaxInstance_ - currentSize;
                if (newParticles.size() > space) {
                    newParticles.resize(space);
                }
                particles_.insert(particles_.end(), std::make_move_iterator(newParticles.begin()), std::make_move_iterator(newParticles.end()));
            }
            emitter_.frequencyTime -= setting_.emitFrequency; // 以前の引き算方式に戻す
        }
    }

    auto cameraMgr = CameraResource::GetCameraManager();
    Matrix4x4 managerWorldMatrix = Math::MakeAffineMatrix(emitter_.transform.scale, emitter_.transform.rotate, emitter_.transform.translate);

    Matrix4x4 billBoardMatrix = Math::MakeIdentity();
    if (setting_.isBillboard) {
        billBoardMatrix = Math::Inverse(cameraMgr->GetViewMatrix3D());
        billBoardMatrix.m[3][0] = 0.0f;
        billBoardMatrix.m[3][1] = 0.0f;
        billBoardMatrix.m[3][2] = 0.0f;
    }

    // ビュープロジェクション行列をループ前に事前計算してオーバーヘッドを完全に排除
    Matrix4x4 viewProj = Math::Multiply(cameraMgr->GetViewMatrix3D(), cameraMgr->GetProjectionMatrix3D());

    for (size_t i = 0; i < particles_.size(); ) {
        auto& p = particles_[i];

        // 寿命判定、または地面到達（killAtY）判定
        bool isDead = (p.currentTime >= p.lifeTime);
        if (setting_.killAtY > -900.0f && p.transform.translate.y <= setting_.killAtY) {
            isDead = true;
        }

        if (isDead) {
            // 消滅する瞬間に onDeathEffectName が設定されていれば発生させる
            if (!setting_.onDeathEffectName.empty()) {
                EffectPlayParam param;
                param.position = p.transform.translate;
                param.colorOverride = p.inheritColor; // 親の色を引き継ぐ

                // カンマ区切りで複数のエフェクトを発生させる
                std::string names = setting_.onDeathEffectName;
                size_t pos = 0;
                while ((pos = names.find(",")) != std::string::npos) {
                    std::string token = names.substr(0, pos);
                    EffectManager::GetInstance()->PlayEffect3D(token, param);
                    names.erase(0, pos + 1);
                }
                EffectManager::GetInstance()->PlayEffect3D(names, param);
            }
            
            // ベクトルの高速削除（末尾の要素と入れ替えて pop_back）
            if (i != particles_.size() - 1) {
                particles_[i] = std::move(particles_.back());
            }
            particles_.pop_back();
            continue; // インデックスは進めずに次の要素を処理
        }

        // 遅延処理：設定されたディレイ時間を過ぎるまで更新・描画をスキップする
        if (p.currentTime < setting_.delay) {
            p.currentTime += kDeltaTime_;
            i++;
            continue;
        }

        // トレイル（移動中の連鎖エフェクト）の発生処理
        if (!setting_.trailEffectName.empty()) {
            p.trailFrequencyTimer += kDeltaTime_;
            if (p.trailFrequencyTimer >= setting_.trailFrequency) {
                EffectPlayParam param;
                param.position = p.transform.translate;
                param.colorOverride = p.inheritColor; // 親の色を引き継ぐ
                EffectManager::GetInstance()->PlayEffect3D(setting_.trailEffectName, param);
                p.trailFrequencyTimer -= setting_.trailFrequency;
            }
        }

        Vector3 currentScale = p.transform.scale;
        Vector3 currentRotate = p.transform.rotate;

        if (setting_.isVelocityAligned) {
            float speed = std::sqrt(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y + p.velocity.z * p.velocity.z);
            if (speed > 0.001f) {
                Vector3 dir = { p.velocity.x / speed, p.velocity.y / speed, p.velocity.z / speed };
                currentRotate.y = std::atan2(dir.x, dir.z);
                currentRotate.x = std::asin(-dir.y);
                currentRotate.z = 0.0f;
                // 進行方向（Z軸）をスピードに比例して引き伸ばす
                currentScale.z = speed * setting_.velocityAlignmentScale; 
                // Y軸・X軸は細くする
                currentScale.x *= 0.2f;
                currentScale.y *= 0.2f;
            }
        }

        Matrix4x4 particleWorldMatrix;
        if (setting_.isBillboard) {
            Matrix4x4 scaleMatrix = Math::MakeScaleMatrix(currentScale);
            Matrix4x4 rotateMatrix = Math::MakeRotateMatrix(currentRotate.x, currentRotate.y, currentRotate.z);
            Matrix4x4 translateMatrix = Math::MakeTranslateMatrix(p.transform.translate);
            
            particleWorldMatrix = Math::Multiply(scaleMatrix, rotateMatrix);
            particleWorldMatrix = Math::Multiply(particleWorldMatrix, billBoardMatrix);
            particleWorldMatrix = Math::Multiply(particleWorldMatrix, translateMatrix);
        } else {
            particleWorldMatrix = Math::MakeAffineMatrix(currentScale, currentRotate, p.transform.translate);
        }

        particleWorldMatrix = Math::Multiply(particleWorldMatrix, managerWorldMatrix);

        Matrix4x4 worldViewProjection = Math::Multiply(particleWorldMatrix, viewProj);

        // 加速度（重力など）を速度に加算
        p.velocity.x += setting_.acceleration.x * kDeltaTime_;
        p.velocity.y += setting_.acceleration.y * kDeltaTime_;
        p.velocity.z += setting_.acceleration.z * kDeltaTime_;

        // 速度・回転速度を座標に加算
        p.transform.translate += p.velocity * kDeltaTime_;
        p.transform.rotate += p.rotationVelocity * kDeltaTime_;
        p.currentTime += kDeltaTime_;

        float progress = (std::min)(p.currentTime / p.lifeTime, 1.0f);
        
        float scaleProgress = progress;
        if (setting_.useEaseInScale) scaleProgress = progress * progress;
        else if (setting_.useEaseOutScale) scaleProgress = 1.0f - (1.0f - progress) * (1.0f - progress);

        float alphaProgress = progress;
        if (setting_.useEaseInAlpha) alphaProgress = progress * progress;
        else if (setting_.useEaseOutAlpha) alphaProgress = 1.0f - (1.0f - progress) * (1.0f - progress);

        p.transform.scale = p.startScale + (p.endScale - p.startScale) * scaleProgress;
        
        // Y軸にサイン波を適用する場合 (0 -> 1 -> 0)
        if (setting_.useSineScaleY) {
            float sineProgress = std::sin(progress * 3.14159265f);
            p.transform.scale.y = p.startScale.y + (p.endScale.y - p.startScale.y) * sineProgress;
        }

        // XZ軸にサイン波を適用する場合 (0 -> 1 -> 0)
        if (setting_.useSineScaleXZ) {
            float sineProgress = std::sin(progress * 3.14159265f);
            p.transform.scale.x = p.startScale.x + (p.endScale.x - p.startScale.x) * sineProgress;
            p.transform.scale.z = p.startScale.z + (p.endScale.z - p.startScale.z) * sineProgress;
        }
        
        p.color.x = p.startColor.x + (p.endColor.x - p.startColor.x) * progress;
        p.color.y = p.startColor.y + (p.endColor.y - p.startColor.y) * progress;
        p.color.z = p.startColor.z + (p.endColor.z - p.startColor.z) * progress;
        p.color.w = p.startColor.w + (p.endColor.w - p.startColor.w) * alphaProgress;

        if (numInstance_ < numMaxInstance_) {
            instanceData_[numInstance_].world = particleWorldMatrix;
            instanceData_[numInstance_].WVP = worldViewProjection;
            instanceData_[numInstance_].color = p.color;
            numInstance_++;
        }
        i++;
    }
}

void BaseParticleObject::EmitAt(uint32_t count, const EffectPlayParam& param) {
    if (!param.textureKey.empty()) {
        setting_.textureName = param.textureKey;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (particles_.size() < numMaxInstance_) {
            particles_.push_back(MakeNewParticle(randomEngine_, param));
        }
    }
}

Particle BaseParticleObject::MakeNewParticle(std::mt19937& randomEngine, const EffectPlayParam& param) {
    Particle particle;
    
    std::uniform_real_distribution<float> distPosX(setting_.spawnAreaMin.x, setting_.spawnAreaMax.x);
    std::uniform_real_distribution<float> distPosY(setting_.spawnAreaMin.y, setting_.spawnAreaMax.y);
    std::uniform_real_distribution<float> distPosZ(setting_.spawnAreaMin.z, setting_.spawnAreaMax.z);
    
    particle.transform.translate = param.position + setting_.positionOffset + 
                                   Vector3{ distPosX(randomEngine), distPosY(randomEngine), distPosZ(randomEngine) };

    auto getRandVec3 = [&](const Vector3& min, const Vector3& max) -> Vector3 {
        return {
            std::uniform_real_distribution<float>((std::min)(min.x, max.x), (std::max)(min.x, max.x))(randomEngine),
            std::uniform_real_distribution<float>((std::min)(min.y, max.y), (std::max)(min.y, max.y))(randomEngine),
            std::uniform_real_distribution<float>((std::min)(min.z, max.z), (std::max)(min.z, max.z))(randomEngine)
        };
    };

    particle.startScale = getRandVec3(setting_.scaleMin, setting_.scaleMax);
    if (setting_.isUniformScaleXZ) {
        particle.startScale.z = particle.startScale.x;
    }
    particle.startScale.x *= param.scale.x;
    particle.startScale.y *= param.scale.y;
    particle.startScale.z *= param.scale.z;

    // 未設定（負の値）なら開始時と同じにする
    if (setting_.scaleEndMin.x < 0.0f) {
        particle.endScale = particle.startScale;
    } else {
        particle.endScale = getRandVec3(setting_.scaleEndMin, setting_.scaleEndMax);
        if (setting_.isUniformScaleXZ) {
            particle.endScale.z = particle.endScale.x;
        }
        particle.endScale.x *= param.scale.x;
        particle.endScale.y *= param.scale.y;
        particle.endScale.z *= param.scale.z;
    }
    
    particle.transform.scale = particle.startScale;
    particle.transform.rotate = getRandVec3(setting_.rotationMin, setting_.rotationMax);
    particle.transform.rotate.x += param.rotation.x;
    particle.transform.rotate.y += param.rotation.y;
    particle.transform.rotate.z += param.rotation.z;
    
    particle.rotationVelocity = getRandVec3(setting_.rotationVelocityMin, setting_.rotationVelocityMax);
    
    Vector3 baseVel;
    if (setting_.isSpherical) {
        // 球状拡散：velocityMin.x を最小スピード、velocityMax.x を最大スピードとする
        float speedMin = setting_.velocityMin.x;
        float speedMax = setting_.velocityMax.x;
        float speed = std::uniform_real_distribution<float>((std::min)(speedMin, speedMax), (std::max)(speedMin, speedMax))(randomEngine);
        
        // 極座標系でランダムな方向ベクトルを生成
        float theta = std::uniform_real_distribution<float>(0.0f, 3.14159265f * 2.0f)(randomEngine); // 方位角 (0〜2PI)
        float phi = std::acos(std::uniform_real_distribution<float>(-1.0f, 1.0f)(randomEngine));     // 仰角 (-1〜1の逆余弦で一様な全球分布)
        
        Vector3 direction;
        direction.x = std::sin(phi) * std::cos(theta);
        direction.y = std::sin(phi) * std::sin(theta);
        direction.z = std::cos(phi);
        
        baseVel.x = direction.x * speed;
        baseVel.y = direction.y * speed;
        baseVel.z = direction.z * speed;
    } else {
        baseVel = getRandVec3(setting_.velocityMin, setting_.velocityMax);
    }
    
    particle.velocity = baseVel + param.velocityOverride;

    auto getRandVec4 = [&](const Vector4& min, const Vector4& max) -> Vector4 {
        return {
            std::uniform_real_distribution<float>((std::min)(min.x, max.x), (std::max)(min.x, max.x))(randomEngine),
            std::uniform_real_distribution<float>((std::min)(min.y, max.y), (std::max)(min.y, max.y))(randomEngine),
            std::uniform_real_distribution<float>((std::min)(min.z, max.z), (std::max)(min.z, max.z))(randomEngine),
            std::uniform_real_distribution<float>((std::min)(min.w, max.w), (std::max)(min.w, max.w))(randomEngine)
        };
    };

    particle.startColor = getRandVec4(setting_.colorStartMin, setting_.colorStartMax);
    // 未設定（負の値）なら、RGBは維持してアルファだけ0（フェードアウト）にする
    if (setting_.colorEndMin.x < 0.0f) {
        particle.endColor = particle.startColor;
        particle.endColor.w = 0.0f; 
    } else {
        particle.endColor = getRandVec4(setting_.colorEndMin, setting_.colorEndMax);
    }

    // --- 外からの色上書きを適用 ---
    if (param.colorOverride.w > 0.0f) {
        particle.startColor = param.colorOverride;
        // 終了時の色は、指定された色のアルファだけを0にしたものにする（フェードアウト維持）
        particle.endColor = param.colorOverride;
        particle.endColor.w = 0.0f;
    }

    particle.color = particle.startColor;

    particle.inheritColor = param.colorOverride; // 色を保存しておく
    particle.lifeTime = std::uniform_real_distribution<float>((std::min)(setting_.lifeTimeMin, setting_.lifeTimeMax), (std::max)(setting_.lifeTimeMin, setting_.lifeTimeMax))(randomEngine);
    particle.currentTime = 0.0f;

    return particle;
}

std::vector<Particle> BaseParticleObject::Emit(const Emitter& emitter, std::mt19937& randomEngine) {
    std::vector<Particle> particles;
    EffectPlayParam param;
    param.position = emitter.transform.translate;
    param.rotation = emitter.transform.rotate;
    param.scale = emitter.transform.scale;
    uint32_t count = std::uniform_int_distribution<uint32_t>(setting_.emitCountMin, setting_.emitCountMax)(randomEngine);
    particles.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        particles.push_back(MakeNewParticle(randomEngine, param));
    }
    return particles;
}

void BaseParticleObject::ImGuiControl(const std::string& name) {
    // 省略 (必要に応じて後で実装)
}

void BaseParticleObject::UpdateMatrices() {
    numInstance_ = 0;

    auto cameraMgr = CameraResource::GetCameraManager();
    Matrix4x4 managerWorldMatrix = Math::MakeAffineMatrix(emitter_.transform.scale, emitter_.transform.rotate, emitter_.transform.translate);

    Matrix4x4 billBoardMatrix = Math::MakeIdentity();
    if (setting_.isBillboard) {
        billBoardMatrix = Math::Inverse(cameraMgr->GetViewMatrix3D());
        billBoardMatrix.m[3][0] = 0.0f;
        billBoardMatrix.m[3][1] = 0.0f;
        billBoardMatrix.m[3][2] = 0.0f;
    }

    // ビュープロジェクション行列をループ前に事前計算
    Matrix4x4 viewProj = Math::Multiply(cameraMgr->GetViewMatrix3D(), cameraMgr->GetProjectionMatrix3D());

    for (size_t i = 0; i < particles_.size(); ++i) {
        auto& p = particles_[i];

        Vector3 currentScale = p.transform.scale;
        Vector3 currentRotate = p.transform.rotate;

        Matrix4x4 particleWorldMatrix;
        if (setting_.isBillboard) {
            Matrix4x4 scaleMatrix = Math::MakeScaleMatrix(currentScale);
            Matrix4x4 rotateMatrix = Math::MakeRotateMatrix(currentRotate.x, currentRotate.y, currentRotate.z);
            Matrix4x4 translateMatrix = Math::MakeTranslateMatrix(p.transform.translate);
            
            particleWorldMatrix = Math::Multiply(scaleMatrix, rotateMatrix);
            particleWorldMatrix = Math::Multiply(particleWorldMatrix, billBoardMatrix);
            particleWorldMatrix = Math::Multiply(particleWorldMatrix, translateMatrix);
        } else {
            particleWorldMatrix = Math::MakeAffineMatrix(currentScale, currentRotate, p.transform.translate);
        }

        particleWorldMatrix = Math::Multiply(particleWorldMatrix, managerWorldMatrix);

        Matrix4x4 worldViewProjection = Math::Multiply(particleWorldMatrix, viewProj);

        if (numInstance_ < numMaxInstance_) {
            instanceData_[numInstance_].world = particleWorldMatrix;
            instanceData_[numInstance_].WVP = worldViewProjection;
            instanceData_[numInstance_].color = p.color;
            numInstance_++;
        }
    }
}
