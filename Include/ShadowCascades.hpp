#pragma once
#include <glm.hpp>
#include <vector>

class ShadowCascades
{
public:
	static constexpr uint32_t NUM_CASCADES = 4;

	struct CascadeData
	{
		glm::mat4 viewProj;
		float nearDepth;
		float farDepth;
	};

	void updateCascades(
		const glm::mat4& cameraView,
		const glm::mat4& cameraProj,
		const glm::vec3& lightDir,
		float nearPlane,
		float farPlane,
		float lambda = 0.95f
	);

	const std::vector<CascadeData>& getCascades() const { return m_cascades; }

private:
	std::vector<CascadeData> m_cascades;
	std::vector<float> m_splitDepths;

	void calculateSplitDepths(float near, float far, float lambda);
	glm::mat4 calculateLightMatrix(const std::vector<glm::vec3>& frustumCorners, const glm::vec3& lightDir);
	std::vector<glm::vec3> getFrustumCornersWorldSpace(const glm::mat4& viewProj);
};