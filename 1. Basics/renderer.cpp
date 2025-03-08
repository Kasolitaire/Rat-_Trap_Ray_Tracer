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
float3 Renderer::Trace( Ray& ray )
{

	float3 finalColor = float3(0);

	tinybvh::Ray bvh_ray(ray.O, normalize(ray.D)); // Ensure ray direction is normalized
	scene.tlas.Intersect(bvh_ray) && bvh_ray.hit.t > 0.0f;

	if (bvh_ray.hit.t < BVH_FAR) // Check if there is a hit
	{
		
		uint32_t instanceIndex = bvh_ray.hit.inst;
		tinybvh::BLASInstance& instance = scene.instances[instanceIndex];
		uint32_t modelIndex = instance.blasIdx;
		Model& model = *scene.models[modelIndex];


		mat4 modelMatrix = *reinterpret_cast<mat4*>(&instance.transform);
		mat4 inverseMatrix = *reinterpret_cast<mat4*>(&instance.invTransform);

		float3 intersection = ray.O + bvh_ray.hit.t * ray.D;

		// Calculate barycentric coordinates
		float u = bvh_ray.hit.u;
		float v = bvh_ray.hit.v;
		float w = 1.0f - u - v;

		uint32_t index = bvh_ray.hit.prim;

		Vertex vertex1 = scene.models[modelIndex]->m_vertices[index * 3];
		Vertex vertex2 = scene.models[modelIndex]->m_vertices[index * 3 + 1];
		Vertex vertex3 = scene.models[modelIndex]->m_vertices[index * 3 + 2];

		float3 normal1 = vertex1.normal;
		float3 normal2 = vertex2.normal;
		float3 normal3 = vertex3.normal;

		float3 interpolatedNormal = (w * normal1) + (u * normal2) + (v * normal3);


		//Interpolation can cause slight skewing, so re - orthogonalizing the tangent space helps :

		//cpp
		//	Copy
		//	Edit
		//	T = normalize(T - dot(T, N) * N); // Make T orthogonal to N
		//B = cross(N, T);                  // Recompute B to ensure orthogonality

		// Transform normal correctly
		interpolatedNormal = (float3(inverseMatrix.Transposed() * float4(interpolatedNormal, 0.0f)));


		

		//mat4 TBN =  
		/*float2 position1 = float2(scene.models[modelIndex]->m_vertices[index * 3].texCoords);
		float2 position2 = float2(scene.models[modelIndex]->m_vertices[index * 3 + 1].position);
		float2 position3 = float2(scene.models[modelIndex]->m_vertices[index * 3 + 2].position);*/

		uint* normalTexture = nullptr;
		float2 normalDimensions;

		float2 interpolatedUVCoords = InterpolateUV(vertex1.texCoords, vertex2.texCoords, vertex3.texCoords, float3(w, u, v));
		float3 albedo = float3(1.f);

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
					float3 T = normalize(float3(vertex1.tangent * w + vertex2.tangent * u + vertex3.tangent * v));
					float3 B = normalize(float3(vertex1.bitangent * w + vertex2.bitangent * u + vertex3.bitangent * v));
					T = normalize(T - dot(T, interpolatedNormal) * interpolatedNormal); // Make T orthogonal to N
					B = cross(interpolatedNormal, T);
					B *= -1.0f;

					mat4 TBN;
					TBN[0] = T.x;  TBN[1] = B.x;  TBN[2] = interpolatedNormal.x;  TBN[3] = 0.f;
					TBN[4] = T.y;  TBN[5] = B.y;  TBN[6] = interpolatedNormal.y;  TBN[7] = 0.f;
					TBN[8] = T.z;  TBN[9] = B.z;  TBN[10] = interpolatedNormal.z;  TBN[11] = 0.f;
					TBN[12] = 0.f;  TBN[13] = 0.f;  TBN[14] = 0.f;                  TBN[15] = 1.f; // Identity row

					uint* normalTexture = model.m_textures[textureKey];
					float3 normalSample = SampleTexture(normalTexture, textureData.dimensions.x, textureData.dimensions.y, interpolatedUVCoords, true);
					float3 N_tangent = normalSample * 2.0f - 1.0f; // Convert from [0,1] to [-1,1]
					interpolatedNormal = normalize(TBN * float4(normalSample, 0.f));
					//float3 sampledNormal = SampleTexture
				}
			}

		}

		std::string name = model.m_name;

		if (scene.m_pointLights.enabled && scene.m_pointLights.positions.size()) 
		{
			finalColor += ComputePointLights(interpolatedNormal, intersection);
		}

		if (scene.m_directionalLights.enabled && scene.m_directionalLights.directions.size())
		{
			finalColor += ComputeDirectionalLights(interpolatedNormal, intersection);
		}


		//return albedo;
		return (interpolatedNormal + 1) * 0.5f;
		//return finalColor * albedo;
	}
	else
	{
		float u = (atan2f(ray.D.z, ray.D.x) * INV2PI + 0.5f) * skyWidth;
		float v = acosf(ray.D.y) * INVPI * skyHeight;

		int uIdx = std::clamp(static_cast<int>(u), 0, skyWidth - 1);
		int vIdx = std::clamp(static_cast<int>(v), 0, skyHeight - 1);

		int skyIdx = uIdx + vIdx * skyWidth;

		return 0.65f * float3(skyPixels[skyIdx * 3], skyPixels[skyIdx * 3 + 1], skyPixels[skyIdx * 3 + 2]);
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
			float4 pixel = float4( Trace( camera.GetPrimaryRay( (float)x, (float)y ) ), 0 );
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

float3 Tmpl8::Renderer::ComputePointLights(const float3 normal, const float3 intersection)
{
	float3 finalColor = float3(0);

	for (unsigned int index = 0; index < scene.m_pointLights.positions.size(); index++)
	{

		float3 lightPosition = scene.m_pointLights.positions[index];
		float3 lightVector = lightPosition - intersection;
		float distance = length(lightVector); // Correct distance before normalization
		float3 lightDirection = lightVector / distance; // Normalized light direction

		float cosa = max(0.0f, dot(normal, normalize(lightDirection)));

		float3 newOrigin = intersection + normal * EPSILON;
		tinybvh::Ray shadowRay(newOrigin, lightDirection, distance);
		if (!scene.tlas.IsOccludedTLAS(shadowRay))
		{
			finalColor += scene.m_pointLights.colors[index] * scene.m_pointLights.intensities[index] * (1 / (distance * distance)) * cosa;
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

	// Convert UVs to pixel coordinates
	int x = static_cast<int>(uv.x * texWidth) % texWidth;
	int y = static_cast<int>(uv.y * texHeight) % texHeight;

	// Fetch texel
	int index = y * texWidth + x;
	uint32_t texel = texture[index];

	// Extract RGB
	float r = ((texel >> 16) & 0xFF) / 255.0f;
	float g = ((texel >> 8) & 0xFF) / 255.0f;
	float b = (texel & 0xFF) / 255.0f;

	return float3(r, g, b);
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
