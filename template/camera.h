#pragma once

namespace Tmpl8 {
class Camera
{
public:
	Camera()
	{
		// setup a basic view frustum
		camPos = float3( 0, 0, -2 );
		camTarget = float3( 0, 0, -1 );
		topLeft = float3( -aspect, 1, 0 );
		topRight = float3( aspect, 1, 0 );
		bottomLeft = float3( -aspect, -1, 0 );
	}
	Ray GetPrimaryRay( const float x, const float y )
	{
		// calculate pixel position on virtual screen plane
		const float u = (float)x * (1.0f / SCRWIDTH);
		const float v = (float)y * (1.0f / SCRHEIGHT);
		const float3 P = topLeft + u * (topRight - topLeft) + v * (bottomLeft - topLeft);
		return Ray( camPos, normalize( P - camPos ) );
	}
	bool HandleInput( const float t )
	{
		if (!WindowHasFocus()) return false;
		float speed = 0.0025f * t;
		float3 ahead = normalize( camTarget - camPos );
		float3 tmpUp( 0, 1, 0 );
		float3 right = normalize( cross( tmpUp, ahead ) );
		float3 up = normalize( cross( ahead, right ) );
		bool changed = false;
		if (IsKeyDown( GLFW_KEY_A )) camPos -= speed * 2 * right, changed = true;
		if (IsKeyDown( GLFW_KEY_D )) camPos += speed * 2 * right, changed = true;
		if (IsKeyDown( GLFW_KEY_W )) camPos += speed * 2 * ahead, changed = true;
		if (IsKeyDown( GLFW_KEY_S )) camPos -= speed * 2 * ahead, changed = true;
		if (IsKeyDown( GLFW_KEY_R )) camPos += speed * 2 * up, changed = true;
		if (IsKeyDown( GLFW_KEY_F )) camPos -= speed * 2 * up, changed = true;
		camTarget = camPos + ahead;
		if (IsKeyDown( GLFW_KEY_UP )) camTarget -= speed * up, changed = true;
		if (IsKeyDown( GLFW_KEY_DOWN )) camTarget += speed * up, changed = true;
		if (IsKeyDown( GLFW_KEY_LEFT )) camTarget -= speed * right, changed = true;
		if (IsKeyDown( GLFW_KEY_RIGHT )) camTarget += speed * right, changed = true;
		if (!changed) return false;
		ahead = normalize( camTarget - camPos );
		up = normalize( cross( ahead, right ) );
		right = normalize( cross( up, ahead ) );
		topLeft = camPos + ahead * 2.0f - aspect * right + up;
		topRight = camPos + ahead * 2.0f + aspect * right + up;
		bottomLeft = camPos + ahead * 2.0f - aspect * right - up;
		return true;
	}
	float pixelSpreadAngle() // when we our camera position get closer to the plane that the pixel is on. The pixel which acts like a window into the world gets larger and we need to caclulate that spread
	{

		// Compute pixel size in world space
		float3 dX = (topRight - topLeft) / SCRWIDTH;
		float3 dY = (bottomLeft - topLeft) / SCRHEIGHT;

		// Approximate pixel spread distance
		float spreadX = length(dX);
		float spreadY = length(dY);
		float pixelSize = fmaxf(spreadX, spreadY);

		// Compute camera distance to the image plane
		float3 camToPlane = (topLeft + bottomLeft + topRight) / 3.0f - camPos;
		float distance = length(camToPlane);

		// Compute the angular spread using small-angle approximation: theta ≈ pixelSize / distance
		return atanf(pixelSize / distance);
	}
	float aspect = (float)SCRWIDTH / (float)SCRHEIGHT;
	float3 camPos, camTarget;
	float3 topLeft, topRight, bottomLeft;
};
}