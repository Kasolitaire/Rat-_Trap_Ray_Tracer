#pragma once
#include "Model.h"
#include "RenderObject.h"   
#include "Material.h"
#include <unordered_map>
#include "Lights.h"
#include <filesystem>
namespace fs = std::filesystem;


struct RenderData 
{
	Model* model;
    unsigned int instanceIndex;
    unsigned int materialIndex;
};

class DemoScene
{
public:
	DemoScene() 
	{

  //      models.push_back(new Model("../assets/Suzanne.obj"));
  //      models.push_back(new Model("../assets/teapot.obj"));

		////Model& model = *models[0];
  ////      if (model.m_bvh.usedNodes > 0)
  ////      {  // Ensure the BVH is not empty
  ////          bvhList.push_back(&model.m_bvh);
  ////      }
  ////      else { assert(0); }

		//// pushing pointer to bvh for every unique model
  //      for (unsigned int index = 0; index < models.size(); index++) 
  //      {
  //          bvhList.push_back(&models[index]->m_bvh);
  //      }


        LoadModel("ilo_cube");
        LoadModel("teapot");

        CreateRenderObject("Monkey", "ilo_cube");
        CreateRenderObject("pot", "teapot");

		//m_pointLights.CreatePointLight(float3(0, 10, 0), float3(1, 1, 1), 100);
        m_directionalLights.CreateDirectionalLight(float3(0, -10.f, 0), float3(1, 1, 1), 10);
	};

    void RebuildTLAS() 
    {
        tlas.Build(instances.data(),
            static_cast<uint32_t>(instances.size()),
            bvhList.data(),
            static_cast<uint32_t>(bvhList.size()));
        isDirty = false;
    };

    void Update() 
    {
        for (auto& pair : m_renderObjects) 
        {
			RenderObject& renderObject = pair.second;

            if (renderObject.IsDirty()) 
            {
				isDirty = true;
                mat4 modelMatrix = renderObject.GetModelMatrix();
				unsigned int  renderDataIndex = renderObject.GetRenderDataIndex();

                RenderData& renderData = m_renderData[renderDataIndex];
                instances[renderData.instanceIndex].UpdateTransform(&modelMatrix[0]);
            };
        }

		if (isDirty)
		{
			RebuildTLAS ();
		}
    };

	void LoadModel(std::string name, bool smoothNormals = false) // only loads obj files
	{

		//std::string path =  + name + ".obj";


        const std::string directory = "../assets/";
        std::vector<fs::path> filePaths;
        std::string extension = ".obj";
        const fs::path& fsDirectory = directory;
        if (fs::exists(fsDirectory) && fs::is_directory(fsDirectory))
        {
            auto iterator = fs::recursive_directory_iterator(fsDirectory);

            for (const auto& entry : iterator)
            {
                if (fs::is_regular_file(entry) && entry.path().extension() == extension)
                {
                    filePaths.push_back(entry.path());
                }
            }
        }
        else
        {
            std::cerr << "ModelManager: Directory does not exist or is not a directory.\n";
            assert(0);
        }

        Model* model = nullptr;

        for (auto path : filePaths)
        {
            std::string pathString = path.string();
            char target = '\\';
            std::string replacement = "/";

            size_t pos = pathString.find(target);
            while (pos != std::string::npos)
            {
                pathString.replace(pos, 1, replacement);
                pos = pathString.find(target, pos + 1);
            }

            std::string objectName = path.filename().replace_extension().string();
            std::string fullDirectory = path.remove_filename().string();
            fullDirectory.erase(fullDirectory.size() - 1);

            if (objectName == name)
            {
                 model = new Model(pathString, fullDirectory, objectName ,smoothNormals);
            }
        }

        if (model == nullptr) 
        {
            std::cout << "Model doesn't exist: " << name << std::endl;
            assert(0);
        } 


		unsigned int modelIndex = models.size();

		m_modelAddressToIndexMap.emplace(model, modelIndex); // allows us to find the index of the model with the address of the model
		m_modelNameToIndexMap.emplace(name, modelIndex); // allows us to find the index of the model with the name of the model

        models.push_back(model);

		bvhList.push_back(&model->m_bvh);

		isDirty = true;
	};  

	unsigned int GetModelIndex(Model* model)
	{
        return 0;
	};



	void CreateRenderObject(std::string objectName, std::string modelName, unsigned int materialIndex = 0) // should model index as parameter and model* as parameter
	{
        if (m_modelNameToIndexMap.find(modelName) == m_modelNameToIndexMap.end()) 
        {
            assert(0);  // Key doesn't exist
        }

        unsigned int modelIndex = m_modelNameToIndexMap.at(modelName);
        unsigned int instanceIndex = instances.size();
		Model* model = models[modelIndex];

		m_renderObjects.emplace(objectName, RenderObject(m_renderData.size()));

        m_renderData.push_back(RenderData{ model, instanceIndex ,materialIndex });

        instances.push_back(tinybvh::BLASInstance(modelIndex));
        
		isDirty = true;
	};

    //models
    std::vector<Model*> models;
    std::unordered_map<Model*, unsigned int> m_modelAddressToIndexMap;
    std::unordered_map<std::string, unsigned int> m_modelNameToIndexMap;

    //bvh
	tinybvh::BVH tlas;
	std::vector<tinybvh::BLASInstance> instances;
    std::vector<tinybvh::BVHBase*> bvhList;

    //data
    std::vector<Material> m_materials;
	std::vector<RenderData> m_renderData;
    std::unordered_map<std::string, RenderObject> m_renderObjects;

	//lights
    PointLights m_pointLights;
	DirectionalLights m_directionalLights;

	bool isDirty = false;
private:

};

