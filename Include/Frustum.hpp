#pragma once
#include <glm.hpp>
#include <array>

struct Plane
{
	glm::vec3 normal;
	float distance;

	float distanceToPoint(const glm::vec3& point) const
	{
		return glm::dot(normal, point) + distance;
	}
};

class Frustum
{
public:
	std::array<Plane, 6> planes;

	void update(const glm::mat4& viewProj)
	{
        // Extract frustum planes from view-projection matrix
        // Left plane
        planes[0].normal = glm::vec3(viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0]);
        planes[0].distance = viewProj[3][3] + viewProj[3][0];

        // Right plane
        planes[1].normal = glm::vec3(viewProj[0][3] - viewProj[0][0],
            viewProj[1][3] - viewProj[1][0],
            viewProj[2][3] - viewProj[2][0]);
        planes[1].distance = viewProj[3][3] - viewProj[3][0];

        // Bottom plane
        planes[2].normal = glm::vec3(viewProj[0][3] + viewProj[0][1],
            viewProj[1][3] + viewProj[1][1],
            viewProj[2][3] + viewProj[2][1]);
        planes[2].distance = viewProj[3][3] + viewProj[3][1];

        // Top plane
        planes[3].normal = glm::vec3(viewProj[0][3] - viewProj[0][1],
            viewProj[1][3] - viewProj[1][1],
            viewProj[2][3] - viewProj[2][1]);
        planes[3].distance = viewProj[3][3] - viewProj[3][1];

        // Near plane
        planes[4].normal = glm::vec3(viewProj[0][3] + viewProj[0][2],
            viewProj[1][3] + viewProj[1][2],
            viewProj[2][3] + viewProj[2][2]);
        planes[4].distance = viewProj[3][3] + viewProj[3][2];

        // Far plane
        planes[5].normal = glm::vec3(viewProj[0][3] - viewProj[0][2],
            viewProj[1][3] - viewProj[1][2],
            viewProj[2][3] - viewProj[2][2]);
        planes[5].distance = viewProj[3][3] - viewProj[3][2];

        // Normalize all planes
        for (auto& plane : planes)
        {
            float length = glm::length(plane.normal);
            plane.normal /= length;
            plane.distance /= length;
        }
	}

    bool isBoxVisible(const glm::vec3& minBounds, const glm::vec3& maxBounds) const
    {
        for (const Plane& plane : planes)
        {
            // Get positive vertex (vertex closest to plane normal)
            glm::vec3 positiveVertex;
            positiveVertex.x = (plane.normal.x >= 0.0f) ? maxBounds.x : minBounds.x;
            positiveVertex.y = (plane.normal.y >= 0.0f) ? maxBounds.y : minBounds.y;
            positiveVertex.z = (plane.normal.z >= 0.0f) ? maxBounds.z : minBounds.z;

            // If positive vertex is outside, the box is outside
            if (plane.distanceToPoint(positiveVertex) < 0.0f) 
            {
                return false;
            }
        }
        return true;
    }

    bool isSphereVisible(const glm::vec3& center, float radius) const
    {
        for (const auto& plane : planes) 
        {
            if (plane.distanceToPoint(center) < -radius)
            {
                return false;
            }
        }
        return true;
    }
};