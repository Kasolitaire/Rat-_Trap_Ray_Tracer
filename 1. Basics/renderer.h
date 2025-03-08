#pragma once
#include "DemoScene.h"
namespace Tmpl8
{

class Renderer : public TheApp
{
public:
	// game flow methods
	void Init();
	float3 Trace( Ray& ray );
	void Tick( float deltaTime );
	float3 ComputePointLights(const float3 normal, const float3 intersection); // works
	float3 ComputeDirectionalLights(const float3 normal, const float3 intersection); // works
	void ComputeSpotLights();
	float2 InterpolateUV(float2 uv0, float2 uv1, float2 uv2, float3 barycentricCoordinates);
	float3 SampleTexture(uint32_t* texture, int texWidth, int texHeight, float2 uv, bool tile);
	void UI();
	void ImGuiCreateObjectPopout(std::string objectName, bool open);
	void Shutdown() { /* implement if you want to do things on shutdown */ }
	// input handling
	void MouseUp( int button ) { /* implement if you want to detect mouse button presses */ }
	void MouseDown( int button ) { /* implement if you want to detect mouse button presses */ }
	void MouseMove( int x, int y ) { mousePos.x = x, mousePos.y = y; }
	void MouseWheel( float y ) { /* implement if you want to handle the mouse wheel */ }
	void KeyUp( int key ) { /* implement if you want to handle keys */ }
	void KeyDown( int key ) { /* implement if you want to handle keys */ }

	// data members
	int2 mousePos;
	float4* accumulator;
	//Scene scene;
	DemoScene scene;
	Camera camera;
	bool animating = true;
	float anim_time = 0;

	float* skyPixels = nullptr;
	int skyWidth = 0;
	int skyHeight = 0;
	int skyBpp = 0;
};

} // namespace Tmpl8