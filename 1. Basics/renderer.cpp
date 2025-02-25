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

	if (scene.model.m_bvh.Intersect(bvh_ray) && bvh_ray.hit.t > 0.0f) // Check if there is a hit
	{
		float3 intersection = ray.O + bvh_ray.hit.t * ray.D;
		float l = length(intersection - ray.O); // Calculate distance to intersection

		// Reverse the brightness: closer = brighter, farther = darker
		float brightness = 1.0f - min(l / MAX_DISTANCE, 1.0f); // Clamp to [0, 1] range

		// Calculate barycentric coordinates
		float u = bvh_ray.hit.u;
		float v = bvh_ray.hit.v;

		float w = 1.0f - u - v;



		uint32_t index = bvh_ray.hit.prim;
		float3 normal1 = scene.model.m_vertices[index * 3].normal;
		float3 normal2 = scene.model.m_vertices[index * 3 + 1].normal;
		float3 normal3 = scene.model.m_vertices[index * 3 + 2].normal;
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
// -----------------------------------------------------------
void Renderer::Tick( float deltaTime )
{
	// animation
	if (animating) scene.SetTime( anim_time += deltaTime * 0.002f );
	// pixel loop
	Timer t;
	// lines are executed as OpenMP parallel tasks (disabled in DEBUG)
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
	Ray r = camera.GetPrimaryRay( (float)mousePos.x, (float)mousePos.y );
	scene.FindNearest( r );
	ImGui::Text( "Object id: %i", r.objIdx );
}