#include "App/Load/ResourceLoader.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Objects/3d/Model/ModelManager.h"
#include "Engine/Base/Log/Log.h"
#include <chrono>
#include <format>

namespace {
	// 起動時に事前ロードするエンジンの必須基盤アセット
	constexpr const char* kWhiteTexPath = "resources/Textures/white.png";
	constexpr const char* kLegacyWhiteKey = "white";

	constexpr const char* kSkyboxTexPath = "resources/Textures/skybox.dds";
	constexpr const char* kLegacySkyboxKey = "skyboxTex";

	constexpr const char* kCubeModelPath = "resources/obj/cube/cube.obj";
	constexpr const char* kLegacyCubeKey = "cube";

	constexpr int kEssentialTextureCount = 2;
	constexpr int kEssentialModelCount = 1;
}

void ResourceLoader::LoadAll() {
	Log::Write(L" ├─ [リソース最小限事前ロード開始] エンジン必須アセットの読み込みを開始します。");
	auto start = std::chrono::steady_clock::now();

	LoadTextures();
	LoadModels();

	auto end = std::chrono::steady_clock::now();
	float elapsed = std::chrono::duration<float>(end - start).count();

	Log::Write(std::format(L" ├─ [リソース最小限事前ロード完了] 必須テクスチャ: {}枚 | 必須モデル: {}個 | 所要時間: {:.4f}秒 (他アセットは動的オンデマンドロード)", 
		kEssentialTextureCount, kEssentialModelCount, elapsed));
}

void ResourceLoader::LoadTextures() {
	TextureManager* texMgr = TextureResource::GetTextureManager();
	if (!texMgr) return;

	// エンジン基盤テクスチャ（フォールバック用とスカイボックス）のみ事前ロード
	texMgr->LoadTexture(kWhiteTexPath, kWhiteTexPath);
	texMgr->LoadTexture(kLegacyWhiteKey, kWhiteTexPath);

	texMgr->LoadTexture(kSkyboxTexPath, kSkyboxTexPath);
	texMgr->LoadTexture(kLegacySkyboxKey, kSkyboxTexPath);
}

void ResourceLoader::LoadModels() {
	ModelManager* modelMgr = ModelResource::GetModelManager();
	if (!modelMgr) return;

	// 基本形状のデフォルトモデルのみ事前ロード
	modelMgr->LoadModel(kCubeModelPath, kCubeModelPath);
	modelMgr->LoadModel(kLegacyCubeKey, kCubeModelPath);
}
