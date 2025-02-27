#include "precomp.h"

// -----------------------------------------------------------
// Initialize the renderer
// -----------------------------------------------------------
void Renderer::Init()
{
	// create fp32 rgb pixel buffer to render to
	accumulator = (float4*)MALLOC64( SCRWIDTH * SCRHEIGHT * 16 );
	memset( accumulator, 0, SCRWIDTH * SCRHEIGHT * 16 );
}

// -----------------------------------------------------------
// Evaluate light transport
// -----------------------------------------------------------
float3 Renderer::Trace( Ray& ray )
{

	const float MAX_DISTANCE = 1000.0f; // Set this to the maximum distance you expect

	tinybvh::Ray bvh_ray(ray.O, normalize(ray.D)); // Ensure ray direction is normalized
	scene.tlas.Intersect(bvh_ray) && bvh_ray.hit.t > 0.0f;


	if (bvh_ray.hit.t < BVH_FAR) // Check if there is a hit
	{
		
		uint32_t modelIndex = scene.instances[bvh_ray.hit.inst].blasIdx;

		float3 intersection = ray.O + bvh_ray.hit.t * ray.D;

		// Calculate barycentric coordinates
		float u = bvh_ray.hit.u;
		float v = bvh_ray.hit.v;

		float w = 1.0f - u - v;

		uint32_t index = bvh_ray.hit.prim;
		float3 normal1 = scene.models[modelIndex]->m_vertices[index * 3].normal;
		float3 normal2 = scene.models[modelIndex]->m_vertices[index * 3 + 1].normal;
		float3 normal3 = scene.models[modelIndex]->m_vertices[index * 3 + 2].normal;
		float3 smoothNormal = float3((w * normal1) + (u * normal2) + (v * normal3));


		// Final color: make it grayscale based on the reversed brightness
		//float3 finalColor = float3(brightness, brightness, brightness);
		return (smoothNormal + 1) * 0.5f;
	}

	return float3(0, 0, 0); // Return black if no intersection



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

	RenderObject& monkey = scene.m_renderObjects["Monkey"];
	float3 position = monkey.GetPosition();
	position.x += 0.001f * deltaTime;
	monkey.SetPosition(position);
	float3 rotation = monkey.GetRotation ();
	rotation.y += 0.001f * deltaTime;
	monkey.SetRotation(rotation);


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

void Tmpl8::Renderer::ComputePointLights()
{
}

void Tmpl8::Renderer::ComputeDirectionalLights()
{
}

void Tmpl8::Renderer::ComputeSpotLights()
{
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
		ImGui::End();
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
