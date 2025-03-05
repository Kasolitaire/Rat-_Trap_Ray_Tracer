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
		flags = aiPostProcessSteps(aiProcess_Triangulate | aiProcess_DropNormals | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
	}
	else
	{
		flags = aiPostProcessSteps(aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
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

		// Get textures from the material
		unsigned int diffuseTextureCount = material->GetTextureCount(aiTextureType_DIFFUSE);
		for (unsigned int diffuseTextureIndex = 0; diffuseTextureIndex < diffuseTextureCount; ++diffuseTextureIndex)
		{
			TextureData textureData;
			aiString aitexturePath;

			material->GetTexture(aiTextureType_DIFFUSE, diffuseTextureIndex, &aitexturePath);
			std::cout << "Diffuse texture path: " << aitexturePath.C_Str() << std::endl;
			std::string texturePath = aitexturePath.C_Str();

			if (!m_textures.count(texturePath))
			{
				Surface texture = Surface((directory + "/" + texturePath).c_str());

				textureData.dimensions.x = texture.width;
				textureData.dimensions.y = texture.height;
				textureData.path = texturePath;

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

				meshData.textures.push_back(textureData);
			}
		}

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


				if (mesh->HasNormals()) {
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

				vertex.meshIndex = m_meshes.size();

				m_vertices.push_back(vertex);
			}

			m_meshes.push_back(meshData);
		}
	}

	m_bvh.Build(tinybvh::bvhvec4slice(reinterpret_cast<float4*>(&m_vertices[0]), m_vertices.size(), sizeof(Vertex)));
}
