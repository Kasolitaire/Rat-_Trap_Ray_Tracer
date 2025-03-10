#pragma once
#include "Model.h"
#include "RenderObject.h"   
#include "Material.h"
#include <unordered_map>
#include "Lights.h"
#include <filesystem>
#include <unordered_set>
namespace fs = std::filesystem;


struct RenderData 
{
    unsigned modelIndex;
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

        LoadModel("Sponza");
        LoadModel("ilo_cube");
        //LoadModel("teapot");
        LoadModel("Sphere", true);
        Material blueDiffuse;
        blueDiffuse.setAlbedo(float3(0.5f,0.5f,1.f));

        Material reflective;
        reflective.setType(MaterialType::Reflective);

        CreateRenderObject("Cube1", "ilo_cube");
        //CreateRenderObject("Cube2", "ilo_cube");
      // CreateRenderObject("pot", "teapot", reflective);
        CreateRenderObject("Sphere", "Sphere", reflective);
        CreateRenderObject("sponza", "Sponza");
        RenderObject& sponza = m_renderObjects.at("sponza");
        sponza.SetScale(float3(0.03f, 0.03f, 0.03f));

		m_pointLights.CreatePointLight(float3(0, 10, 0), float3(1, 1, 1), 100);
        //m_directionalLights.CreateDirectionalLight(float3(0, -10.f, 0), float3(1, 1, 1), 10);
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

        // Set of valid extensions
        const std::unordered_set<std::string> validExtensions = { ".obj", ".gltf", ".glb", ".fbx" };

        const fs::path fsDirectory = directory;
        if (fs::exists(fsDirectory) && fs::is_directory(fsDirectory))
        {
            for (const auto& entry : fs::recursive_directory_iterator(fsDirectory))
            {
                if (fs::is_regular_file(entry))
                {
                    std::string ext = entry.path().extension().string();
                    if (validExtensions.find(ext) != validExtensions.end())
                    {
                        filePaths.push_back(entry.path());
                    }
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

	void CreateRenderObject(std::string objectName, std::string modelName, Material material = Material()) // should model index as parameter and model* as parameter
	{
        // Ensure the modelName exists in the map
        auto it = m_modelNameToIndexMap.find(modelName);
        assert(it != m_modelNameToIndexMap.end() && "Model name not found in the index map!");
        unsigned int modelIndex = it->second;

        // Ensure the model index is within bounds
        assert(modelIndex < models.size() && "Model index out of bounds in the models array!");

        // Ensure the instance index correlates to the correct render data index
        unsigned int instanceIndex = instances.size();
        assert(instanceIndex == m_renderData.size() && "Instance index does not match render data size!");

        // The material index will always be valid as you are pushing the material right after the render data.
        m_renderObjects.emplace(objectName, RenderObject(m_renderData.size()));
        m_renderData.push_back(RenderData{ modelIndex, instanceIndex, (unsigned int)m_materials.size() });
        m_materials.push_back(material);
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

