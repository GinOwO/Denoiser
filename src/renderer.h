#pragma once

#include "common.h"

#include <embree3/rtcore.h>
#include <GLFW/glfw3.h>
#include <OpenImageDenoise/oidn.hpp>

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

	oidn::DeviceRef m_oidn_device;
	oidn::FilterRef m_denoiser_filter;
	std::vector<float> v_color_buffer;
	std::vector<float> v_albedo_buffer;
	std::vector<float> v_normal_buffer;
	std::vector<float> m_denoised_frame;

    private:
	void init_glfw();
	void init_embree_device();
	void init_camera();
	void render_frame(std::vector<float> &vec_framebuffer);
	static void
	write_buffer_to_image(const std::vector<float> &vec_buffer,
			      const int32_t i_width, const int32_t i_height,
			      const std::string &s_output_file = "output.png");

    public:
	Engine(const int32_t i_width = 1024, const int32_t i_height = 1024,
	       const float f_ambient_intensity = 0.1f);
	~Engine();

	void load_obj_scene(const std::string &s_obj_file,
			    const std::string &s_base_dir);

	void render_loop(const int sample_limit = 16);

	void oidn_denoise();

	void custom_denoise();
};
} // namespace Renderer