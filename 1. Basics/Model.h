#pragma once
 
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include "tmpl8math.h"
#include <iostream>
#include <unordered_map>

enum class TextureType 
{
	Diffuse,
	Normal
};

struct Vertex 
{
	float4 position;
	float3 normal;
	float2 texCoords;
	float3 tangent;
	float3 bitangent;
	unsigned int meshIndex;
};


struct TextureData
{
	//unsigned int id;
	TextureType type;
	std::string path;
	int2 dimensions;
};

struct Mesh
{
	// maybe add mesh name
	// maybe also add material
	std::vector<TextureData> textures;
};

class Model
{
public :
	Model(std::string path, std::string directory, std::string name, bool smoothNormals = false);
	tinybvh::BVH8_CPU m_bvh;
public:
	std::vector<Vertex> m_vertices;
	std::unordered_map<std::string, uint*> m_textures;
	std::vector<Mesh> m_meshes;
	std::string m_name;

	std::vector<float4> ExtractPositions(const std::vector<Vertex>& vertices) 
	{
		std::vector<float4> positions;
		positions.reserve(vertices.size());

		for (const auto& vertex : vertices) 
		{
			positions.push_back(vertex.position);
		}

		return positions;
	}

private:
	void LoadTextureType(aiTextureType aiTextureType, aiMaterial* material, std::string directory, Mesh& meshData);
};

