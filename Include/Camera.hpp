#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

class Camera
{
public:
    // Camera Attributes
    glm::vec3 Position{ 0.0f, 2.0f, 8.0f };
    glm::vec3 Front{ 0.0f, 0.0f, -1.0f };
    glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
    glm::vec3 Right{ 1.0f, 0.0f, 0.0f };

    // Euler Angles
    float Yaw{ -90.0f };
    float Pitch{ 0.0f };

    // Camera options
    float MouseSensitivity{ 0.2f };
    float Zoom{ 60.0f };

    // Follow camera settings
    float FollowDistance{ 4.0f };
    float FollowHeight{ 2.0f };

    // Default Constructor
    Camera(glm::vec3 position = { 0.0f, 2.0f, 8.0f });

    inline glm::mat4 GetViewMatrix() const {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void ProcessMouseScroll(float yOffset);

    // Update camera position to follow target
    void FollowTarget(const glm::vec3& targetPos);

private:
    static constexpr glm::vec3 WorldUp{ 0.0f, 1.0f, 0.0f };
    void UpdateCameraVectors();
};
