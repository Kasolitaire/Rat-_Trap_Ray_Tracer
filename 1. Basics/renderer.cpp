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

		if (modelIndex == 0) 
		{
			int x = 0;
		}

		if (modelIndex == 1)
		{
			int x = 1;
		}

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

// -----------------------------------------------------------
// Update user interface (imgui)
// -----------------------------------------------------------
void Renderer::UI()
{
	// animation toggle
	ImGui::Checkbox( "Animate scene", &animating );
	// ray query on mouse
	/*Ray r = camera.GetPrimaryRay( (float)mousePos.x, (float)mousePos.y );
	scene.FindNearest( r );
	ImGui::Text( "Object id: %i", r.objIdx );*/
}