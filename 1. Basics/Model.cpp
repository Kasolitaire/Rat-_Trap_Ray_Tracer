#include "precomp.h"
#include "Model.h"
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"

Model::Model(std::string path, std::string directory, std::string name, bool smoothNormals) : m_name(name)
{
	std::vector<unsigned int> indcies;

	// read file via ASSIMP
	Assimp::Importer importer;
	aiPostProcessSteps flags;

	if (smoothNormals) 
	{
		flags = aiPostProcessSteps(aiProcess_Triangulate | aiProcess_DropNormals | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	}
	else
	{
		flags = aiPostProcessSteps(aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	}

	const aiScene* scene = importer.ReadFile(path.c_str(), flags);
	// check for errors
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
	{
		cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
		return;
	}

	// Iterate through all meshes
	for (unsigned int index = 0; index < scene->mNumMeshes; index++)
	{
		aiMesh* mesh = scene->mMeshes[index];
		Mesh meshData;

		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		LoadTextureType(aiTextureType_DIFFUSE, material, directory, meshData);
		LoadTextureType(aiTextureType_HEIGHT, material, directory, meshData);
		LoadTextureType(aiTextureType_NORMALS, material, directory, meshData);

		

		for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++)
		{
			aiFace face = mesh->mFaces[faceIndex];

			for (unsigned int index = 0; index < face.mNumIndices; index++)
			{

				unsigned int currentIndice = face.mIndices[index];

				Vertex vertex = {};

				vertex.position.x = mesh->mVertices[currentIndice].x;
				vertex.position.y = mesh->mVertices[currentIndice].y;
				vertex.position.z = mesh->mVertices[currentIndice].z;


				if (mesh->HasNormals()) 
				{
					vertex.normal.x = mesh->mNormals[currentIndice].x;
					vertex.normal.y = mesh->mNormals[currentIndice].y;
					vertex.normal.z = mesh->mNormals[currentIndice].z;
				}
				else
				{
					vertex.normal = float3(0.f);
				}

				if (mesh->HasTextureCoords(0))
				{
					vertex.texCoords.x = mesh->mTextureCoords[0][currentIndice].x;
					vertex.texCoords.y = mesh->mTextureCoords[0][currentIndice].y;
				}
				else
				{
					vertex.texCoords = float2(0.f);
				}

				if (mesh->HasTangentsAndBitangents()) 
				{
					vertex.tangent.x = mesh->mTangents[currentIndice].x;
					vertex.tangent.y = mesh->mTangents[currentIndice].y;
					vertex.tangent.z = mesh->mTangents[currentIndice].z;

					vertex.bitangent.x = mesh->mBitangents[currentIndice].x;
					vertex.bitangent.y = mesh->mBitangents[currentIndice].y;
					vertex.bitangent.z = mesh->mBitangents[currentIndice].z;
				}
				else 
				{
					vertex.tangent = float3(0);
					vertex.bitangent = float3(0);
				}

				vertex.meshIndex = m_meshes.size();

				m_vertices.push_back(vertex);
			}

			m_meshes.push_back(meshData);
		}
	}

	m_bvh.BuildHQ(tinybvh::bvhvec4slice(reinterpret_cast<float4*>(&m_vertices[0]), m_vertices.size(), sizeof(Vertex)));
}

void Model::LoadTextureType(aiTextureType aiTextureType, aiMaterial* material, std::string directory, Mesh& meshData)
{
	unsigned int normalTextureCount = material->GetTextureCount(aiTextureType);
	for (unsigned int normalTextureIndex = 0; normalTextureIndex < normalTextureCount; normalTextureIndex++)
	{
		TextureData textureData;
		aiString aitexturePath;

		material->GetTexture(aiTextureType, normalTextureIndex, &aitexturePath);
		std::string texturePath = aitexturePath.C_Str();
		TextureType textureType;

		switch (aiTextureType)
		{
		case aiTextureType_DIFFUSE:
			textureType = TextureType::Diffuse;
			std::cout << "Diffuse texture path: " << aitexturePath.C_Str() << std::endl;
			break;
		case aiTextureType_HEIGHT:
			textureType = TextureType::Normal;
			std::cout << "Height map texture path: " << aitexturePath.C_Str() << std::endl;
			break;
		case aiTextureType_NORMALS:
			textureType = TextureType::Normal;
			std::cout << "Normal map texture path: " << aitexturePath.C_Str() << std::endl;
			break;
		default :
			break;
		}

		if (!m_textures.count(texturePath))
		{
			Surface texture = Surface((directory + "/" + texturePath).c_str());

			textureData.dimensions.x = texture.width;
			textureData.dimensions.y = texture.height;
			textureData.path = texturePath;
			textureData.type = textureType;

			meshData.textures.push_back(textureData);

			// Create a deep copy of the pixel data
			uint* texturePixelsCopy = new uint[texture.width * texture.height];
			std::memcpy(texturePixelsCopy, texture.pixels, texture.width * texture.height * sizeof(uint));

			// Store the copied pixel data in the map
			m_textures.emplace(texturePath, texturePixelsCopy);
		}
		else
		{
			Surface texture = Surface((directory + "/" + texturePath).c_str());

			textureData.dimensions.x = texture.width;
			textureData.dimensions.y = texture.height;
			textureData.path = texturePath;
			textureData.type = textureType;

			meshData.textures.push_back(textureData);
		}
	}
}
