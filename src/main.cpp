#include "common.h"
#include "renderer.h"

#include <embree3/rtcore.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cfloat>

static void glfw_error_callback(int i_error, const char *psz_description)
{
	std::cerr << "GLFW Error (" << i_error << "): " << psz_description
		  << "\n";
}

int main()
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW\n";
		return EXIT_FAILURE;
	}

	GLFWwindow *p_window = glfwCreateWindow(
		1024, 1024, "Cornell Box - Flat Light + Lambert", nullptr,
		nullptr);
	if (!p_window) {
		std::cerr << "Failed to create GLFW window\n";
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(p_window);
	glfwSwapInterval(1);

	RTCDevice p_rtc_device = init_embree_device();

	std::string s_input_file =
		"/home/gin/Desktop/denoise/src/CornellBox.obj";
	std::string s_base_dir = "/home/gin/Desktop/denoise/src/";

	tinyobj::attrib_t t_attrib;
	std::vector<tinyobj::shape_t> vec_shapes;
	std::vector<tinyobj::material_t> vec_materials;

	RTCScene p_scene = load_obj_scene(p_rtc_device, s_input_file,
					  s_base_dir, t_attrib, vec_shapes,
					  vec_materials);

	rtcCommitScene(p_scene);

	glm::vec3 vec_scene_center(-278.0f, 274.4f, -279.6f);
	glm::vec3 vec_camera_origin(vec_scene_center.x, vec_scene_center.y,
				    800.0f);
	glm::vec3 vec_view_dir =
		glm::normalize(vec_scene_center - vec_camera_origin);
	glm::vec3 vec_up(0.0f, 1.0f, 0.0f);
	glm::vec3 vec_right = glm::normalize(glm::cross(vec_view_dir, vec_up));

	float f_fov = 45.0f;
	float f_focal_length =
		glm::length(vec_scene_center - vec_camera_origin);
	float f_viewport_height =
		2.0f * f_focal_length * tan(glm::radians(f_fov) / 2.0f);
	float f_viewport_width = f_viewport_height;

	glm::vec3 vec_lower_left_corner =
		vec_camera_origin + vec_view_dir * f_focal_length -
		vec_right * (f_viewport_width * 0.5f) -
		vec_up * (f_viewport_height * 0.5f);

	render_loop(p_scene, p_rtc_device, p_window, 1024, 1024,
		    vec_camera_origin, vec_lower_left_corner, vec_right, vec_up,
		    f_viewport_width, f_viewport_height, vec_materials);

	rtcReleaseScene(p_scene);
	rtcReleaseDevice(p_rtc_device);
	glfwDestroyWindow(p_window);
	glfwTerminate();
	return EXIT_SUCCESS;
}
