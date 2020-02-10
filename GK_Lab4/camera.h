#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include "lights.h"

// CAMERA CLASS IS USED TO HOLD ALL THE NECESSARY DATA
// CONNECTED TO THE CAMERA (AND OTHER, NOT THAT NECESSARY
// BUT THE CODE ISN'T PERFECT)
class Camera {
public:
	int windowWidth;
	int windowHeight;
	glm::mat4 ViewMatrix;
	glm::mat4 ProjectionMatrix;
	glm::mat4 getViewMatrix() {
		return ViewMatrix;
	}
	glm::mat4 getProjectionMatrix() {
		return ProjectionMatrix;
	}
	// INITIAL POSITION: DEFAULT ON +Z
	glm::vec3 position = glm::vec3(0, 0, 5);
	// INITIAL HORIZONTAL ANGLE: TOWARDS -Z
	float horizontalAngle = 3.14f;
	// INITIAL VERTICAL ANGLE: 0
	float verticalAngle = 0;
	float initialFoV;
	float speed = 3.0f; // 3 units / second
	float mouseSpeed = 0.005f;
	float time = 0.0f;
	GLFWwindow* window;
	// Should we process mouse / WASD movements? (or is mouse used for changing models in AntTweaker?)
	bool shouldChange = true;
	// How many frames should pass until we should be able to click Q (change shouldChange) again?
	int releaseFrames = 70;
	int currentFrames = 70;
	// Did we just change from AntTweaker view to normal view?
	bool justChanged = false;
	// Does our camera come with spotlight?
	bool hasLight;
	spot_light_info* spot_light;

	Camera(int width, int height, glm::vec3 initialPos, float initialFoV, float speed, float mouseSpeed, GLFWwindow* window, spot_light_info* spot_light, bool hasLight);
	Camera(){ }
};