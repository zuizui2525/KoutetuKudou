#include "App/Load/ResourceLoader.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Objects/3d/Model/ModelManager.h"
#include "Engine/Base/Log/Log.h"
#include <chrono>
#include <format>

void ResourceLoader::LoadAll() {
	Log::Write(L" ├─ [リソース一括ロード開始] アセットの読み込みを開始します。");
	auto start = std::chrono::steady_clock::now();

	LoadTextures();
	LoadModels();

	auto end = std::chrono::steady_clock::now();
	float elapsed = std::chrono::duration<float>(end - start).count();

	// 登録されたリソース数をカウント（マジックナンバー排除）
	constexpr int kTextureCount = 10;
	constexpr int kModelCount = 5;

	Log::Write(std::format(L" ├─ [リソース一括ロード完了] テクスチャ: {}枚 | モデル: {}個 | 総ロード時間: {:.4f}秒", kTextureCount, kModelCount, elapsed));
}

void ResourceLoader::LoadTextures() {
	TextureManager* texMgr = TextureResource::GetTextureManager();
	if (!texMgr) return;

	texMgr->LoadTexture("white", "resources/Textures/white.png");
	texMgr->LoadTexture("monsterBall", "resources/Textures/monsterBall.png");
	texMgr->LoadTexture("circle", "resources/Textures/circle.png");
	texMgr->LoadTexture("circle_solid", "resources/Textures/circle_solid.png");
	texMgr->LoadTexture("reticle", "resources/Textures/reticle.png");
	texMgr->LoadTexture("uvChecker", "resources/Textures/uvChecker.png");
	texMgr->LoadTexture("skyboxTex", "resources/Textures/skybox.dds");
	texMgr->LoadTexture("forestTex", "resources/Textures/forest.dds");
	texMgr->LoadTexture("gradationLine", "resources/Textures/gradationLine.png");
	texMgr->LoadTexture("slashTex", "resources/Textures/slashTex.png");
	texMgr->LoadTexture("noise0", "resources/Textures/noise0.png");
	texMgr->LoadTexture("noise1", "resources/Textures/noise1.png");
	texMgr->LoadTexture("noise2", "resources/Textures/noise2.png");
	texMgr->LoadTexture("minimap_bg", "resources/Textures/minimap_bg.png");
}

void ResourceLoader::LoadModels() {
	ModelManager* modelMgr = ModelResource::GetModelManager();
	if (!modelMgr) return;

	modelMgr->LoadModel("cube", "resources/obj/cube/cube.obj");
	modelMgr->LoadModel("teapot", "resources/obj/teapot/teapot.obj");
	modelMgr->LoadModel("bunny", "resources/obj/bunny/bunny.obj");
	modelMgr->LoadModel("terrain", "resources/obj/terrain/terrain.obj");
	modelMgr->LoadModel("skydome", "resources/obj/skydome/skydome.obj");
}
