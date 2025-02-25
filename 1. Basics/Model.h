#pragma once
 
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include "tmpl8math.h"
#include <iostream>

struct Triangle 
{
	float4 vertices[3] = { 3 };
	
};

struct Vertex 
{
	float4 position;
	float3 normal;
	float2 texCoords;
};

class Model
{
public :
	Model() 
	{
		std::vector<unsigned int> indcies;

		const char* path = "../assets/Roy_Campbell.obj";
		// read file via ASSIMP
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
		// check for errors
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
		{
			cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
			return;
		}

		unsigned int indexOffset = 0;

		// Iterate through all meshes
		for (unsigned int index = 0; index < scene->mNumMeshes; index++) 
		{
			aiMesh* mesh = scene->mMeshes[index];

			// Add vertices
			for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++)
			{
				Vertex vertex = {};

				// Copy position
				vertex.position.x = mesh->mVertices[vertexIndex].x;
				vertex.position.y = mesh->mVertices[vertexIndex].y;
				vertex.position.z = mesh->mVertices[vertexIndex].z;

				// Copy normal
				if (mesh->HasNormals()) {
					vertex.normal.x = mesh->mNormals[vertexIndex].x;
					vertex.normal.y = mesh->mNormals[vertexIndex].y;
					vertex.normal.z = mesh->mNormals[vertexIndex].z;
				}

				// Copy texture coordinates (only first set)
				if (mesh->HasTextureCoords(0)) {
					vertex.texCoords.x = mesh->mTextureCoords[0][vertexIndex].x;
					vertex.texCoords.y = mesh->mTextureCoords[0][vertexIndex].y;
				}
				else {
					vertex.texCoords.x = vertex.texCoords.y = 0.0f;
				}

				m_vertices.push_back(vertex);
			}

			// Add indices with offset correction
			for (unsigned int f = 0; f < mesh->mNumFaces; f++) 
			{
				aiFace face = mesh->mFaces[f];
				for (unsigned int j = 0; j < face.mNumIndices; j++) 
				{
					indcies.push_back(face.mIndices[j] + indexOffset);
				}
			}

			// Update offset
			indexOffset += mesh->mNumVertices;
		}



		//bvh.Build(&m_vertices[0].position, m_vertices.size() / 3);
		m_bvh.Build(tinybvh::bvhvec4slice(reinterpret_cast<float4*>(&m_vertices[0]), m_vertices.size(), sizeof(Vertex)));
	};
	tinybvh::BVH m_bvh;
public:
	std::vector<Vertex> m_vertices;
private:

};

