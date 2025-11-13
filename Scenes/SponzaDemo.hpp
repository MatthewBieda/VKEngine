#pragma once

SceneConfig scene = { 0.1f, 50.0f, "YokohamaCity" };

void setupLighting(LightingData& lights)
{
    // Directional light
    lights.dirLight.direction = glm::vec4(-0.3f, -1.5f, -0.3f, 0.0f);
    lights.dirLight.color = glm::vec4(1.0f, 0.97f, 0.9f, 1.0f); // slightly warm sunlight

    // Point lights
    lights.numPointLights = 0;

    // Warm interior lights
    auto addLight = [&](glm::vec3 pos, glm::vec3 color, float radius)
        {
            uint32_t i = lights.numPointLights++;
            lights.pointLights[i].position = glm::vec4(pos, 1.0f);
            lights.pointLights[i].color = glm::vec4(color, 1.0f);
            lights.pointLights[i].radius = radius;
        };

    // Center hall chandeliers (warm)
    // addLight({ 0.0f, 3.0f, 0.0f }, { 1.0f, 0.85f, 0.7f }, 12.0f);
    addLight({ 0.0f, 7.0f, 3.8f }, { 1.0f, 0.85f, 0.7f }, 12.0f);
    addLight({ 0.0f, 7.0f, -4.3f }, { 1.0f, 0.85f, 0.7f }, 12.0f);

    // Side corridor lights (slightly dimmer)
    addLight({ 8.0f, 3.0f, 3.8f }, { 0.9f, 0.75f, 0.6f }, 10.0f);
    addLight({ -8.0f, 3.0f, 3.8f }, { 0.9f, 0.75f, 0.6f }, 10.0f);
    addLight({ 8.0f, 3.0f, -4.3f }, { 0.9f, 0.75f, 0.6f }, 10.0f);
    addLight({ -8.0f, 3.0f, -4.3f }, { 0.9f, 0.75f, 0.6f }, 10.0f);

    // Lights illuminating the lion statue
    addLight({ 11.0f, 3.0f, 0.0f }, { 1.0f, 0.85f, 0.7f }, 12.0f);
    addLight({ -12.0f, 3.0f, 0.0f }, { 1.0f, 0.85f, 0.7f }, 12.0f);

    // Cool fill light
    addLight({ 0.0f, 10.0f, 0.0f }, { 0.3f, 0.4f, 0.6f }, 25.0f);
}

void setupSceneObjects(std::vector<ObjectData>& objectData)
{
    // Remove this comment to debug light positions
    /*
    uint32_t lightCasterIndex = static_cast<uint32_t>(MeshType::LightCaster);

    for (uint32_t i = 0; i < lights.numPointLights; ++i)
    {
        glm::vec3 pos = lights.pointLights[i].position;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
        objectData.push_back({ model, lightCasterIndex });
    }
    */

    // Sponza
    glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    uint32_t meshIndex = static_cast<uint32_t>(MeshType::Sponza);
    objectData.push_back({ model, meshIndex });

    // Glass window
    pos = { 1.0f, 6.0f, 4.0f };
    model = glm::translate(glm::mat4(1.0f), pos);
    meshIndex = static_cast<uint32_t>(MeshType::GlassWindow);
    objectData.push_back({ model, meshIndex });
}

void updateLighting(LightingData& lights, float deltaTime)
{
}

void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, float deltaTime)
{
    static float rotationAngle = 0.0f;
    rotationAngle += glm::radians(45.0f) * deltaTime; // 45 degrees per second

    glm::vec3 pos = glm::vec3(objectData[1].model[3]); // Extract current position
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
    objectData[1].model = translation * rotation;
}
