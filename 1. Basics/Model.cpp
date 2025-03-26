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
		LoadTextureType(aiTextureType_DIFFUSE_ROUGHNESS, material, directory, meshData);	

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

				if (std::isnan(vertex.normal.x) || std::isnan(vertex.normal.y) || std::isnan(vertex.normal.z)) __debugbreak();


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

					vertex.handedness = (dot(cross(vertex.tangent, vertex.bitangent), vertex.normal) < 0.0f) ? -1.0f : 1.0f;
				}
				if(length(vertex.tangent) == 0 || length(vertex.bitangent) == 0)
				{
					vertex.tangent = float3(1, 0, 0);
					vertex.bitangent = float3(0, 1, 0);
				}
				if (length(vertex.tangent) == 0) __debugbreak();
				if (length(vertex.bitangent) == 0) __debugbreak();
	

				if (std::isnan(vertex.tangent.x) || std::isnan(vertex.tangent.y) || std::isnan(vertex.tangent.z)) __debugbreak();
				if (std::isnan(vertex.bitangent.x) || std::isnan(vertex.normal.y) || std::isnan(vertex.normal.z)) __debugbreak();

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
	unsigned int textureCount = material->GetTextureCount(aiTextureType);
	for (unsigned int textureIndex = 0; textureIndex < textureCount; textureIndex++)
	{
		//TextureData textureData;
		Texture texture;
		aiString aitexturePath;

		material->GetTexture(aiTextureType, textureIndex, &aitexturePath);
		std::string texturePath = aitexturePath.C_Str();

		Surface surface = Surface((directory + "/" + texturePath).c_str());

		// generate mip maps using open gl

		GLuint textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// Upload sRGB texture for correct mipmap filtering
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, surface.width, surface.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface.pixels);

		// Set filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Trilinear filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Enable anisotropic filtering if supported
		float maxAniso = 0.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso); // Query max supported level
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso); // Set to max
		// Generate mipmaps after setting all parameters
		glGenerateMipmap(GL_TEXTURE_2D);

		int mipLevels = 1 + std::floor(std::log2(std::max(surface.width, surface.height)));

		for (int level = 0; level < mipLevels; level++) 
		{
			int mipWidth = std::max(1, surface.width >> level);
			int mipHeight = std::max(1, surface.height >> level);

			uint* mipPixels = static_cast<uint*>(_aligned_malloc(mipWidth * mipHeight * sizeof(uint), 16)); // Allocate storage
			glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, mipPixels);

			Mip mip;
			mip.texture = mipPixels;
			mip.dimensions = float2(mipWidth, mipHeight);
			texture.mips.push_back(mip);
		}

		switch (aiTextureType)
		{
		case aiTextureType_DIFFUSE:
			//std::cout << "Diffuse texture path: " << aitexturePath.C_Str() << std::endl;
			meshData.diffuseTextures.push_back(texture);
			break;
		case aiTextureType_HEIGHT:
			//std::cout << "Height map texture path: " << aitexturePath.C_Str() << std::endl;
			meshData.normalTextures.push_back(texture);
			break;
		case aiTextureType_NORMALS:
			//std::cout << "Normal map texture path: " << aitexturePath.C_Str() << std::endl;
			meshData.normalTextures.push_back(texture);
			break;
		case aiTextureType_DIFFUSE_ROUGHNESS:
			//std::cout << "Unknown texture path: " << aitexturePath.C_Str() << std::endl;
			meshData.MetallicRoughnessTextures.push_back(texture);
			break;
		default :
			break;
		}

		

		//if (!m_textures.count(texturePath))
		//{
		//	Surface surface = Surface((directory + "/" + texturePath).c_str());

		//	textureData.dimensions.x = surface.width;
		//	textureData.dimensions.y = surface.height;
		//	textureData.path = texturePath;
		//	textureData.type = textureType;

		//	meshData.textures.push_back(textureData);

		//	// Create a deep copy of the pixel data
		//	uint* texturePixelsCopy = new uint[surface.width * surface.height];
		//	std::memcpy(texturePixelsCopy, surface.pixels, surface.width * surface.height * sizeof(uint));

		//	// Store the copied pixel data in the map
		//	m_textures.emplace(texturePath, texturePixelsCopy);
		//}
		//else
		//{
		//	Surface texture = Surface((directory + "/" + texturePath).c_str());

		//	textureData.dimensions.x = surface.width;
		//	textureData.dimensions.y = surface.height;
		//	textureData.path = texturePath;
		//	textureData.type = textureType;

		//	meshData.textures.push_back(textureData);
		//}
	}
}
