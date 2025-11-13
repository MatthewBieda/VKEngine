#pragma once

SceneConfig scene = { 0.1f, 200.0f, "Maskonaive2" };

void setupLighting(LightingData& lights)
{
    // Warm sunset lighting
    lights.dirLight.direction = glm::vec4(-0.3f, -0.7f, -0.5f, 0.0f);
    lights.dirLight.color = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f); // Slightly warm

    // Optional: Add some ambient point lights for interest
    //lights.numPointLights = 3;
    // Campfire or lanterns scattered around
}

void setupSceneObjects(std::vector<ObjectData>& objectData)
{
    uint32_t terrainIndex = static_cast<uint32_t>(MeshType::Terrain);
    uint32_t grassIndex = static_cast<uint32_t>(MeshType::Grass);
    uint32_t snakeIndex = static_cast<uint32_t>(MeshType::SnakeStatue);

    // Main terrain
    glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    objectData.push_back({ model, terrainIndex });

    // Statues
    pos = { 0.0f, -2.0f, 3.0f };
    model = glm::translate(glm::mat4(1.0f), pos);
    model = glm::scale(model, glm::vec3(0.3f));
    objectData.push_back({ model, snakeIndex });
}


void updateLighting(LightingData& lights, float deltaTime)
{
    // Slow sun movement - full cycle takes ~5 minutes
    constexpr float rotationSpeed = glm::radians(2.0f); // degrees per second
    static float totalTime = 0.0f;
    totalTime += deltaTime;

    // Rotate around the scene
    float angle = totalTime * rotationSpeed;
    lights.dirLight.direction.x = glm::cos(angle) * 0.3f;
    lights.dirLight.direction.y = -0.7f; // Keep mostly downward
    lights.dirLight.direction.z = glm::sin(angle) * 0.5f;

    // Optional: Adjust color based on time of day
    float sunHeight = lights.dirLight.direction.y;
    float sunsetBlend = glm::smoothstep(-0.9f, -0.5f, sunHeight);
    lights.dirLight.color = glm::mix(
        glm::vec4(1.0f, 0.6f, 0.3f, 1.0f), // Sunset orange
        glm::vec4(1.0f, 0.95f, 0.9f, 1.0f), // Midday white
        sunsetBlend
    );
}

void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, float deltaTime)
{
    // Optional: Gentle grass swaying animation
    // You could update a uniform that shaders use to offset grass vertices
}