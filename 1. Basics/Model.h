#pragma once
 
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include "tmpl8math.h"
#include <iostream>
#include <unordered_map>

struct Vertex 
{
	float4 position;
	float3 normal;
	float2 texCoords;
	unsigned int meshIndex;
};


struct TextureData
{
	//unsigned int id;
	std::string type;
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
	tinybvh::BVH m_bvh;
public:
	std::vector<Vertex> m_vertices;
	std::unordered_map<std::string, uint*> m_textures;
	std::vector<Mesh> m_meshes;
	std::string m_name;

private:

};

