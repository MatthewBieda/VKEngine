#include "camera.hpp"

Camera::Camera(glm::vec3 position) : Position(position) {
	UpdateCameraVectors();
}

void Camera::ProcessMouseScroll(float yOffset)
{
	Zoom -= yOffset;
	if (Zoom < 1.0)
	{
		Zoom = 1.0f;
	}
	else if (Zoom > 90.0f)
	{
		Zoom = 90.0f;
	}
}

void Camera::FollowTarget(const glm::vec3& targetPos)
{
	glm::vec3 offset(FollowDistance, FollowHeight, 0.0f); // right side
	Position = glm::mix(Position, targetPos + offset, 0.1f);
	Front = glm::normalize(targetPos - Position);
}

void Camera::UpdateCameraVectors()
{
	// Calculate the new direction vector from the camera's updated Euler angles
	float yawRad = glm::radians(Yaw);
	float pitchRad = glm::radians(Pitch);

	Front = glm::normalize(glm::vec3(
		cos(yawRad) * cos(pitchRad),
		sin(pitchRad),
		sin(yawRad) * cos(pitchRad)
	));

	// Also, re-calculate the Right and Up vectors
	Right = glm::normalize(glm::cross(Front, WorldUp));
	Up = glm::normalize(glm::cross(Right, Front));
}
