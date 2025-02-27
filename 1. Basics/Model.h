#pragma once
 
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include "tmpl8math.h"
#include <iostream>

struct Vertex 
{
	float4 position;
	float3 normal;
	float2 texCoords;
};

class Model
{
public :
	Model(std::string path, bool smoothNormals = false);
	tinybvh::BVH m_bvh;
public:
	std::vector<Vertex> m_vertices;
private:

};

