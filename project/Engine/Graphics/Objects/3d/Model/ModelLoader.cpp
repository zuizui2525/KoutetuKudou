#include "Engine/Graphics/Objects/3d/Model/ModelLoader.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cassert>
#include <format>

namespace {
	constexpr uint32_t kFirstTextureIndex = 0;
	constexpr char kEmbeddedTexturePrefix = '*';

	Node ReadNode(aiNode* node) {
		assert(node != nullptr);
		Node result;
		aiVector3D scale, translate;
		aiQuaternion rotate;
		node->mTransformation.Decompose(scale, rotate, translate);

		result.transform.scale = { scale.x, scale.y, scale.z };
		result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w };
		result.transform.translate = { -translate.x, translate.y, translate.z };
		result.localMatrix = Math::MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);
		result.name = node->mName.C_Str();

		result.children.resize(node->mNumChildren);
		for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
			result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
		}
		return result;
	}
}

MaterialData ModelLoader::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData ModelLoader::LoadObjFile(const std::string& filename) {
	Assimp::Importer importer;
	ModelData modelData;

	// 四角形ポリゴン等の自動三角形化(Triangulate)および法線自動生成(GenSmoothNormals)を追加
	const aiScene* scene = importer.ReadFile(
		filename.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_GenSmoothNormals
	);
	if (!scene || !scene->HasMeshes()) {
		return modelData;
	}

	if (scene->mRootNode) {
		modelData.rootNode = ReadNode(scene->mRootNode);
	}
	
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		bool hasNormals = mesh->HasNormals();
		bool hasTexcoords = mesh->HasTextureCoords(0);

		uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());
		modelData.vertices.resize(vertexOffset + mesh->mNumVertices);

		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D normal = hasNormals ? mesh->mNormals[vertexIndex] : aiVector3D(0.0f, 1.0f, 0.0f);
			aiVector3D texcoord = hasTexcoords ? mesh->mTextureCoords[0][vertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);

			VertexData vertex;
			vertex.position = { -position.x, position.y, position.z, 1.0f };
			vertex.normal = { -normal.x, normal.y, normal.z };
			vertex.texcoord = { texcoord.x, texcoord.y };
			modelData.vertices[vertexOffset + vertexIndex] = vertex;
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			if (face.mNumIndices != 3) {
				continue;
			}

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				modelData.indices.push_back(vertexOffset + vertexIndex);
			}
		}

		// ボーン情報とスキンウェイトの抽出 (GPUスキニング用)
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			// バインドポーズ行列の抽出と右手系->左手系DirectX変換
			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

			Matrix4x4 bindPoseMatrix = Math::MakeAffineMatrix(
				{ scale.x, scale.y, scale.z },
				{ rotate.x, -rotate.y, -rotate.z, rotate.w },
				{ -translate.x, translate.y, translate.z }
			);
			jointWeightData.inverseBindPoseMatrix = Math::Inverse(bindPoseMatrix);

			// 頂点ごとのウェイト情報を格納 (複数メッシュオフセット vertexOffset を加算)
			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				jointWeightData.vertexWeights.push_back({
					bone->mWeights[weightIndex].mWeight,
					bone->mWeights[weightIndex].mVertexId + vertexOffset
				});
			}
		}
	}

	// マテリアル・テクスチャパスの安全抽出 (DIFFUSE または glTF PBR BASE_COLOR のフォールバック探索)
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		aiString textureFilePath;

		bool hasTexture = (material->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
			material->GetTexture(aiTextureType_DIFFUSE, kFirstTextureIndex, &textureFilePath) == AI_SUCCESS);

		// glTF 2.0 形式の BASE_COLOR テクスチャをフォールバック探索
		if (!hasTexture && material->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
			hasTexture = (material->GetTexture(aiTextureType_BASE_COLOR, kFirstTextureIndex, &textureFilePath) == AI_SUCCESS);
		}

		if (hasTexture) {
			std::string texPathStr = textureFilePath.C_Str();

			// 埋め込みテクスチャ (*0, *1 等) のハンドリング
			if (!texPathStr.empty() && texPathStr[0] == kEmbeddedTexturePrefix) {
				Log::Write(std::format(L" [警告] モデル「{}」に埋め込みテクスチャ「{}」が検出されました。外部テクスチャファイルの利用を推奨します。",
					ConvertString(filename), ConvertString(texPathStr)));
			} else {
				std::filesystem::path fullPath(filename);
				std::filesystem::path dirPath = fullPath.parent_path();
				std::filesystem::path combinedPath = dirPath / texPathStr;
				modelData.material.textureFilePath = combinedPath.generic_string();
				break; // 最初に見つかった有効な外部テクスチャを採用
			}
		}
	}

	return modelData;
}
