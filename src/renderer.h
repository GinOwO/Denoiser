#pragma once

#include "common.h"

#include <embree3/rtcore.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cfloat>

namespace Renderer
{
struct Scene {
	RTCScene p_RTCscene;
	tinyobj::attrib_t S_attrib;
	std::vector<tinyobj::shape_t> v_shapes;
	std::vector<tinyobj::material_t> v_materials;
};

struct Camera {
	glm::vec3 vec_up;
	glm::vec3 vec_right;
	glm::vec3 vec_view_dir;
	glm::vec3 vec_scene_center;
	glm::vec3 vec_camera_origin;
	glm::vec3 vec_lower_left_corner;

	float f_fov;
	float f_focal_length;
	float f_viewport_height;
	float f_viewport_width;
};

class Renderer {
	int i_width = 1024;
	int i_height = 1024;

	GLFWwindow *p_window;
	RTCDevice p_RTCdevice;
	Camera S_camera;
	Scene S_scene;

    private:
	void init_glfw();
	void init_embree_device();
	void init_camera();
	void render_frame(std::vector<float> &vec_framebuffer);

    public:
	Renderer(int i_width = 1024, int i_height = 1024);
	~Renderer();

	void load_obj_scene(const std::string &s_obj_file,
			    const std::string &s_base_dir);
	void render_loop();
};
} // namespace Renderer