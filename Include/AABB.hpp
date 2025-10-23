#pragma once
#include <glm.hpp>

struct AABB
{
	glm::vec3 min{ FLT_MAX };
	glm::vec3 max{ -FLT_MAX };

	void expand(const glm::vec3& point)
	{
		min = glm::min(min, point);
		max = glm::max(max, point);
	}

	AABB transform(const glm::mat4& matrix) const
	{
		// Transform all 8 corners and recomupute bounds
		AABB result;
		glm::vec3 corners[8] = {
			{min.x, min.y, min.z},
			{max.x, min.y, min.z},
			{min.x, max.y, min.z},
			{max.x, max.y, min.z},
			{min.x, min.y, max.z},
			{max.x, min.y, max.z},
			{min.x, max.y, max.z},
			{max.x, max.y, max.z}
		};

		for (const glm::vec3& corner : corners)
		{
			glm::vec3 transformed = glm::vec3(matrix * glm::vec4(corner, 1.0f));
			result.expand(transformed);
		}
		return result;
	}

	glm::vec3 center() const
	{
		return (min + max) * 0.5f;
	}

	float radius() const 
	{
		return glm::length(max - min) * 0.5f;
	}
};
