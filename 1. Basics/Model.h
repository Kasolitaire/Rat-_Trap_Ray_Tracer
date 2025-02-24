#pragma once
#include "TinyObj.h"
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"

class Model
{
public :
	Model() 
	{

	};
private:

	tinybvh::BVH bvh;
};

