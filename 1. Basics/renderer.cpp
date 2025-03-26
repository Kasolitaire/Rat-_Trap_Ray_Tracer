#include "precomp.h"
#include "../lib/stb_image.h"
#include <iostream>
#include <chrono>

using namespace chrono;

// -----------------------------------------------------------
// Initialize the renderer
// -----------------------------------------------------------
static bool enableNormalMaps = true;
static bool enableMipMapping = true;
static bool viewMipMapping = true;
static bool enableMaterialPoints = true;

void Renderer::Init()
{
	// create fp32 rgb pixel buffer to render to
	accumulator = (float4*)MALLOC64( SCRWIDTH * SCRHEIGHT * 16 );
	memset( accumulator, 0, SCRWIDTH * SCRHEIGHT * 16 );

	// load HDR sky
	 // load HDR sky
	int bpp = 0;
	skyPixels = stbi_loadf("../assets/sky_19.hdr", &skyWidth, &skyHeight, &skyBpp, 0);
	for (int i = 0; i < skyWidth * skyHeight * 3; i++) skyPixels[i] = sqrtf(skyPixels[i]);

	/*Surface hdrTexture = Surface("../assets/kloppenheim_06_puresky_4k.hdr");
	skyWidth = hdrTexture.width;
	skyHeight = hdrTexture.height;

	uint* texturePixelsCopy = new uint[hdrTexture.width * hdrTexture.height];*/
}

// -----------------------------------------------------------
// Evaluate light transport
// -----------------------------------------------------------
float3 Renderer::Trace( Ray& ray, unsigned int depth)
{

	float3 finalColor = float3(0);

	tinybvh::Ray bvh_ray(ray.O, normalize(ray.D)); // Ensure ray direction is normalized
	scene.tlas.Intersect(bvh_ray) && bvh_ray.hit.t > 0.0f;

	if (depth > 5) return SampleSky(bvh_ray.D);

	if (bvh_ray.hit.t < BVH_FAR) // Check if there is a hit
	{
		
		uint32_t instanceIndex = bvh_ray.hit.inst;
		tinybvh::BLASInstance& instance = scene.instances[instanceIndex];
		RenderData& renderData = scene.m_renderData[instanceIndex];
		Model& model = *scene.models[renderData.modelIndex];
		Material& material = scene.m_materials[renderData.instanceIndex];

		mat4 modelMatrix = *reinterpret_cast<mat4*>(&instance.transform);
		mat4 inverseMatrix = *reinterpret_cast<mat4*>(&instance.invTransform);

		float3 intersection = ray.O + bvh_ray.hit.t * ray.D;

		// Calculate barycentric coordinates
		float u = bvh_ray.hit.u;
		float v = bvh_ray.hit.v;
		float w = 1.0f - u - v;


		uint32_t index = bvh_ray.hit.prim;

		Vertex vertex1 = model.m_vertices[index * 3];
		Vertex vertex2 = model.m_vertices[index * 3 + 1];
		Vertex vertex3 = model.m_vertices[index * 3 + 2];

		float3 normal1 = vertex1.normal;
		float3 normal2 = vertex2.normal;
		float3 normal3 = vertex3.normal;

		float3 interpolatedNormal = (w * normal1) + (u * normal2) + (v * normal3);
		float3 temp = interpolatedNormal;

		// Interpolating normals, but matrix transformation is not needed for ray tracing
		// interpolatedNormal = (float3(inverseMatrix.Transposed() * float4(interpolatedNormal, 0.0f)));

		float2 interpolatedUVCoords = InterpolateUV(vertex1.texCoords, vertex2.texCoords, vertex3.texCoords, float3(w, u, v));

		float3 surfaceNormal = interpolatedNormal;
		 surfaceNormal = (float3(inverseMatrix.Transposed() * float4(surfaceNormal, 0.0f)));
		 surfaceNormal = normalize(surfaceNormal);

		float3 albedo = float3(1.f);
		float3 mipSample = float3(0.f);
		float3 metallicRoughnessSample = float3(1.f);
		RayCone rayCone;
		rayCone.pixelSpreadAngle = camera.pixelSpreadAngle();
		rayCone.surfaceSpreadAngle = ComputeSurfaceSpreadAngle(bvh_ray.D, surfaceNormal);
		float triangleLODConstant = GetTriangleLODConstant(vertex1.position, vertex2.position, vertex3.position, vertex1.texCoords, vertex2.texCoords, vertex3.texCoords); // can be precalculated

		Mesh& mesh = model.m_meshes[vertex1.meshIndex];

		float3 scale = float3(instance.transform[0], instance.transform[5], instance.transform[10]);
		float modelScale = length(scale);

		if (mesh.diffuseTextures.size()) 
		{
			uint* baseDiffuseTexture = mesh.diffuseTextures[0].mips[0].texture;
			float2 baseDimensions = mesh.diffuseTextures[0].mips[0].dimensions;
			int levels = mesh.diffuseTextures[0].mips.size();

			float lambda = ComputeTextureLOD(bvh_ray.D, surfaceNormal, triangleLODConstant, rayCone, baseDimensions.x, baseDimensions.y, intersection, bvh_ray.O);
			int mipLevel = int(clamp(int(floorf(lambda + 0.5f)), 0, levels - 1));  // Adding 0.5 for better rounding

			Mip selectedMip = mesh.diffuseTextures[0].mips[mipLevel];

			uint* selectedDiffuseTexture = selectedMip.texture;
			float2 selectedDimensions = selectedMip.dimensions;

			//using Clock = std::chrono::high_resolution_clock;
			//int iterations = 1000000; // 1 million calls
			//auto start = Clock::now();
			//float3 result = float3();
			//for (int i = 0; i < iterations; ++i) 
			//{
			//	result = SampleTexture(selectedDiffuseTexture, selectedDimensions.x, selectedDimensions.y, interpolatedUVCoords, true);
			//}
			//auto end = Clock::now();
			//auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			//std::cout << "Average execution time: " << duration / iterations << " ns" << std::endl;


			if(enableMipMapping) albedo = SampleTexture(selectedDiffuseTexture, selectedDimensions.x, selectedDimensions.y, interpolatedUVCoords, true);
			else albedo = SampleTexture(baseDiffuseTexture, baseDimensions.x, baseDimensions.y, interpolatedUVCoords, true);

			if (viewMipMapping) 
			{
				if (mipLevel == 0) {
					mipSample = float3(1.0f, 0.0f, 0.0f);  // Red for highest detail
				}
				else if (mipLevel == 1) {
					mipSample = float3(1.0f, 1.0f, 0.0f);  // Yellow for slightly lower detail
				}
				else if (mipLevel == 2) {
					mipSample = float3(0.0f, 1.0f, 0.0f);  // Green for mid detail
				}
				else if (mipLevel == 3) {
					mipSample = float3(0.0f, 0.5f, 1.0f);  // Light blue for lower detail
				}
				else if (mipLevel == 4) {
					mipSample = float3(0.0f, 0.0f, 1.0f);  // Dark blue for low detail
				}
				else if (mipLevel == 5) {
					mipSample = float3(0.5f, 0.0f, 1.0f);  // Purple for even lower detail
				}
				else {
					mipSample = float3(1.0f, 1.0f, 1.0f);  // White for the lowest detail
				}

				albedo = mipSample;
			}
			
		}

		if (mesh.normalTextures.size() && enableNormalMaps)
		{
			float3 T = normalize(vertex1.tangent * w + vertex2.tangent * u + vertex3.tangent * v);
			float3 B = normalize(vertex1.bitangent * w + vertex2.bitangent * u + vertex3.bitangent * v);
			T = normalize(T - dot(T, interpolatedNormal) * interpolatedNormal); // Gram-Schmidt orthogonalization
			B = normalize(cross(interpolatedNormal, T));
			B *= vertex1.handedness;// Ensure correct handedness

			// Manually assign the TBN matrix (assuming row-major order)
			mat4 TBN;
			TBN[0] = T.x;  TBN[1] = B.x;  TBN[2] = interpolatedNormal.x;  TBN[3] = 0.f;
			TBN[4] = T.y;  TBN[5] = B.y;  TBN[6] = interpolatedNormal.y;  TBN[7] = 0.f;
			TBN[8] = T.z;  TBN[9] = B.z;  TBN[10] = interpolatedNormal.z; TBN[11] = 0.f;
			TBN[12] = 0.f; TBN[13] = 0.f; TBN[14] = 0.f; TBN[15] = 1.f; // Identity row

			uint* baseNormalTexture = mesh.normalTextures[0].mips[0].texture;
			float2 baseDimensions = mesh.normalTextures[0].mips[0].dimensions;
			int levels = mesh.normalTextures[0].mips.size();

			float lambda = ComputeTextureLOD(bvh_ray.D, surfaceNormal, triangleLODConstant, rayCone, baseDimensions.x, baseDimensions.y, intersection, bvh_ray.O);
			int mipLevel = int(clamp(int(floorf(lambda + 0.5f)), 0, levels - 1));  // Adding 0.5 for better rounding

			Mip selectedMip = mesh.normalTextures[0].mips[mipLevel];

			uint* selectedNormalTexture = selectedMip.texture;
			float2 selectedDimensions = selectedMip.dimensions;

	/*		uint* normalTexture = mesh.normalTextures[0].mips[0].texture;
			float2 dimensions = mesh.diffuseTextures[0].mips[0].dimensions;*/
			float3 normalSample = float3(0);
			if (enableMipMapping) normalSample = SampleTexture(selectedNormalTexture, selectedDimensions.x, selectedDimensions.y, interpolatedUVCoords, true);
			else normalSample = SampleTexture(baseNormalTexture, baseDimensions.x, baseDimensions.y, interpolatedUVCoords, true);
			float3 N_tangent = normalSample * 2.0f - 1.0f; // Convert from [0,1] to [-1,1]

			// Transform normal using the upper-left 3×3 portion of TBN
			interpolatedNormal = normalize(float3(
				TBN[0] * N_tangent.x + TBN[1] * N_tangent.y + TBN[2] * N_tangent.z,
				TBN[4] * N_tangent.x + TBN[5] * N_tangent.y + TBN[6] * N_tangent.z,
				TBN[8] * N_tangent.x + TBN[9] * N_tangent.y + TBN[10] * N_tangent.z
			));
		}

		if (mesh.MetallicRoughnessTextures.size() && enableMaterialPoints) 
		{

			uint* baseMetallicRoughnessTexture = mesh.normalTextures[0].mips[0].texture;
			float2 baseDimensions = mesh.normalTextures[0].mips[0].dimensions;
			int levels = mesh.normalTextures[0].mips.size();

			float lambda = ComputeTextureLOD(bvh_ray.D, surfaceNormal, triangleLODConstant, rayCone, baseDimensions.x, baseDimensions.y, intersection, bvh_ray.O);
			int mipLevel = int(clamp(int(floorf(lambda + 0.5f)), 0, levels - 1));  // Adding 0.5 for better rounding

			Mip selectedMip = mesh.normalTextures[0].mips[mipLevel];

			uint* selectedMetallicRoughnessTexture = selectedMip.texture;
			float2 selectedDimensions = selectedMip.dimensions;
			if (enableMipMapping) metallicRoughnessSample = SampleTexture(selectedMetallicRoughnessTexture, selectedDimensions.x, selectedDimensions.y, interpolatedUVCoords, true);
			else metallicRoughnessSample = SampleTexture(baseMetallicRoughnessTexture, baseDimensions.x, baseDimensions.y, interpolatedUVCoords, true);

		}

		// Check if texture is available
		//if (model.m_textures.size())
		//{
		//	for (TextureData& textureData : model.m_meshes[vertex1.meshIndex].textures)
		//	{
		//		std::string textureKey = textureData.path;
		//		TextureType type = textureData.type;

		//		if (type == TextureType::Diffuse)
		//		{
		//			uint* diffuseTexture = model.m_textures[textureKey];
		//			albedo = SampleTexture(diffuseTexture, textureData.dimensions.x, textureData.dimensions.y, interpolatedUVCoords, true);
		//			float lambda = ComputeTextureLOD(bvh_ray.D, surfaceNormal, triangleLODConstant, rayCone, textureData.dimensions.x, textureData.dimensions.y, intersection, camera.camPos);

		//			// Calculate the number of mip levels for the texture
		//			int textureWidth = textureData.dimensions.x;
		//			int textureHeight = textureData.dimensions.y;
		//			int levels = 1; // At least one level (the base level)


		//			//if (lambda < -2.0f) mipSample = float3(1, 0, 0);  // Red = most detailed
		//			//else if (lambda < 0.0f) mipSample = float3(1, 1, 0);  // Yellow
		//			//else if (lambda < 2.0f) mipSample = float3(0, 1, 0);  // Green
		//			//else mipSample = float3(0, 0, 1);  // Blue = least detail

		//			// Calculate how many mipmap levels are needed
		//			//while (textureWidth > 1 || textureHeight > 1)
		//			//{
		//			//	textureWidth = max(textureWidth / 2, 1);  // Halve the width, but ensure it doesn't go below 1
		//			//	textureHeight = max(textureHeight / 2, 1); // Halve the height, but ensure it doesn't go below 1
		//			//	levels++; // Increase the mipmap level count
		//			//}

		//			//// Compute mip level based on lambda (texture detail)
		//		

		//			//int mipLevel = int(clamp(int(floorf(lambda + 0.5f)), 0, levels - 1));  // Adding 0.5 for better rounding

		//			//// Optional: Visualize the mip level (for debugging)
		//			//if (mipLevel == 0) {
		//			//	mipSample = float3(1.0f, 0.0f, 0.0f);  // Red for highest detail
		//			//}
		//			//else if (mipLevel == 1) {
		//			//	mipSample = float3(1.0f, 1.0f, 0.0f);  // Yellow for slightly lower detail
		//			//}
		//			//else if (mipLevel == 2) {
		//			//	mipSample = float3(0.0f, 1.0f, 0.0f);  // Green for mid detail
		//			//}
		//			//else if (mipLevel == 3) {
		//			//	mipSample = float3(0.0f, 0.5f, 1.0f);  // Light blue for lower detail
		//			//}
		//			//else if (mipLevel == 4) {
		//			//	mipSample = float3(0.0f, 0.0f, 1.0f);  // Dark blue for low detail
		//			//}
		//			//else if (mipLevel == 5) {
		//			//	mipSample = float3(0.5f, 0.0f, 1.0f);  // Purple for even lower detail
		//			//}
		//			//else {
		//			//	mipSample = float3(1.0f, 1.0f, 1.0f);  // White for the lowest detail
		//			//}

		//			//albedo = mipSample;

		//		}
		//	
		//

		//		if (type == TextureType::Normal && enableNormalMaps)
		//		{
		//			float3 T = normalize(vertex1.tangent * w + vertex2.tangent * u + vertex3.tangent * v);
		//			float3 B = normalize(vertex1.bitangent * w + vertex2.bitangent * u + vertex3.bitangent * v);
		//			T = normalize(T - dot(T, interpolatedNormal) * interpolatedNormal); // Gram-Schmidt orthogonalization
		//			B = normalize(cross(interpolatedNormal, T)); 
		//			B *= vertex1.handedness;// Ensure correct handedness

		//			// Manually assign the TBN matrix (assuming row-major order)
		//			mat4 TBN;
		//			TBN[0] = T.x;  TBN[1] = B.x;  TBN[2] = interpolatedNormal.x;  TBN[3] = 0.f;
		//			TBN[4] = T.y;  TBN[5] = B.y;  TBN[6] = interpolatedNormal.y;  TBN[7] = 0.f;
		//			TBN[8] = T.z;  TBN[9] = B.z;  TBN[10] = interpolatedNormal.z; TBN[11] = 0.f;
		//			TBN[12] = 0.f; TBN[13] = 0.f; TBN[14] = 0.f; TBN[15] = 1.f; // Identity row

		//			uint* normalTexture = model.m_textures[textureKey];
		//			float3 normalSample = SampleTexture(normalTexture, textureData.dimensions.x, textureData.dimensions.y, interpolatedUVCoords, true);
		//			float3 N_tangent = normalSample * 2.0f - 1.0f; // Convert from [0,1] to [-1,1]

		//			// Transform normal using the upper-left 3×3 portion of TBN
		//			interpolatedNormal = normalize(float3(
		//				TBN[0] * N_tangent.x + TBN[1] * N_tangent.y + TBN[2] * N_tangent.z,
		//				TBN[4] * N_tangent.x + TBN[5] * N_tangent.y + TBN[6] * N_tangent.z,
		//				TBN[8] * N_tangent.x + TBN[9] * N_tangent.y + TBN[10] * N_tangent.z
		//			));
		//		}
		//	}
		//}

		interpolatedNormal = (float3(inverseMatrix.Transposed() * float4(interpolatedNormal, 0.0f)));
		interpolatedNormal = normalize(interpolatedNormal);


		if (material.isReflective()) 
		{
			float3 reflection = bvh_ray.D - (2.0f * dot(bvh_ray.D, interpolatedNormal)) * interpolatedNormal;

			float3 newOrigin = intersection + interpolatedNormal * EPSILON;
			return Trace(Ray(newOrigin, reflection), depth + 1);
		}
		else if (material.getType() == MaterialType::Glossy)
		{
			// Sample a microfacet distribution
		}
		else if (material.getType() == MaterialType::Dielectric)
		{
			// Compute refraction using IOR
		}
		else 
		{
			std::string name = model.m_name;

			if (scene.m_pointLights.enabled && scene.m_pointLights.positions.size())
			{
				finalColor += ComputePointLights(interpolatedNormal, intersection, albedo, -bvh_ray.D, metallicRoughnessSample.z, metallicRoughnessSample.y);
			}

			if (scene.m_directionalLights.enabled && scene.m_directionalLights.directions.size())
			{
				finalColor += ComputeDirectionalLights(interpolatedNormal, intersection);
			}

			return finalColor;
			return (interpolatedNormal + 1) * 0.5f;
			return albedo;
			return mipSample;
			//finalColor *= material.getAlbedo();
			//finalColor *= albedo;
		}
	}
	else
	{

		return SampleSky(bvh_ray.D);
		//return SampleTexture(skyPixels, skyWidth, skyHeight, GetSkyUV(bvh_ray.D), true);
	}

	//scene.FindNearest( ray );
	//if (ray.objIdx == -1) return 0; // or a fancy sky color
	//float3 I = ray.O + ray.t * ray.D;
	//float3 N = scene.GetNormal( ray.objIdx, I, ray.D );
	//float3 albedo = scene.GetAlbedo( ray.objIdx, I );
	/* visualize normal */ //return (N + 1) * 0.5f;
	/* visualize distance */ // return 0.1f * float3( ray.t, ray.t, ray.t );
	/* visualize albedo */ // return albedo;
	//return albedo;
}

// -----------------------------------------------------------
// Main application tick function - Executed once per frame
// -----------------------------------------------------------tyr 
void Renderer::Tick( float deltaTime )
{
	//RenderObject& torus = scene.m_renderObjects.at("torus");
	//float3 rotation = torus.GetRotation();
	//float rotationSpeed = 0.001f;  // Control the rotation speed
	//rotation.y += rotationSpeed * deltaTime;  // Apply deltaTime to make it frame rate independent
	//torus.SetRotation(rotation);

	// animation
	//if (animating) scene.SetTime( anim_time += deltaTime * 0.002f );
	// pixel loop
	Timer t;
	// lines are executed as OpenMP parallel tasks (disabled in DEBUG)

	//if (MouseDown()) std::cout << "Mouse Down" << std::endl;
	scene.Update();
	
#pragma omp parallel for schedule(dynamic)
	for (int y = 0; y < SCRHEIGHT; y++)
	{

		// trace a primary ray for each pixel on the line
		for (int x = 0; x < SCRWIDTH; x++)
		{

			Ray primaryRay(camera.GetPrimaryRay((float)x, (float)y));
			if (mousePos.x == x && mousePos.y == y && m_rightMouseDown) DebugBreak();
			float4 pixel = float4( Trace( primaryRay ), 0 );
			// translate accumulator contents to rgb32 pixels
			screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8( &pixel );
			accumulator[x + y * SCRWIDTH] = pixel;
		}
	}
	m_rightMouseDown = false;
	// performance report - running average - ms, MRays/s
	static float avg = 10, alpha = 1;
	avg = (1 - alpha) * avg + alpha * t.elapsed() * 1000;
	if (alpha > 0.05f) alpha *= 0.5f;
	float fps = 1000.0f / avg, rps = (SCRWIDTH * SCRHEIGHT) / avg;
	printf( "%5.2fms (%.1ffps) - %.1fMrays/s\n", avg, fps, rps / 1000 );
	// handle user input
	camera.HandleInput( deltaTime );
}


float3 Tmpl8::Renderer::ComputePointLights(const float3 normal, const float3 intersection, float3 albedo, float3 viewDirection, float metallic, float roughness)
{
	float3 finalColor = float3(0);
	//DebugBreak();//albedo = float3(1, 0, 0);

	for (unsigned int index = 0; index < scene.m_pointLights.positions.size(); index++)
	{

		float3 lightPosition = scene.m_pointLights.positions[index];
		float3 lightVector = lightPosition - intersection;
		float distance = length(lightVector); // Correct distance before normalization
		float3 lightDirection = lightVector / distance; // Normalized light direction

		//float cosa = max(0.0f, dot(normal, normalize(lightDirection)));

		float3 newOrigin = intersection + normal * EPSILON;
		tinybvh::Ray shadowRay(newOrigin, lightDirection, distance);
		if (!scene.tlas.IsOccludedTLAS(shadowRay))
		{

			//finalColor += scene.m_pointLights.colors[index] * scene.m_pointLights.intensities[index] * (1 / (distance * distance)) * cosa;
			finalColor += ShadeLambertian(normal, lightDirection, albedo, scene.m_pointLights.colors[index], scene.m_pointLights.intensities[index], distance);

			float3 fresnelReflectance = (1.0f - metallic) * float3(0.04f) + metallic * albedo;

			float3 kd = 1.0f - fresnelReflectance; // Fresnel term reduces diffuse
			kd *= 1.0f - metallic;  // Metals have no diffuse component

			// Cook-Torrance specular shading
			finalColor += CookTorranceBRDF(lightDirection, viewDirection, normal, fresnelReflectance, roughness);
		}
	}

	return finalColor;
}

float3 Tmpl8::Renderer::ComputeDirectionalLights(const float3 normal, const float3 intersection)
{
	float3 finalColor = float3(0);

	float3 lightDirection = normalize(-scene.m_directionalLights.directions[0]);
	float cosa = max(0.0f, dot(normal, normalize(lightDirection)));

	float3 newOrigin = intersection + normal * EPSILON;
	tinybvh::Ray shadowRay(newOrigin, lightDirection);  // No distance needed!

	if (!scene.tlas.IsOccluded(shadowRay))
	{
		finalColor += scene.m_directionalLights.colors[0] * scene.m_directionalLights.intensities[0] * cosa;
	}

	return finalColor;
}

void Tmpl8::Renderer::ComputeSpotLights()
{
}

float2 Tmpl8::Renderer::InterpolateUV(float2 uv0, float2 uv1, float2 uv2, float3 barycentricCoordinates)
{
	return barycentricCoordinates.x * uv0 + barycentricCoordinates.y * uv1 + barycentricCoordinates.z * uv2;
}

float3 Tmpl8::Renderer::SampleTexture(uint32_t* texture, int texWidth, int texHeight, float2 uv, bool tile)
{


	// Wrap or clamp UV coordinates
	if (tile) {
		uv.x -= floorf(uv.x);
		uv.y -= floorf(uv.y);
	}
	else {
		uv.x = std::clamp(uv.x, 0.0f, 1.0f);
		uv.y = std::clamp(uv.y, 0.0f, 1.0f);
	}

	// **Extra Clamp to Ensure Valid Range**
	uv.x = std::clamp(uv.x, 0.0f, 1.0f);
	uv.y = std::clamp(uv.y, 0.0f, 1.0f);

	// Scale to texel space
	float x = uv.x * (texWidth - 1);
	float y = uv.y * (texHeight - 1);

	// **Clamp Again Before Indexing**
	x = std::clamp(x, 0.0f, float(texWidth - 1));
	y = std::clamp(y, 0.0f, float(texHeight - 1));

	// Compute texel indices
	int x0 = static_cast<int>(x);
	int y0 = static_cast<int>(y);
	int x1 = std::min(x0 + 1, texWidth - 1);
	int y1 = std::min(y0 + 1, texHeight - 1);


	// Compute interpolation weights
	float dx = x - x0;
	float dy = y - y0;

	// Ensure texture dimensions are valid
	if (texWidth <= 0 || texHeight <= 0) __debugbreak();

	// Ensure UV coordinates are not NaN
	if (std::isnan(uv.x) || std::isnan(uv.y)) __debugbreak();

	// Ensure computed texel coordinates are within bounds
	if (x0 < 0 || x0 >= texWidth || y0 < 0 || y0 >= texHeight ||
		x1 < 0 || x1 >= texWidth || y1 < 0 || y1 >= texHeight) {
		__debugbreak();
	}

	// Ensure interpolation weights are valid
	if (std::isnan(dx) || std::isnan(dy)) __debugbreak();

	// Wrap or clamp UV coordinates
	if (tile) {
		uv.x -= floorf(uv.x);
		uv.y -= floorf(uv.y);
	}
	else {
		uv.x = std::clamp(uv.x, 0.0f, 1.0f);
		uv.y = std::clamp(uv.y, 0.0f, 1.0f);
	}

	// Load texel values as 32-bit integers (RGBA packed)
	__m128i texel00 = _mm_cvtsi32_si128(texture[y0 * texWidth + x0]);
	__m128i texel10 = _mm_cvtsi32_si128(texture[y0 * texWidth + x1]);
	__m128i texel01 = _mm_cvtsi32_si128(texture[y1 * texWidth + x0]);
	__m128i texel11 = _mm_cvtsi32_si128(texture[y1 * texWidth + x1]);

	// Convert from packed 8-bit (RGBA8888) to 32-bit integer channels
	texel00 = _mm_cvtepu8_epi32(texel00);
	texel10 = _mm_cvtepu8_epi32(texel10);
	texel01 = _mm_cvtepu8_epi32(texel01);
	texel11 = _mm_cvtepu8_epi32(texel11);

	// Convert to float and normalize to [0, 1]
	__m128 scale = _mm_set1_ps(1.0f / 255.0f);
	__m128 c00 = _mm_mul_ps(_mm_cvtepi32_ps(texel00), scale);
	__m128 c10 = _mm_mul_ps(_mm_cvtepi32_ps(texel10), scale);
	__m128 c01 = _mm_mul_ps(_mm_cvtepi32_ps(texel01), scale);
	__m128 c11 = _mm_mul_ps(_mm_cvtepi32_ps(texel11), scale);

	// **Explicitly Zero Out Alpha**
	__m128 mask = _mm_castsi128_ps(_mm_set_epi32(0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF));
	c00 = _mm_and_ps(c00, mask);
	c10 = _mm_and_ps(c10, mask);
	c01 = _mm_and_ps(c01, mask);
	c11 = _mm_and_ps(c11, mask);

	// Bilinear interpolation weights
	__m128 w00 = _mm_set1_ps((1.0f - dx) * (1.0f - dy));
	__m128 w10 = _mm_set1_ps(dx * (1.0f - dy));
	__m128 w01 = _mm_set1_ps((1.0f - dx) * dy);
	__m128 w11 = _mm_set1_ps(dx * dy);

	// Compute bilinear interpolation
	__m128 result = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(c00, w00), _mm_mul_ps(c10, w10)),
		_mm_add_ps(_mm_mul_ps(c01, w01), _mm_mul_ps(c11, w11))
	);

	// **Extract ONLY RGB from the final result**
	alignas(16) float temp[4];  // Ensure memory is aligned for SIMD
	_mm_storeu_ps(temp, result);

	float3 finalColor = { temp[2], temp[1],temp[0] };  // Extract R, G, B
	return finalColor;
		

	//float3 T00;
	//T00.x = r4f.a[0];
	//T00.y = g4f.a[0];
	//T00.z = b4f.a[0];
	//		  
	//float3 T10;
	//T10.x = r4f.a[1];
	//T10.y = g4f.a[1];
	//T10.z = b4f.a[1];

	//float3 T01;
	//T01.x = r4f.a[2];
	//T01.y = g4f.a[2];
	//T01.z = b4f.a[2];

	//float3 T11;
	//T11.x = r4f.a[3];
	//T11.y = g4f.a[3];
	//T11.z = b4f.a[3];


 //   // Bilinear interpolation
 // // Compute weights
	//float w0 = 1.0f - dx;
	//float w1 = dx;
	//float h0 = 1.0f - dy;
	//float h1 = dy;

	//// Compute horizontal blendin


	//float3 L0_left = w0 * T00;

	//float3 L0_right = w1 * T10;
	//float3 L0 = L0_left + L0_right;

	//float3 L1_left = w0 * T01;
	//float3 L1_right = w1 * T11;
	//float3 L1 = L1_left + L1_right;

	//float3 finalColor_bottom = h1 * L1;
	//// Compute vertical blending
	//float3 finalColor_top = h0 * L0;
	//float3 finalColor = finalColor_top + finalColor_bottom;

 //   return finalColor;


	/// bilinear filtering non-vectorized

	//// Handle UV wrapping or clamping
	//if (tile) {
	//	uv = float2(uv.x - floor(uv.x), uv.y - floor(uv.y)); // Wrap
	//}
	//else {
	//	uv = float2(std::clamp(uv.x, 0.0f, 1.0f), std::clamp(uv.y, 0.0f, 1.0f)); // Clamp
	//}

	//// Scale UV coordinates to texel space
	//float x = uv.x * (texWidth - 1);
	//float y = uv.y * (texHeight - 1);

	//// Compute integer texel positions
	//int x0 = static_cast<int>(x);
	//int y0 = static_cast<int>(y);
	//int x1 = std::min(x0 + 1, texWidth - 1);
	//int y1 = std::min(y0 + 1, texHeight - 1);

	//// Compute interpolation factors
	//float dx = x - x0;
	//float dy = y - y0;

	//// Fetch the four neighboring texels
	//uint32_t texel00 = texture[y0 * texWidth + x0];
	//uint32_t texel10 = texture[y0 * texWidth + x1];
	//uint32_t texel01 = texture[y1 * texWidth + x0];
	//uint32_t texel11 = texture[y1 * texWidth + x1];

	//// Extract RGB components
	//auto UnpackColor = [](uint32_t texel) -> float3 {
	//	return float3(
	//		((texel >> 16) & 0xFF) / 255.0f,
	//		((texel >> 8) & 0xFF) / 255.0f,
	//		(texel & 0xFF) / 255.0f
	//	);
	//	};

	//float3 T00 = UnpackColor(texel00);
	//float3 T10 = UnpackColor(texel10);
	//float3 T01 = UnpackColor(texel01);
	//float3 T11 = UnpackColor(texel11);

	//// Bilinear interpolation
	//float3 L0 = (1.0f - dx) * T00 + dx * T10;
	//float3 L1 = (1.0f - dx) * T01 + dx * T11;
	//float3 finalColor = (1.0f - dy) * L0 + dy * L1;

	//return finalColor;

	/// sampling og

	//// Handle UV wrapping or clamping
	//if (tile) {
	//	uv = float2(uv.x - floor(uv.x), uv.y - floor(uv.y)); // Wrap
	//}
	//else {
	//	uv = float2(std::clamp(uv.x, 0.0f, 1.0f), std::clamp(uv.y, 0.0f, 1.0f)); // Clamp
	//}

	//// Convert UVs to pixel coordinates
	//int x = static_cast<int>(uv.x * texWidth) % texWidth;
	//int y = static_cast<int>(uv.y * texHeight) % texHeight;

	//// Fetch texel
	//int index = y * texWidth + x;
	//uint32_t texel = texture[index];

	//// Extract RGB
	//float r = ((texel >> 16) & 0xFF) / 255.0f;
	//float g = ((texel >> 8) & 0xFF) / 255.0f;
	//float b = (texel & 0xFF) / 255.0f;

	//return float3(r, g, b);
}

float3 Tmpl8::Renderer::SampleTexture(float* texture, int texWidth, int texHeight, float2 uv, bool tile)
{
	// Wrap or clamp UV coordinates
	if (tile) {
		uv.x -= floorf(uv.x);
		uv.y -= floorf(uv.y);
	}
	else {
		uv.x = std::clamp(uv.x, 0.0f, 1.0f);
		uv.y = std::clamp(uv.y, 0.0f, 1.0f);
	}

	// Scale to texel space
	float x = uv.x * (texWidth - 1);
	float y = uv.y * (texHeight - 1);

	// **Clamp Again Before Indexing**
	x = std::clamp(x, 0.0f, float(texWidth - 1));
	y = std::clamp(y, 0.0f, float(texHeight - 1));

	// Compute texel indices
	int x0 = static_cast<int>(x);
	int y0 = static_cast<int>(y);
	int x1 = std::min(x0 + 1, texWidth - 1);
	int y1 = std::min(y0 + 1, texHeight - 1);

	// **Debug Checks**
	if (x0 < 0 || x0 >= texWidth || y0 < 0 || y0 >= texHeight ||
		x1 < 0 || x1 >= texWidth || y1 < 0 || y1 >= texHeight) {
		__debugbreak(); // If this triggers, inspect 'uv', 'x', 'y', 'texWidth', and 'texHeight'
	}

	// Load texel values as 32-bit floats (assuming a float4 layout in the HDR texture)
	__m128 texel00 = _mm_load_ps(&texture[(y0 * texWidth + x0) * 4]);
	__m128 texel10 = _mm_load_ps(&texture[(y0 * texWidth + x1) * 4]);
	__m128 texel01 = _mm_load_ps(&texture[(y1 * texWidth + x0) * 4]);
	__m128 texel11 = _mm_load_ps(&texture[(y1 * texWidth + x1) * 4]);

	// Bilinear interpolation weights
	float dx = x - x0;
	float dy = y - y0;

	__m128 w00 = _mm_set1_ps((1.0f - dx) * (1.0f - dy));
	__m128 w10 = _mm_set1_ps(dx * (1.0f - dy));
	__m128 w01 = _mm_set1_ps((1.0f - dx) * dy);
	__m128 w11 = _mm_set1_ps(dx * dy);

	// Compute bilinear interpolation
	__m128 result = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(texel00, w00), _mm_mul_ps(texel10, w10)),
		_mm_add_ps(_mm_mul_ps(texel01, w01), _mm_mul_ps(texel11, w11))
	);

	// Extract the final RGB color
	alignas(16) float temp[4];  // Ensure memory is aligned for SIMD
	_mm_store_ps(temp, result);

	float3 finalColor = { temp[0], temp[1], temp[2] };  // Extract R, G, B
	return finalColor;
}


float2 Tmpl8::Renderer::GetSkyUV(const float3& direction)
{
	float u = atan2f(direction.z, direction.x) * INV2PI + 0.5f;
	float v = acosf(direction.y) * INVPI;
	return float2(u, v);
}


float3 Tmpl8::Renderer::SampleSky(const float3& direction)
{

	float u = (atan2f(direction.z, direction.x) * INV2PI + 0.5f) * skyWidth;
	float v = acosf(direction.y) * INVPI * skyHeight;

	int uIdx = std::clamp(static_cast<int>(u), 0, skyWidth - 1);
	int vIdx = std::clamp(static_cast<int>(v), 0, skyHeight - 1);

	int skyIdx = uIdx + vIdx * skyWidth;

	return 0.65f * float3(skyPixels[skyIdx * 3], skyPixels[skyIdx * 3 + 1], skyPixels[skyIdx * 3 + 2]);
}

float3 Tmpl8::Renderer::CookTorranceBRDF(const float3& lightDirection, const float3& viewDirection, const float3& normal, const float3& fresnelReflectance, float roughness)
{
	// Calculate the halfway vector
	float3 halfwayVector = normalize(viewDirection + lightDirection);

	// Calculate the Distribution GGX
	float D = DistributionGGX(normal, halfwayVector, roughness);

	// Calculate the Fresnel-Schlick term
	float3 F = FresnelSchlick(std::max(dot(halfwayVector, viewDirection), 0.0f), fresnelReflectance);

	// Calculate the Geometry term (using Smith's approximation)
	float G = GeometrySmith(normal, viewDirection, lightDirection, roughness);

	// Dot products between normal and direction vectors
	float NdotV = std::max(dot(normal, viewDirection), 0.0f);
	float NdotL = std::max(dot(normal, lightDirection), 0.0f);

	// Avoid division by zero (use small constant in the denominator)
	float denominator = 4.0f * NdotV * NdotL;
	float3 specular = (D * F * G) / std::max(denominator, 0.0001f);

	return specular;
}

float3 Tmpl8::Renderer::ShadeLambertian(const float3& normal, const float3& lightVector, const float3& albedo, const float3& lightColor, float lightIntensity, float distance) // probably can optimize
{
	float NdotL = std::max(dot(normal, lightVector), 0.0f); // Prevent negative light contribution (cosine between N and L)
	float attenuation = 1.0f / (distance * distance); // Inverse-square falloff

	float3 diffuse = (albedo / PI) * lightColor * lightIntensity * NdotL * attenuation;

	return diffuse;
}

float Tmpl8::Renderer::DistributionGGX(const float3& normal, const float3& halfwayVector, float roughness) // probably can optimize
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = std::max(dot(normal, halfwayVector), 0.0f);
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = PI * denom * denom;

	return a2 / denom;
}

float3 Tmpl8::Renderer::FresnelSchlick(float cosTheta, const float3& fresnelReflectance) // probably can optimize
{
	return fresnelReflectance + (float3(1.0f) - fresnelReflectance) * pow(1.0f - cosTheta, 5.0f);
}

float Tmpl8::Renderer::GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r * r) / 8.0f;
	return NdotV / (NdotV * (1.0f - k) + k);
}

float Tmpl8::Renderer::GeometrySmith(const float3& normal, const float3& viewDirection, const float3& lightVector, float roughness) // I just know this hides the fractals that are obscured by other fractals no clue how it works
{
	float NdotV = std::max(dot(normal, viewDirection), 0.0f);
	float NdotL = std::max(dot(normal, lightVector), 0.0f);
	float ggx1 = GeometrySchlickGGX(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

float Tmpl8::Renderer::ComputeSurfaceSpreadAngle(float3 rayDirection, float3 surfaceNormal)
{
	// Normalize vectors if necessary (optional if they're already normalized)
	rayDirection = normalize(rayDirection);
	surfaceNormal = normalize(surfaceNormal);

	// Calculate the dot product between the ray and the surface normal
	float dotProduct = dot(rayDirection, surfaceNormal);

	// Clamp the dot product to the range [-1, 1] to avoid domain errors
	dotProduct = clamp(dotProduct, -1.0f, 1.0f);

	// Calculate the angle between the ray and the surface normal
	float angle = acos(dotProduct);  // The angle is in radians

	return angle;  // This is the surface spread angle in radians
}

float Tmpl8::Renderer::ComputeTextureLOD(float3 rayDirection, float3 normal, float triangleLODConstant, RayCone cone, float textureWidth, float textureHeight, float3 surfacePoint, float3 cameraPosition)
{
	float lambda = triangleLODConstant;

	// Add pixel spread angle contribution
	lambda += log2f(fmaxf(abs(cone.pixelSpreadAngle), 1e-6f));

	// Account for texture resolution
	lambda += 0.5f * log2f(textureWidth * textureHeight);

	// Account for foreshortening (apply a minimum clamp to avoid extreme LOD drop)
	float cosTheta = fmaxf(abs(dot(normalize(rayDirection), normalize(normal))), 0.2f); // Min threshold to avoid excessive LOD reduction
	lambda -= log2f(cosTheta);


	// Account for foreshortening based on angle between normal and ray
	lambda -= log2f(fmaxf(abs(dot(normalize(rayDirection), normalize(normal))), 1e-6f));

	// *** NEW: Add distance term ***
	float distance = length(surfacePoint - cameraPosition);
	lambda += log2f(distance);  // The farther away, the higher the mip level

	return lambda;
}

float Tmpl8::Renderer::GetTriangleLODConstant(float3 v0, float3 v1, float3 v2, float2 uv0, float2 uv1, float2 uv2)
{
	// Compute world-space triangle area
	float3 edge1 = v1 - v0;
	float3 edge2 = v2 - v0;
	float P_a = length(cross(edge1, edge2));

	// Compute texture-space triangle area
	float2 uvEdge1 = uv1 - uv0;
	float2 uvEdge2 = uv2 - uv0;
	float T_a = fabs(uvEdge1.x * uvEdge2.y - uvEdge1.y * uvEdge2.x);

	// Prevent division by zero or extreme values
	P_a = fmaxf(P_a, 1e-6f);
	T_a = fmaxf(T_a, 1e-6f);

	// Normalize LOD constant
	float lodConstant = 0.5f * log2f(T_a / P_a);
	lodConstant = clamp(lodConstant, -5.0f, 5.0f); // Keep values in a reasonable range

	return lodConstant;
}

// -----------------------------------------------------------
// Update user interface (imgui)
// -----------------------------------------------------------
void Renderer::UI()
{
	// animation toggle
	//ImGui::Checkbox( "Animate scene", &animating );
	// ray query on mouse
	/*Ray r = camera.GetPrimaryRay( (float)mousePos.x, (float)mousePos.y );
	scene.FindNearest( r );
	ImGui::Text( "Object id: %i", r.objIdx );*/
	static bool showHierarchy = true;
	static bool showLights = true;


	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			if (ImGui::MenuItem("Hierarchy", nullptr, showHierarchy)) showHierarchy = !showHierarchy;
			if (ImGui::MenuItem("Lights", nullptr, showLights)) showLights = !showLights;
			ImGui::EndMenu();
		}
		//ImGui::SameLine();
		//ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - 100.0f, 0.0f)); // Adjust 100.0f for space
		//ImGui::Text("Your Text Here");
		ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f); // Adjust position
		ImVec2 mousePos = ImGui::GetMousePos();
		ImGui::Text("Mouse: (%.0f, %.0f)", mousePos.x, mousePos.y);

		ImGui::EndMainMenuBar();
	}

	//ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

	// Dock the Hierarchy window to the left
	if (showHierarchy) {
		//ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
		ImGui::Begin("Hierarchy", &showHierarchy, ImGuiWindowFlags_NoCollapse);

		static std::unordered_map<std::string, bool> popoutWindowStates;

		for (auto& pair : scene.m_renderObjects)
		{
			std::string name = pair.first;
			RenderObject& renderObject = pair.second;

			// Display a button for each render object
			if (ImGui::Button(name.c_str()))
			{
				// Toggle the popout window state when the button is pressed
				popoutWindowStates[name] = !popoutWindowStates[name];
			}

			// Check if the popout window for this render object should be open
			if (popoutWindowStates[name])
			{
				// Create the popout window for the render object
				if (ImGui::Begin(name.c_str(), &popoutWindowStates[name], ImGuiWindowFlags_AlwaysAutoResize))
				{
					// Popout window content
					ImGui::Text("Render Object: %s", name.c_str());

					ImGui::Text("Transform Controls");

					float3 position = renderObject.GetPosition();
					float3 rotation = renderObject.GetRotation();
					float3 scale = renderObject.GetScale();

					// Position input
					if (ImGui::DragFloat3("Position", &position[0], 0.01f, -FLT_MAX, FLT_MAX)) {
						// You can apply the change in position here
						// For example, apply it to the object or store it for further use
						renderObject.SetPosition(position);
					}

					// Rotation input (e.g., for Euler angles)
					if (ImGui::DragFloat3("Rotation", &rotation[0], 0.01f, -FLT_MAX, FLT_MAX)) {
						// You can apply the change in rotation here
						renderObject.SetRotation(rotation);
					}

					// Scale input
					if (ImGui::DragFloat3("Scale", &scale[0], 0.01f, -FLT_MAX, FLT_MAX)) {
						// You can apply the change in scale here
						renderObject.SetScale(scale);
					}

					ImGui::End();
				}
			}
		}

		// Button to trigger the pop-out window
		


		// Show the pop-out window
		
		
		ImGui::End();
	}

	// Dock the Inspector window to the right (next to Hierarchy)
	if (showLights) {
		//ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
		ImGui::Begin("Scene Lighting", &showLights, ImGuiWindowFlags_NoCollapse);
		ImGui::Text("Object Properties:");
		if (ImGui::Checkbox("enable normal maps", &enableNormalMaps))
		{
		}
		if (ImGui::Checkbox("enable MaterialPoints", &enableMaterialPoints))
		{
		}
		if (ImGui::Checkbox("enable MipMapping", &enableMipMapping))
		{
		}
		if (ImGui::Checkbox("view MipMapping", &viewMipMapping))
		{
		}
		/*ImGui::SliderFloat("metallic", &metallic, 0.0f, 1.0f);
		ImGui::SliderFloat("roughness", &roughness, 0.0f, 1.0f);*/
		ImGui::End();

		// Create a checkbox
	}

}

void Tmpl8::Renderer::ImGuiCreateObjectPopout(std::string objectName, bool open)
{
	if (open)
	{
		// Create the pop-out window
		ImGui::Begin(objectName.c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize);

		// Window content
		ImGui::Text("This is a popout window!");

		// Close button
		if (ImGui::Button("Close"))
		{
			open = false;
		}

		ImGui::End();
	}
}

void Tmpl8::Renderer::MouseDown(int button)
{
	if (button == GLFW_MOUSE_BUTTON_2)
	{
		m_rightMouseDown = true;
	}
}
