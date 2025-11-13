#pragma once

SceneConfig scene = { 0.1f, 200.0f, "YokohamaCity"};

void setupLighting(LightingData& lights)
{
	lights.dirLight.direction = glm::vec4(-1.0f, -1.0f, -1.0f, 0.0f);
	lights.dirLight.color = glm::vec4(1.0f);
}

void setupSceneObjects(std::vector<ObjectData>& objectData)
{
	glm::vec3 pos{};
	glm::mat4 model{};
	uint32_t groundPlaneIndex{};
	uint32_t SnakeStatueIndex{};

	// Snake Statues - instanced grid on planes
	groundPlaneIndex = static_cast<uint32_t>(MeshType::GroundPlane);
	SnakeStatueIndex = static_cast<uint32_t>(MeshType::SnakeStatue);

	const int gridCount = 5; // 5x5 = 25 statues
	const float spacing = 25.0f; // distance between each statue

	for (int x = 0; x < gridCount; ++x)
	{
		for (int z = 0; z < gridCount; ++z)
		{
			glm::vec3 offset = {
				(x - gridCount / 2) * spacing,
				0.0f,
				(z - gridCount / 2) * spacing
			};

			model = glm::translate(glm::mat4(1.0f), offset);

			// Random rotation
			float randomRot = glm::radians(static_cast<float>(rand() % 360));
			model = glm::rotate(model, randomRot, glm::vec3(0.0f, 1.0f, 0.0f));

			objectData.push_back({ model, SnakeStatueIndex });
		}
	}

	for (int x = 0; x < gridCount; ++x)
	{
		for (int z = 0; z < gridCount; ++z)
		{
			glm::vec3 offset = {
				(x - gridCount / 2) * spacing,
				0.0f,
				(z - gridCount / 2) * spacing
			};

			model = glm::translate(glm::mat4(1.0f), offset);
			objectData.push_back({ model, groundPlaneIndex });
		}
	}
}

void updateLighting(LightingData& lights, float deltaTime)
{
	// Speed of rotation in radians per second
	constexpr float rotationSpeed = glm::radians(10.0f);

	static float totalTime = 0.0f;
	totalTime += deltaTime;
	lights.dirLight.direction.x = glm::cos(totalTime * rotationSpeed);
	lights.dirLight.direction.z = glm::sin(totalTime * rotationSpeed);
}

void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, float deltaTime)
{
}
