#pragma once
#include <glm/glm.hpp>

struct directional_light_info{
	glm::vec3 position; // the direction will always be: from position to (0,0,0)
    glm::vec3 light_color;
    float light_power;
};

struct point_light_info {
    glm::vec3 position;

    float constant;
    float linear;
    float quadratic;

    glm::vec3 light_color;
    float light_power;
};

struct spot_light_info {
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 light_color;

    float cutOff;
    float outerCutOff;
    float light_power;
    float constant;
    float linear;
    float quadratic;
};