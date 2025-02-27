#pragma once
class Model;
class RenderObject
{
public:
	RenderObject() = default;
	RenderObject(unsigned int renderDataIndex) : m_renderDataIndex(renderDataIndex) {};
	mat4  GetModelMatrix() 
	{
		mat4 modelMatrix = mat4::Identity();
		modelMatrix = modelMatrix * mat4::Translate(m_position);
		modelMatrix = modelMatrix * mat4::RotateX(m_rotation.x);
		modelMatrix = modelMatrix * mat4::RotateY(m_rotation.y);
		modelMatrix = modelMatrix * mat4::RotateZ(m_rotation.z);
		modelMatrix = modelMatrix * mat4::Scale(m_scale);
		
		return modelMatrix;
	};

	float3 GetPosition() { return m_position; }
	void SetPosition(float3 position) { m_position = position; isDirty = true; }

	float3 GetRotation() { return m_rotation; }
	void SetRotation(float3 rotation) { m_rotation = rotation; isDirty = true; }

	float3 GetScale() { return m_scale; }
	void SetScale(float3 scale) { m_scale = scale; isDirty = true; }

	void MarkAsClean() { isDirty = false; }
	bool IsDirty() { return isDirty; }

	unsigned int GetRenderDataIndex() { return m_renderDataIndex; }
private:
	unsigned int m_renderDataIndex = 0;
	float3 m_position = { 0, 0, 0 };
	float3 m_rotation = { 0, 0, 0 }; // euler for now
	float3 m_scale = { 1, 1, 1 };
	bool isDirty = false;
};

