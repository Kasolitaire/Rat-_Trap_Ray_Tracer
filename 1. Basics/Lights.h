#pragma once
#include "tmpl8math.h"
#include <vector>
struct PointLights
{
	bool enabled = true;
	std::vector<float3> positions;
	std::vector<float3> colors;
	std::vector<float> intensities;
	void CreatePointLight(float3 position, float3 color, float intensity)
	{
		positions.push_back(position);
		colors.push_back(color);
		intensities.push_back(intensity);
	}
};

struct DirectionalLights
{
	bool enabled = true;
	std::vector<float3> directions;
	std::vector<float3> colors;
	std::vector<float> intensities;

	void CreateDirectionalLight(float3 direction, float3 color, float intensity)
	{
		directions.push_back(direction);
		colors.push_back(color);
		intensities.push_back(intensity);
	}
};