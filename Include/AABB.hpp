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
		// optimized AABB transform
		glm::vec3 newCenter = glm::vec3(matrix * glm::vec4(center(), 1.0f));

		// Abs of upper 3x3 matrix
		glm::mat3 absM = glm::mat3(
			glm::abs(glm::vec3(matrix[0])),
			glm::abs(glm::vec3(matrix[1])),
			glm::abs(glm::vec3(matrix[2]))
		);

		glm::vec3 halfSize = (max - min) * 0.5f;
		glm::vec3 newExtents = absM * halfSize;

		AABB result;
		result.min = newCenter - newExtents;
		result.max = newCenter + newExtents;
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
