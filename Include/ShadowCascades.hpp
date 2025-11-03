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
		const glm::vec3& camPos,
		const glm::vec3& camFront,
		const glm::vec3& camUp,
		const glm::vec3& camRight,
		float fov,
		float aspect,
		const glm::vec3& lightDir,
		float nearPlane,
		float farPlane,
		float lambda
	);

	const std::vector<CascadeData>& getCascades() const { return m_cascades; }

private:
	std::vector<CascadeData> m_cascades;
	std::vector<float> m_splitDepths;

	void calculateSplitDepths(float near, float far, float lambda);
	glm::mat4 calculateLightMatrix(const std::vector<glm::vec3>& frustumCorners, const glm::vec3& lightDirNormalized);
	std::vector<glm::vec3> getCascadeFrustumCorners(
		const glm::vec3& camPos,
		const glm::vec3& camFront,
		const glm::vec3& camUp,
		const glm::vec3& camRight,
		float fov,
		float aspect,
		float nearPlane,
		float farPlane);
};
