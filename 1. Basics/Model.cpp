#include "precomp.h"
#include "Model.h"
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"

Model::Model(std::string path)
{
	std::vector<unsigned int> indcies;

	//const char* path = "";
	// read file via ASSIMP
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
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

				m_vertices.push_back(vertex);
			}
		}
	}

	m_bvh.Build(tinybvh::bvhvec4slice(reinterpret_cast<float4*>(&m_vertices[0]), m_vertices.size(), sizeof(Vertex)));
}
