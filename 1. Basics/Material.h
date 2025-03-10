#pragma once

enum class MaterialType {
    Diffuse,
    Reflective,
    Glossy,
    Metal,
    Dielectric,
    Emissive
};

struct Material {
private:
    float3 albedo = float3(1.0f);  // Ensuring valid color range
    float roughness = 0.0f;
    float metallic = 0.0f;
    float IOR = 1.5f; // Default to glass
    float3 emission = float3(0.0f);
    MaterialType type;
public:

    MaterialType getType() const { return type; }
    void setType(MaterialType value) { type = value; }

    // Getter & Setter for Albedo (Clamp to [0,1])
    float3 getAlbedo() const { return albedo; }
    void setAlbedo(const float3& value) 
    {
        albedo.x = std::clamp(value.x, 0.0f, 1.0f);
        albedo.y = std::clamp(value.y, 0.0f, 1.0f);
        albedo.z = std::clamp(value.z, 0.0f, 1.0f);
    }

    // Getter & Setter for Roughness (Clamp to [0,1])
    float getRoughness() const { return roughness; }
    void setRoughness(float value) { roughness = std::clamp(value, 0.0f, 1.0f); }

    // Getter & Setter for Metallic (Clamp to [0,1])
    float getMetallic() const { return metallic; }
    void setMetallic(float value) { metallic = std::clamp(value, 0.0f, 1.0f); }

    // Getter & Setter for IOR (Ensure it's > 1 for refraction)
    float getIOR() const { return IOR; }
    void setIOR(float value) { IOR = std::max(1.0f, value); }

    // Getter & Setter for Emission
    float3 getEmission() const { return emission; }
    void setEmission(const float3& value) { emission = value; }

    // Utility Functions
    bool isReflective() const { return type == MaterialType::Reflective || (metallic > 0.99f && roughness == 0.0f); }
    bool isEmissive() const { return length(emission) > 0.0f; }
};
