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

	bool overlaps(const AABB& other) const
	{
		// Check for non-overlap (separation) along the X-axis
		// If the left side of 'this' is past the right side of 'other', OR
		// if the right side of 'this' is past the left side of 'other', they don't overlap.
		if (this->max.x < other.min.x || this->min.x > other.max.x) {
			return false;
		}

		// Check for non-overlap (separation) along the Y-axis
		if (this->max.y < other.min.y || this->min.y > other.max.y) {
			return false;
		}

		// Check for non-overlap (separation) along the Z-axis
		if (this->max.z < other.min.z || this->min.z > other.max.z) {
			return false;
		}

		// If we haven't returned false, it means the boxes overlap on all three axes.
		return true;
	}
};
