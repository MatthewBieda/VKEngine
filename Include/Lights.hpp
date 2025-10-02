#pragma once
#include "glm.hpp"

struct DirectionalLight
{
	glm::vec4 direction; 
	glm::vec4 color; 
};

struct PointLight
{
	glm::vec4 position;
	glm::vec4 color;
	float radius;
	float padding[3];
};

struct LigthingData
{
	DirectionalLight dirLight;
	int numPointLights;
	PointLight pointsLights[16];
};