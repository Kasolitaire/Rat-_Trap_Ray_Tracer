#pragma once
#include "DemoScene.h"
namespace Tmpl8
{
	struct RayCone 
	{
		float pixelSpreadAngle;                // Called wi in the text
		float surfaceSpreadAngle;          // Called γi in the text
	};

class Renderer : public TheApp
{
public:
	// game flow methods
	void Init();
	float3 Trace( Ray& ray, unsigned int depth = 0);
	void Tick( float deltaTime);
	float3 ComputePointLights(const float3 normal, const float3 intersection, float3 albedo, float3 viewDirection, float metallic, float roughness); // works
	float3 ComputeDirectionalLights(const float3 normal, const float3 intersection); // works
	void ComputeSpotLights();
	float2 InterpolateUV(float2 uv0, float2 uv1, float2 uv2, float3 barycentricCoordinates);
	float3 SampleTexture(uint32_t* texture, int texWidth, int texHeight, float2 uv, bool tile);
	float3 SampleTexture(float* texture, int texWidth, int texHeight, float2 uv, bool tile);
	float2 GetSkyUV(const float3& direction);
	float3 SampleSky(const float3& direction);

	//BRDF functions
	float3 CookTorranceBRDF(const float3& lightDirection, const float3& viewDirection, const float3& normal, const float3& FresnelReflectance, float roughness);
	float3 ShadeLambertian(const  float3& normal, const float3& lightVector, const float3& albedo, const float3& lightColor, float lightIntensity, float distance);
	float DistributionGGX(const float3& normal, const float3& halfwayVector, float roughness);
	float3 FresnelSchlick(float cosTheta, const float3& fresnelReflectance);
	float GeometrySchlickGGX(float NdotV, float roughness);
	float GeometrySmith(const float3& normal, const float3& viewDirection, const float3& lightVector, float roughness);

	//mipmapping
	float ComputeSurfaceSpreadAngle(float3 rayDirection, float3 surfaceNormal);
	float ComputeTextureLOD(float3 rayDirection, float3 normal, float triangleLODConstant, RayCone cone, float textureWidth, float textureHeight, float3 surfacePoint, float3 cameraPosition);
	float GetTriangleLODConstant(float3 v0, float3 v1, float3 v2, float2 uv0, float2 uv1, float2 uv2);

	void UI();
	void ImGuiCreateObjectPopout(std::string objectName, bool open);
	void Shutdown() { /* implement if you want to do things on shutdown */ }
	// input handling
	void MouseUp( int button ) { /* implement if you want to detect mouse button presses */ }
	void MouseDown(int button);
	void MouseMove( int x, int y ) { mousePos.x = x, mousePos.y = y; }
	void MouseWheel( float y ) { /* implement if you want to handle the mouse wheel */ }
	void KeyUp( int key ) { /* implement if you want to handle keys */ }
	void KeyDown( int key ) { /* implement if you want to handle keys */ }

	// data members
	int2 mousePos;
	float4* accumulator;
	bool m_rightMouseDown = false;

	//Scene scene;
	DemoScene scene;
	Camera camera;
	bool animating = true;
	float anim_time = 0;

	uint* m_skydomeTexture;
	float* skyPixels = nullptr;
	int skyWidth = 0;
	int skyHeight = 0;
	int skyBpp = 0;
	
};

} // namespace Tmpl8