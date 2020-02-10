#include "inputprocessing.h"

// CODE USED TO DETERMINE WHETHER ESC WAS CLICKED
// AND WE SHOULD CLOSE THE WINDOW
void processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}