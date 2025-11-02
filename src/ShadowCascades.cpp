#include "ShadowCascades.hpp"
#include <algorithm>
#include <limits>
#include <cmath>
#include "gtc/matrix_transform.hpp"

void ShadowCascades::calculateSplitDepths(float near, float far, float lambda)
{
	m_splitDepths.resize(NUM_CASCADES);

	float clipRange = far - near;
	float minZ = near;
	float maxZ = near + clipRange;
	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	for (uint32_t i = 0; i < NUM_CASCADES; ++i)
	{
		float p = (i + 1) / static_cast<float>(NUM_CASCADES);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = lambda * (log - uniform) + uniform;
		m_splitDepths[i] = (d - near) / clipRange;
	}
}

std::vector<glm::vec3> ShadowCascades::getFrustumCornersWorldSpace(const glm::mat4& viewProj)
{
	std::vector<glm::vec3> corners;
	corners.reserve(8);

	glm::mat4 inv = glm::inverse(viewProj);

	for (uint32_t x = 0; x < 2; ++x)
	{
		for (uint32_t y = 0; y < 2; ++y)
		{
			for (uint32_t z = 0; z < 2; ++z)
			{
				glm::vec4 pt = inv * glm::vec4(
					2.0f * x - 1.0f,
					2.0f * y - 1.0f,
					2.0f * z - 1.0f,
					1.0f
				);
				corners.push_back(glm::vec3(pt) / pt.w);
			}
		}
	}

	return corners;
}

glm::mat4 ShadowCascades::calculateLightMatrix(const std::vector<glm::vec3>& frustumCorners, const glm::vec3& lightDirNormalized)
{
	// Computes light-space view-projection matrix for a given camera frustum split.
	// Ensures stable texel alignment and dynamically adjusts light position to minimize shimmering.

    // Compute frustum center
    glm::vec3 center(0.0f);
    for (const auto& v : frustumCorners) center += v;
    center /= static_cast<float>(frustumCorners.size());

    constexpr float LIGHT_OFFSET = 50.0f;

    // First pass: rough lightView to estimate bounds
    glm::vec3 tempLightPos = center - lightDirNormalized * (LIGHT_OFFSET + 100.0f);
    glm::mat4 tempLightView = glm::lookAt(tempLightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 minLS(std::numeric_limits<float>::infinity());
    glm::vec3 maxLS(-std::numeric_limits<float>::infinity());
    for (const auto& v : frustumCorners)
    {
        glm::vec4 vLS4 = tempLightView * glm::vec4(v, 1.0f);
        glm::vec3 vLS = glm::vec3(vLS4) / vLS4.w;
        minLS = glm::min(minLS, vLS);
        maxLS = glm::max(maxLS, vLS);
    }

    // Dynamic light position based on cascade size
    float depthOffset = glm::length(maxLS - minLS) * 0.5f + LIGHT_OFFSET;
    glm::vec3 lightPos = center - lightDirNormalized * depthOffset;
    glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

    // Recompute AABB in light space (accurate)
    minLS = glm::vec3(std::numeric_limits<float>::infinity());
    maxLS = glm::vec3(-std::numeric_limits<float>::infinity());
    for (const auto& v : frustumCorners)
    {
        glm::vec4 vLS4 = lightView * glm::vec4(v, 1.0f);
        glm::vec3 vLS = glm::vec3(vLS4) / vLS4.w;
        minLS = glm::min(minLS, vLS);
        maxLS = glm::max(maxLS, vLS);
    }

    // Padding and stability
    constexpr float Z_NEAR_PADDING = 10.0f;
    constexpr float Z_FAR_EXTRUSION = 500.0f;
    minLS.z -= Z_NEAR_PADDING;
    maxLS.z += Z_FAR_EXTRUSION;

    float width = std::max(maxLS.x - minLS.x, 1e-3f);
    float height = std::max(maxLS.y - minLS.y, 1e-3f);

    // Texel snapping
    const float SHADOW_MAP_RESOLUTION = 2048.0f;
    float worldUnitsPerTexel = std::max(width, height) / SHADOW_MAP_RESOLUTION;
    worldUnitsPerTexel = std::max(worldUnitsPerTexel, 1e-6f);

    minLS.x = std::floor(minLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    minLS.y = std::floor(minLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxLS.x = std::ceil(maxLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxLS.y = std::ceil(maxLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    minLS.z = std::min(minLS.z, 0.0f);

    glm::mat4 lightProj = glm::ortho(minLS.x, maxLS.x, minLS.y, maxLS.y, minLS.z, maxLS.z);
    lightProj[1][1] *= -1; // Vulkan Y-flip

    return lightProj * lightView;
}


void ShadowCascades::updateCascades(const glm::mat4& cameraView, const glm::mat4& cameraProj, const glm::vec3& lightDir, float nearPlane, float farPlane, float lambda)
{
	// Updates all cascade split depths and computes their corresponding light-space matrices.

	calculateSplitDepths(nearPlane, farPlane, lambda);
	m_cascades.resize(NUM_CASCADES);
	glm::vec3 lightDirNormalized = glm::normalize(lightDir);

	float lastSplitDist = 0.0f;
	for (uint32_t i = 0; i < NUM_CASCADES; ++i)
	{
		float splitDist = m_splitDepths[i];

		float cascadeNear = nearPlane + lastSplitDist * (farPlane - nearPlane);
		float cascadeFar = nearPlane + splitDist * (farPlane - nearPlane);

		// Create sub-frstum projection
		glm::mat4 cascadeProj = cameraProj;
		cascadeProj[2][2] = (cascadeFar + cascadeNear) / (cascadeNear - cascadeFar);
		cascadeProj[2][3] = (2.0f * cascadeFar * cascadeNear) / (cascadeNear - cascadeFar);

		glm::mat4 cascadeViewProj = cascadeProj * cameraView;
		std::vector<glm::vec3> frustumCorners = getFrustumCornersWorldSpace(cascadeViewProj);

		m_cascades[i].viewProj = calculateLightMatrix(frustumCorners, lightDirNormalized);
		m_cascades[i].nearDepth = cascadeNear;
		m_cascades[i].farDepth = cascadeFar;

		lastSplitDist = splitDist;
	}
}
