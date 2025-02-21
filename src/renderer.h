#pragma once

#include "common.h"

#include <embree3/rtcore.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <cstdint>

namespace Renderer
{
class Engine {
	int32_t i_width = 1024;
	int32_t i_height = 1024;

	GLuint m_texture_id;
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
	Engine(const int32_t i_width = 1024, const int32_t i_height = 1024,
	       const float f_ambient_intensity = 0.1f);
	~Engine();

	void load_obj_scene(const std::string &s_obj_file,
			    const std::string &s_base_dir);
	void render_loop();
};
} // namespace Renderer