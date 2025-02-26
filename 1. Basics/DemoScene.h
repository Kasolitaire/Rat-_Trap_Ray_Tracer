#pragma once
#include "Model.h"
#include "RenderObject.h"   

class DemoScene
{
public:
	DemoScene() 
	{


        models.push_back(new Model("../assets/Suzanne.obj"));
        models.push_back(new Model("../assets/teapot.obj"));

		//Model& model = *models[0];
  //      if (model.m_bvh.usedNodes > 0)
  //      {  // Ensure the BVH is not empty
  //          bvhList.push_back(&model.m_bvh);
  //      }
  //      else { assert(0); }

        for (unsigned int index = 0; index < models.size(); index++) 
        {
            bvhList.push_back(&models[index]->m_bvh);
        }

        for (unsigned int index = 0; index < bvhList.size(); index++) 
        {
			instances.push_back(tinybvh::BLASInstance(index));
        }

        if (!instances.empty() && !bvhList.empty()) { // Ensure valid inputs
            std::cout << "Calling TLAS Build with "
                << instances.size() << " instances and "
                << bvhList.size() << " BLAS elements.\n";

            tlas.Build(instances.data(),
                static_cast<uint32_t>(instances.size()),
                bvhList.data(),
                static_cast<uint32_t>(bvhList.size()));

            std::cout << "TLAS Build complete!" << std::endl;
            std::cout << "TLAS usedNodes: " << tlas.usedNodes << std::endl;
        }
        else {
            std::cerr << "Error: Instances or BLAS list is empty, skipping TLAS Build!" << std::endl;
        }
	};

    void RebuildTLAS() 
    {
        tlas.Build(instances.data(),
            static_cast<uint32_t>(instances.size()),
            bvhList.data(),
            static_cast<uint32_t>(bvhList.size()));
    };

    std::vector<Model*> models;
	tinybvh::BVH tlas;
	std::vector<tinybvh::BLASInstance> instances;
    std::vector<tinybvh::BVHBase*> bvhList;
    std::vector<RenderObject> m_renderObjects;
private:

};

