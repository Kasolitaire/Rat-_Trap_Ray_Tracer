#include "precomp.h"
#include "../lib/stb_image.h"

// -----------------------------------------------------------
// Initialize the renderer
// -----------------------------------------------------------
static bool enableNormalMaps = true;

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

		//interpolatedNormal = (float3(inverseMatrix.Transposed() * float4(interpolatedNormal, 0.0f)));

		float2 interpolatedUVCoords = InterpolateUV(vertex1.texCoords, vertex2.texCoords, vertex3.texCoords, float3(w, u, v));
		float3 albedo = float3(1.f);

		// make this cleaner
		if (model.m_textures.size())
		{
			for (TextureData& textureData : model.m_meshes[vertex1.meshIndex].textures)
			{
				std::string textureKey = textureData.path;
				TextureType type = textureData.type;

				if (type == TextureType::Diffuse)
				{
					uint* diffuseTexture = model.m_textures[textureKey];
					albedo = SampleTexture(diffuseTexture, textureData.dimensions.x, textureData.dimensions.y, interpolatedUVCoords, true);
				}

				if (type == TextureType::Normal && enableNormalMaps)
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

					uint* normalTexture = model.m_textures[textureKey];
					float3 normalSample = SampleTexture(normalTexture, textureData.dimensions.x, textureData.dimensions.y, interpolatedUVCoords, true);
					float3 N_tangent = normalSample * 2.0f - 1.0f; // Convert from [0,1] to [-1,1]

					// Transform normal using the upper-left 3×3 portion of TBN
					interpolatedNormal = normalize(float3(
						TBN[0] * N_tangent.x + TBN[1] * N_tangent.y + TBN[2] * N_tangent.z,
						TBN[4] * N_tangent.x + TBN[5] * N_tangent.y + TBN[6] * N_tangent.z,
						TBN[8] * N_tangent.x + TBN[9] * N_tangent.y + TBN[10] * N_tangent.z
					));
				}
			}
		}

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
				finalColor += ComputePointLights(interpolatedNormal, intersection, albedo, -bvh_ray.D);
			}

			if (scene.m_directionalLights.enabled && scene.m_directionalLights.directions.size())
			{
				finalColor += ComputeDirectionalLights(interpolatedNormal, intersection);
			}

			//return albedo;
			//return (interpolatedNormal + 1) * 0.5f;
			//finalColor *= material.getAlbedo();
			//finalColor *= albedo;
			return finalColor;
		}
	}
	else
	{
		return SampleSky(bvh_ray.D);
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
	// animation
	//if (animating) scene.SetTime( anim_time += deltaTime * 0.002f );
	// pixel loop
	Timer t;
	// lines are executed as OpenMP parallel tasks (disabled in DEBUG)



	scene.Update();

#pragma omp parallel for schedule(dynamic)
	for (int y = 0; y < SCRHEIGHT; y++)
	{
		// trace a primary ray for each pixel on the line
		for (int x = 0; x < SCRWIDTH; x++)
		{
			Ray primaryRay(camera.GetPrimaryRay((float)x, (float)y));
			float4 pixel = float4( Trace( primaryRay ), 0 );
			// translate accumulator contents to rgb32 pixels
			screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8( &pixel );
			accumulator[x + y * SCRWIDTH] = pixel;
		}
	}
	// performance report - running average - ms, MRays/s
	static float avg = 10, alpha = 1;
	avg = (1 - alpha) * avg + alpha * t.elapsed() * 1000;
	if (alpha > 0.05f) alpha *= 0.5f;
	float fps = 1000.0f / avg, rps = (SCRWIDTH * SCRHEIGHT) / avg;
	printf( "%5.2fms (%.1ffps) - %.1fMrays/s\n", avg, fps, rps / 1000 );
	// handle user input
	camera.HandleInput( deltaTime );
}

static float metallic = 0.1f;
static float roughness = 0.2f;

float3 Tmpl8::Renderer::ComputePointLights(const float3 normal, const float3 intersection, float3 albedo, float3 viewDirection)
{
	float3 finalColor = float3(0);
	//albedo = float3(1, 0, 0);

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
    // Handle UV wrapping or clamping
    if (tile) {
        uv = float2(uv.x - floor(uv.x), uv.y - floor(uv.y)); // Wrap
    }
    else {
        uv = float2(std::clamp(uv.x, 0.0f, 1.0f), std::clamp(uv.y, 0.0f, 1.0f)); // Clamp
    }

    // Scale UV coordinates to texel space
    float x = uv.x * (texWidth - 1);
    float y = uv.y * (texHeight - 1);

    // Compute integer texel positions
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = std::min(x0 + 1, texWidth - 1);
    int y1 = std::min(y0 + 1, texHeight - 1);

    // Compute interpolation factors
    float dx = x - x0;
    float dy = y - y0;

    // Fetch the four neighboring texels
    uint32_t texel00 = texture[y0 * texWidth + x0];
    uint32_t texel10 = texture[y0 * texWidth + x1];
    uint32_t texel01 = texture[y1 * texWidth + x0];
    uint32_t texel11 = texture[y1 * texWidth + x1];

    // Extract RGB components
    auto UnpackColor = [](uint32_t texel) -> float3 {
        return float3(
            ((texel >> 16) & 0xFF) / 255.0f,
            ((texel >> 8) & 0xFF) / 255.0f,
            (texel & 0xFF) / 255.0f
        );
    };

    float3 T00 = UnpackColor(texel00);
    float3 T10 = UnpackColor(texel10);
    float3 T01 = UnpackColor(texel01);
    float3 T11 = UnpackColor(texel11);

    // Bilinear interpolation
    float3 L0 = (1.0f - dx) * T00 + dx * T10;
    float3 L1 = (1.0f - dx) * T01 + dx * T11;
    float3 finalColor = (1.0f - dy) * L0 + dy * L1;

    return finalColor;


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
		ImGui::SliderFloat("metallic", &metallic, 0.0f, 1.0f);
		ImGui::SliderFloat("roughness", &roughness, 0.0f, 1.0f);
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
