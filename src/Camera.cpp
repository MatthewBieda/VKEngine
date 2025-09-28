#include "camera.hpp"

Camera::Camera(glm::vec3 position) : Position(position) {
	UpdateCameraVectors();
}

void Camera::ProcessKeyboard(CameraMovement direction, float deltaTime)
{
	float velocity = MovementSpeed * deltaTime;
	switch (direction)
	{
		case CameraMovement::FORWARD: Position += Front * velocity; break;
		case CameraMovement::BACKWARD: Position -= Front * velocity; break;
		case CameraMovement::LEFT: Position -= Right * velocity; break;
		case CameraMovement::RIGHT: Position += Right * velocity; break;
	}
}

void Camera::ProcessMouseMovement(float xOffset, float yOffset)
{
	xOffset *= MouseSensitivity;
	yOffset *= MouseSensitivity;

	Yaw += xOffset;
	Pitch += yOffset;

	// make sure that when pitch is out of bounds, screen doesn't get flipped
	if (Pitch > 89.0f)
	{
		Pitch = 89.0f;
	}
	else if (Pitch < -89.0f)
	{
		Pitch = -89.0f;
	}

	// Update Front, Right and Up vectors using the updated Euler angles
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
