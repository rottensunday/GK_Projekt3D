#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
//#include <filesystem>
#include "utilityreferences.h"
#include "model.h"

#include <AntTweakBar.h>
using namespace glm;


GLFWwindow* window;



Camera* currentCamera;
void TW_CALL set_camera(void* camera)
{
	currentCamera = (Camera*)camera;
}

struct shaders_info {
	Shader** direct;
	Shader** point;
	Shader** spot;
	bool isBlinn;
};

struct shaders {
	Shader* direct;
	Shader* point;
	Shader* spot;
};

struct shading_model {
	shaders new_shaders;
	shaders* current;
	depth_info* depth;
	scene* scene;
	GLuint depth_bias_id;
	GLuint depth_shadow_id;
};

// set Blinn or Phong lighting model
void TW_CALL set_lighting_shaders(void* info) 
{
	shaders_info input_info = *(shaders_info*)info;
	if (input_info.isBlinn) {
		(*input_info.direct)->use();
		(*input_info.direct)->setBool("isBlinn", true);
		(*input_info.point)->use();
		(*input_info.point)->setBool("isBlinn", true);
		(*input_info.spot)->use();
		(*input_info.spot)->setBool("isBlinn", true);
	}
	else {
		(*input_info.direct)->use();
		(*input_info.direct)->setBool("isBlinn", false);
		(*input_info.point)->use();
		(*input_info.point)->setBool("isBlinn", false);
		(*input_info.spot)->use();
		(*input_info.spot)->setBool("isBlinn", false);
	}
}

// set Phong or Gourand shaders
void TW_CALL set_shaders(void* info)
{
	shading_model input_info = *(shading_model*)info;
	input_info.depth->DepthBiasID = input_info.depth_bias_id;
	input_info.depth->ShadowMapID = input_info.depth_shadow_id;
	input_info.scene->direct = (*input_info.new_shaders.direct);
	input_info.scene->point = (*input_info.new_shaders.point);
	input_info.scene->spot = (*input_info.new_shaders.spot);
	input_info.current->direct = input_info.new_shaders.direct;
	input_info.current->point = input_info.new_shaders.point;
	input_info.current->spot = input_info.new_shaders.spot;
}


int main() {

	// INIT GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	///////////////////////////////// INIT WINDOW //////////////////////////////
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	int width = 1800;
	int height = 1000;
	window = glfwCreateWindow(width, height, "Aplikacja", NULL, NULL);

	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	//////////////////////////////////////////////////////////////////////////////

	///////////////////////////////// INIT GLAD //////////////////////////////////
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	//////////////////////////////////////////////////////////////////////////////
	
	// ENABLE DEPTH TEST
	glEnable(GL_DEPTH_TEST);
	// ACCEPT FRAGMENT IF IT IS CLOSER TO THE CAMERA THAN THE FORMER ONE
	glDepthFunc(GL_LESS);
	// CULL TRIANGLES
	glEnable(GL_CULL_FACE);

	///////////////////////////////// INIT SHADERS //////////////////////////////////
	Shader phong_direct("phong_direct.vertexshader", "phong_direct.fragmentshader");
	Shader phong_point("phong_point.vertexshader", "phong_point.fragmentshader");
	Shader phong_spot("phong_spot.vertexshader", "phong_spot.fragmentshader");
	Shader gourand_direct("gourand_direct.vertexshader", "gourand_direct.fragmentshader");
	Shader gourand_point("gourand_point.vertexshader", "gourand_point.fragmentshader");
	Shader gourand_spot("gourand_spot.vertexshader", "gourand_spot.fragmentshader");
	phong_direct.setBool("isBlinn", false);
	phong_direct.setBool("fogEnabled", false);
	phong_direct.setFloat("fogNear", 1.0);
	phong_direct.setFloat("fogFar", 1.0);
	phong_point.setBool("isBlinn", false);
	phong_spot.setBool("isBlinn", false);
	gourand_direct.setBool("isBlinn", false);
	gourand_direct.setBool("fogEnabled", false);
	gourand_direct.setFloat("fogNear", 1.0);
	gourand_direct.setFloat("fogFar", 1.0);
	gourand_point.setBool("isBlinn", false);
	gourand_spot.setBool("isBlinn", false);
	shaders phong_shaders = { &phong_direct, &phong_point, &phong_spot };
	shaders gourand_shaders = { &gourand_direct, &gourand_point, &gourand_spot };
	Shader depthShader("depth.vertexshader", "depth.fragmentshader");
	Shader default_light_shader("light.vertexshader", "light.fragmentshader");
	Shader cube_depth_shader("cubemap_depth.vertexshader", "cubemap_depth.fragmentshader", "cubemap_depth.geom");
	shaders current_shaders = { &phong_direct, &phong_point, &phong_spot };
	//////////////////////////////////////////////////////////////////////////////

	///////////////////////////////// CODE FOR SHADOMAPPING: RENDER TO TEXTURE ///
	GLuint FramebufferName = 0;
	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

	// DEPTH TEXTURE INITIALIZATION
	GLuint depthTexture;
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0);

	// No color output in the bound framebuffer, only depth.
	glDrawBuffer(GL_NONE);

	// CHECK FRAMEBUFFER
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return 1;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// CUBEMAP TEXTURE
	const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
	GLuint depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// DEPTH CUBEMAP (FOR POINT LIGHT)
	GLuint depthCubemap;
	glGenTextures(1, &depthCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
	for (unsigned int i = 0; i < 6; ++i)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//////////////////////////////////////////////////////////////////////////////


	

	///////////////////////////////// INITIALIZE SCENE MODELS ////////////////////
	scene my_scene((*current_shaders.direct), (*current_shaders.point), (*current_shaders.spot));
	// ADD MECH: OUR PLAYER MODEL
	my_scene.add_scene_element(new scene_element(Model("/resources/objects/mech/T 65 .obj"), "Mech"));
	my_scene.scene_collection[0]->transformation.position = glm::vec3(0, -2.8f, 0);
	// ADD CONNECTOR FOR ROTATING LIGHT
	my_scene.add_scene_element(new scene_element(Model("/resources/objects/connector/connector.obj"), "Connector"));
	my_scene.scene_collection[1]->transformation.position = glm::vec3(0, 2.2, 0);
	// ADD FLOOR: MOST OF THE SCENE
	my_scene.add_scene_element(new scene_element(Model("/resources/objects/floor/floor.obj"), "Floor"));
	my_scene.scene_collection[2]->transformation.position = glm::vec3(0, -8, 0);
	// ADD SUN: IT'S ROTATING AROUND THE SCENE. IT'S A DIRECTIONAL LIGHT
	my_scene.add_scene_element(new directional_light(Model("/resources/objects/sphere/sphere.obj"),
		"sun",
		default_light_shader, 
		directional_light_info{ glm::vec3(0,6,0), glm::vec3(1,1,1), 0.2f }));
	// ADD POINT LIGHT, ROTATING AROUND PLAYER
	my_scene.add_scene_element(new point_light(Model("/resources/objects/sphere_rotating/sphere.obj"),
		"Rotating",
		default_light_shader, 
		point_light_info{ glm::vec3(0,2.2,0), 1,1,1, glm::vec3(1,1,1), 10 } ));
	// ADD POINT LIGHT, MOVING IN THE ROOM
	my_scene.add_scene_element(new point_light(Model("/resources/objects/sphere/sphere.obj"),
		"moving_light_1",
		default_light_shader,
		point_light_info{ glm::vec3(31.8f,1.6f,-67.8f), 0.2,0.2,0.5, glm::vec3(1,1,1), 15 }));
	// ADD POINT LIGHT, MOVING IN THE ROOM
	my_scene.add_scene_element(new point_light(Model("/resources/objects/sphere/sphere.obj"),
		"moving_light_2",
		default_light_shader,
		point_light_info{ glm::vec3(-19.8f,1.6f,-58.2f), 0.2,0.2,0.5, glm::vec3(1,1,1), 15 }));
	// ADD CAMERA SPOTLIGHT
	spot_light_info cameraSpotlight = { glm::vec3(0,4,0), glm::vec3(0,0,0), glm::vec3(1,1,1), glm::cos(glm::radians(35.0f)), glm::cos(glm::radians(45.0f)),10.0,1,1,1 };
	my_scene.add_scene_element(new spot_light(Model("/resources/objects/sphere/sphere.obj"),
		"spot_light1",
		default_light_shader,
		&cameraSpotlight));
	// ADD RANDOM SPHERE
	my_scene.add_scene_element(new scene_element(Model("/resources/objects/sphere2/sphere2.obj"), "Sphere"));
	//////////////////////////////////////////////////////////////////////////////


	///////////////////////////////// INITIALIZE CAMERA AND OTHERS ///////////////
	Camera fps_camera(width, height, glm::vec3(0, -0.4, -2.7f), 80.0f, 15.0f, 0.005f, window, &cameraSpotlight, true);
	Camera still_camera(width, height, glm::vec3(0.5, 0, -1.0f), 80.0f, 15.0f, 0.005f, window, nullptr, false);
	Camera tps_camera(width, height, glm::vec3(0.5, 3, 10.0f), 80.0f, 15.0f, 0.005f, window, nullptr, false);
	currentCamera = &fps_camera;
	player_dummy player(my_scene.scene_collection[0], my_scene.scene_collection[1], my_scene.scene_collection[4], &fps_camera, &tps_camera);
	GLuint DepthBiasID_phong = glGetUniformLocation(phong_direct.ID, "DepthBiasMVP");
	GLuint ShadowMapID_phong = glGetUniformLocation(phong_direct.ID, "shadowMap");
	GLuint DepthBiasID_gourand = glGetUniformLocation(gourand_direct.ID, "DepthBiasMVP");
	GLuint ShadowMapID_gourand = glGetUniformLocation(gourand_direct.ID, "shadowMap");
	GLuint depthMatrixID = glGetUniformLocation(depthShader.ID, "depthMVP");
	//////////////////////////////////////////////////////////////////////////////

	depth_info depth = {
		FramebufferName,
		depthTexture,
		depthShader,
		DepthBiasID_gourand,
		ShadowMapID_gourand,
		depthMatrixID,
		depthMapFBO,
		depthCubemap,
		cube_depth_shader,
		width,
		height
	};

	///////////////////////////////// INITIALIZE ANTTWEAKBAR /////////////////////
	TwInit(TW_OPENGL_CORE, NULL);
	TwWindowSize(width, height);
	TwBar* myBar = TwNewBar("Models");
	TwSetParam(myBar, NULL, "refresh", TW_PARAM_CSTRING, 1, "0.1");

	TwStructMember normal_model_memebers[] = {
		{ "Posx", TW_TYPE_FLOAT, offsetof(Transformation, position), "step=0.2" },
		{ "Posy", TW_TYPE_FLOAT, offsetof(Transformation, position.y), "step=0.2" },
		{ "Posz", TW_TYPE_FLOAT, offsetof(Transformation, position.z), "step=0.2" },
		{ "Rotx", TW_TYPE_FLOAT, offsetof(Transformation, rotation), "step=0.2" },
		{ "Roty", TW_TYPE_FLOAT, offsetof(Transformation, rotation.y), "step=0.2" },
		{ "Rotz", TW_TYPE_FLOAT, offsetof(Transformation, rotation.z), "step=0.2" },
		{ "Scax", TW_TYPE_FLOAT, offsetof(Transformation, scale), "step=0.2" },
		{ "Scay", TW_TYPE_FLOAT, offsetof(Transformation, scale.y), "step=0.2" },
		{ "Scaz", TW_TYPE_FLOAT, offsetof(Transformation, scale.z), "step=0.2" }
	};
	TwStructMember directional_light_members[] = {
		{ "Posx", TW_TYPE_FLOAT, offsetof(directional_light_info, position), "step=0.2" },
		{ "Posy", TW_TYPE_FLOAT, offsetof(directional_light_info, position.y), "step=0.2" },
		{ "Posz", TW_TYPE_FLOAT, offsetof(directional_light_info, position.z), "step=0.2" },
		{ "colorR", TW_TYPE_FLOAT, offsetof(directional_light_info, light_color), "step=0.01" },
		{ "colorG", TW_TYPE_FLOAT, offsetof(directional_light_info, light_color.y), "step=0.01" },
		{ "colorB", TW_TYPE_FLOAT, offsetof(directional_light_info, light_color.z), "step=0.01" },
		{ "lightPower", TW_TYPE_FLOAT, offsetof(directional_light_info, light_power), "step=0.1" }
	};
	TwStructMember point_light_members[] = {
		{ "Posx", TW_TYPE_FLOAT, offsetof(point_light_info, position), "step=0.2" },
		{ "Posy", TW_TYPE_FLOAT, offsetof(point_light_info, position.y), "step=0.2" },
		{ "Posz", TW_TYPE_FLOAT, offsetof(point_light_info, position.z), "step=0.2" },
		{ "colorR", TW_TYPE_FLOAT, offsetof(point_light_info, light_color), "step=0.01" },
		{ "colorG", TW_TYPE_FLOAT, offsetof(point_light_info, light_color.y), "step=0.01" },
		{ "colorB", TW_TYPE_FLOAT, offsetof(point_light_info, light_color.z), "step=0.01" },
		{ "lightPower", TW_TYPE_FLOAT, offsetof(point_light_info, light_power), "step=0.1" },
		{ "constant", TW_TYPE_FLOAT, offsetof(point_light_info, constant), "step=0.01" },
		{ "linear", TW_TYPE_FLOAT, offsetof(point_light_info, linear), "step=0.01" },
		{ "quadratic", TW_TYPE_FLOAT, offsetof(point_light_info, quadratic), "step=0.01" }
	};
	TwStructMember spot_light_members[] = {
		{ "Posx", TW_TYPE_FLOAT, offsetof(spot_light_info, position), "step=0.2" },
		{ "Posy", TW_TYPE_FLOAT, offsetof(spot_light_info, position.y), "step=0.2" },
		{ "Posz", TW_TYPE_FLOAT, offsetof(spot_light_info, position.z), "step=0.2" },
		{ "colorR", TW_TYPE_FLOAT, offsetof(spot_light_info, light_color), "step=0.01" },
		{ "colorG", TW_TYPE_FLOAT, offsetof(spot_light_info, light_color.y), "step=0.01" },
		{ "colorB", TW_TYPE_FLOAT, offsetof(spot_light_info, light_color.z), "step=0.01" },
		{ "directionx", TW_TYPE_FLOAT, offsetof(spot_light_info, direction), "step=0.01" },
		{ "directiony", TW_TYPE_FLOAT, offsetof(spot_light_info, direction.y), "step=0.01" },
		{ "directionz", TW_TYPE_FLOAT, offsetof(spot_light_info, direction.z), "step=0.01" },
		{ "lightPower", TW_TYPE_FLOAT, offsetof(spot_light_info, light_power), "step=0.1" },
		{ "constant", TW_TYPE_FLOAT, offsetof(spot_light_info, constant), "step=0.01" },
		{ "linear", TW_TYPE_FLOAT, offsetof(spot_light_info, linear), "step=0.01" },
		{ "quadratic", TW_TYPE_FLOAT, offsetof(spot_light_info, quadratic), "step=0.01" },
		{ "cutOff", TW_TYPE_FLOAT, offsetof(spot_light_info, cutOff), "step=0.01" },
		{ "outerCutOff", TW_TYPE_FLOAT, offsetof(spot_light_info, outerCutOff), "step=0.01" }
	};
	TwType normal_model_type = TwDefineStruct("mojastruktrer", normal_model_memebers, 9, sizeof(Transformation), NULL, NULL);
	TwType directional_type = TwDefineStruct("mojastruktrer1", directional_light_members, 7, sizeof(directional_light_info), NULL, NULL);
	TwType point_type = TwDefineStruct("mojastruktrer2", point_light_members, 10, sizeof(point_light_info), NULL, NULL);
	TwType spot_type = TwDefineStruct("mojastrukturer3", spot_light_members, 15, sizeof(spot_light_info), NULL, NULL);
	for (auto& scene_element : my_scene.scene_collection) {
		if (scene_element->type == element_type::SIMPLE_MODEL) {
			TwAddVarRW(myBar, scene_element->tag.c_str(), normal_model_type, &scene_element->transformation, NULL);
		}
		else if (scene_element->type == element_type::DIRECT_LIGHT) {
			TwAddVarRW(myBar, scene_element->tag.c_str(), directional_type, scene_element->getInfo(), NULL);
		}
		else if (scene_element->type == element_type::POINT_LIGHT) {
			TwAddVarRW(myBar, scene_element->tag.c_str(), point_type, scene_element->getInfo(), NULL);
		}
		else if (scene_element->type == element_type::SPOT_LIGHT) {
			TwAddVarRW(myBar, scene_element->tag.c_str(), spot_type, scene_element->getInfo(), NULL);
		}
	}
	shaders_info set_phong = { &current_shaders.direct, &current_shaders.point, &current_shaders.spot, false };
	shaders_info set_blinn = { &current_shaders.direct, &current_shaders.point, &current_shaders.spot, true };
	shading_model set_phong_shading = { phong_shaders, &current_shaders, &depth, &my_scene,  DepthBiasID_phong, ShadowMapID_phong };
	shading_model set_gourand_shading = { gourand_shaders, &current_shaders, &depth, &my_scene,  DepthBiasID_gourand, ShadowMapID_gourand };
	TwAddButton(myBar, "FPS", set_camera, (void*)&fps_camera, nullptr);
	TwAddButton(myBar, "TPS", set_camera, (void*)&tps_camera, nullptr);
	TwAddButton(myBar, "Still", set_camera, (void*)&still_camera, nullptr);
	TwAddButton(myBar, "Phong Lighting", set_lighting_shaders, (void*)&set_phong, nullptr);
	TwAddButton(myBar, "Blinn Lighting", set_lighting_shaders, (void*)&set_blinn, nullptr);
	TwAddButton(myBar, "Phong Shader", set_shaders, (void*)&set_phong_shading, nullptr);
	TwAddButton(myBar, "Gouraud Shader", set_shaders, (void*)&set_gourand_shading, nullptr);

	// REDIRECT GLFW EVENTS TO ANTTWEAKBAR
	glfwSetMouseButtonCallback(window, (GLFWmousebuttonfun)TwEventMouseButtonGLFW); // - Directly redirect GLFW mouse button events to AntTweakBar
	glfwSetCursorPosCallback(window, (GLFWcursorposfun)TwEventMousePosGLFW);          // - Directly redirect GLFW mouse position events to AntTweakBar
	glfwSetScrollCallback(window, (GLFWscrollfun)TwEventMouseWheelGLFW);    // - Directly redirect GLFW mouse wheel events to AntTweakBar
	glfwSetKeyCallback(window, (GLFWkeyfun)TwEventKeyGLFW);                         // - Directly redirect GLFW key events to AntTweakBar
	glfwSetCharCallback(window, (GLFWcharfun)TwEventCharGLFW);                      // - Directly redirect GLFW char events to AntTweakBar

	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwPollEvents();
	glfwSetCursorPos(window, width / 2, height / 2);
	//////////////////////////////////////////////////////////////////////////////





	// SET SUN AND SKY CONSTANTS
	float sun_degree = 0;
	float sun_radius = 30;
	int steps = 0;
	int steps_stop = 3;
	float step_val = 0.1;
	scene_element* sun = my_scene.find_by_name("sun");
	directional_light_info* info = (directional_light_info*)sun->getInfo();
	point_light_info* moving_light_1_info = (point_light_info*)my_scene.find_by_name("moving_light_1")->getInfo();
	point_light_info* moving_light_2_info = (point_light_info*)my_scene.find_by_name("moving_light_2")->getInfo();
	bool moving_1 = false; // false: we move to negative x
	bool moving_2 = true; // true: we move to positive x

	glm::vec3 sky_black = glm::vec3(0,0,0);
	glm::vec3 sky_white = glm::vec3(135.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f);
	glm::vec3 sky_orange = glm::vec3(204.0f / 255.0f, 112.0f / 255.0f, 0);
	glm::vec3 sky_red = glm::vec3(1, 69.0f / 255.0f, 0);
	//////////////////////////////////////////////////////////////////////////////

	// MAIN RENDERING LOOP
	do {
		// INPUTS (PRESS ESC TO QUIT)
		processInput(window);

		// CLEAR SCREEN
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		current_shaders.direct->use();

		///////////////////////////////////////// SUN / LIGHT MOVEMENT ////
		steps++;
		if (steps % steps_stop == 0) {
			sun_degree += step_val;
			if (sun_degree > 360) sun_degree = 0;
			steps = 0;
			if (sun_degree < 180) phong_direct.setBool("fogEnabled", false);
			else phong_direct.setBool("fogEnabled", true);
		}

		info->position.z = glm::cos(glm::radians(sun_degree)) * sun_radius;
		info->position.y = glm::sin(glm::radians(sun_degree)) * sun_radius;

		float ratio;
		glm::vec3 sky_color;
		if (sun_degree < 90) {
			ratio = 1 - (90.0f - sun_degree) / 90.0f;
			sky_color = glm::mix(sky_orange, sky_white, ratio);
		}
		else if (sun_degree < 120) {
			sky_color = sky_white;
		}
		else if (sun_degree >= 330) {
			ratio = 1 - (360.0f - sun_degree) / 30.0f;
			sky_color = glm::mix(sky_black, sky_orange, ratio);
		}
		else if(sun_degree < 170){
			ratio = 1 - (170.0f - sun_degree) / 50.0f;
			sky_color = glm::mix(sky_white, sky_red, ratio);
		}
		else if (sun_degree >= 180) {
			sky_color = sky_black;
		}
		else if (sun_degree >= 170) {
			ratio = 1 - (180.0f - sun_degree) / 10.0f;
			sky_color = glm::mix(sky_red, sky_black, ratio);
		}
		glClearColor(sky_color.x, sky_color.y, sky_color.z, 1);
		ratio = 1.0 - (sun_degree - 180.0) / 180.0;
		if (ratio < 0.1) ratio = 0.5 + ((0.1 - ratio) * 10);
		else if (ratio < 0.5) ratio = 0.5;
		current_shaders.direct->setFloat("fogNear", ratio * 120);
		current_shaders.direct->setFloat("fogFar", ratio * 195);

		if (moving_1 == true) {
			if (moving_light_1_info->position.x >= 31.8) {
				moving_1 = false;
			}
			else {
				moving_light_1_info->position.x = moving_light_1_info->position.x + 0.05;
			}
		}
		else {
			if (moving_light_1_info->position.x <= -19.8) {
				moving_1 = true;
			}
			else {
				moving_light_1_info->position.x = moving_light_1_info->position.x - 0.05;
			}
		}

		if (moving_2 == true) {
			if (moving_light_2_info->position.x >= 31.8) {
				moving_2 = false;
			}
			else {
				moving_light_2_info->position.x = moving_light_2_info->position.x + 0.05;
			}
		}
		else {
			if (moving_light_2_info->position.x <= -19.8) {
				moving_2 = true;
			}
			else {
				moving_light_2_info->position.x = moving_light_2_info->position.x - 0.05;
			}
		}
		///////////////////////////////////////////////////////////////////

		// MAKE A STEP
		player.step();

		// GET PROJECTION AND VIEW MATRICES FOR CAMERA
		glm::mat4 projection = currentCamera->getProjectionMatrix();
		glm::mat4 view = currentCamera->getViewMatrix();

		// DRAW THE SCENE
		my_scene.draw_scene(view, projection, depth);
		TwDraw();
		
		// EVENTS
		glfwSwapBuffers(window);
		glfwPollEvents();

	}
	while (glfwWindowShouldClose(window) == 0);

	glfwTerminate();

	return 0;
}