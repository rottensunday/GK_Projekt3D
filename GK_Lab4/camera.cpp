#include "camera.h"
#include "lights.h"
#include <chrono>
#include <thread>
using namespace glm;

Camera::Camera(int width, int height, glm::vec3 initialPos, float initialFoV, float speed, float mouseSpeed, GLFWwindow* window, spot_light_info* spot_light, bool hasLight) {
	windowWidth = width;
	windowHeight = height;
	position = initialPos;
	this->initialFoV = initialFoV;
	this->speed = speed;
	this->mouseSpeed = mouseSpeed;
	// FAR PLANE IS SO BIG SO WE CAN SEE VERY FAR
	ProjectionMatrix = glm::perspective(glm::radians(initialFoV), windowWidth / (float)windowHeight, 0.1f, 1000.0f);
	glm::vec3 direction(
		cos(verticalAngle) * sin(horizontalAngle),
		sin(verticalAngle),
		cos(verticalAngle) * cos(horizontalAngle)
	);
	// Right vector
	glm::vec3 right = glm::vec3(
		sin(horizontalAngle - 3.14f / 2.0f),
		0,
		cos(horizontalAngle - 3.14f / 2.0f)
	);
	// Up vector
	glm::vec3 up = glm::cross(right, direction);
	// Camera matrix
	ViewMatrix = glm::lookAt(
		position,           // Camera is here
		position + direction, // and looks here : at the same position, plus "direction"
		up                  // Head is up (set to 0,-1,0 to look upside-down)
	);
	this->window = window;
	this->spot_light = spot_light;
	this->hasLight = hasLight;
}