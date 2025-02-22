#include "renderer.h"
#include "lighting.h"

#include <immintrin.h>
#include <iostream>
#include <execution>
#include <OpenImageDenoise/oidn.hpp>

static void glfw_error_callback(int32_t i_error, const char *psz_description)
{
	std::cerr << "GLFW Error (" << i_error << "): " << psz_description
		  << "\n";
}

static void embree_error_func(void *, RTCError i_error, const char *psz_str)
{
	std::cerr << "Embree error (" << i_error << "): " << psz_str << "\n";
}

void Renderer::Engine::init_glfw()
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW\n";
		exit(EXIT_FAILURE);
	}

	p_window = glfwCreateWindow(1024, 1024,
				    "Cornell Box - Flat Light + Lambert",
				    nullptr, nullptr);
	if (!p_window) {
		std::cerr << "Failed to create GLFW window\n";
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(p_window);
	glfwSwapInterval(1);
}

void Renderer::Engine::init_embree_device()
{
	p_RTCdevice = rtcNewDevice(nullptr);
	if (!p_RTCdevice) {
		std::cerr << "Error: Unable to create Embree device\n";
		exit(EXIT_FAILURE);
	}
	rtcSetDeviceErrorFunction(p_RTCdevice, embree_error_func, nullptr);
}

void Renderer::Engine::init_camera()
{
	S_camera.vec_scene_center = { -278.0f, 274.4f, -279.6f };
	S_camera.vec_camera_origin = { S_camera.vec_scene_center.x,
				       S_camera.vec_scene_center.y, 800.0f };
	S_camera.vec_view_dir = glm::normalize(S_camera.vec_scene_center -
					       S_camera.vec_camera_origin);
	S_camera.vec_up = { 0.0f, 1.0f, 0.0f };
	S_camera.vec_right = glm::normalize(
		glm::cross(S_camera.vec_view_dir, S_camera.vec_up));

	S_camera.f_fov = 45.0f;
	S_camera.f_focal_length = glm::length(S_camera.vec_scene_center -
					      S_camera.vec_camera_origin);
	S_camera.f_viewport_height = 2.0f * S_camera.f_focal_length *
				     tan(glm::radians(S_camera.f_fov) / 2.0f);
	S_camera.f_viewport_width = S_camera.f_viewport_height;

	S_camera.vec_lower_left_corner =
		S_camera.vec_camera_origin +
		S_camera.vec_view_dir * S_camera.f_focal_length -
		S_camera.vec_right * (S_camera.f_viewport_width * 0.5f) -
		S_camera.vec_up * (S_camera.f_viewport_height * 0.5f);
}

Renderer::Engine::Engine(const int32_t i_width, const int32_t i_height,
			 const float f_ambient_intensity)
	: i_width(i_width)
	, i_height(i_height)
{
	init_glfw();
	init_embree_device();
	init_camera();
	S_scene.f_ambient_intensity = f_ambient_intensity;
}

Renderer::Engine::~Engine()
{
	rtcReleaseScene(S_scene.p_RTCscene);
	rtcReleaseDevice(p_RTCdevice);

	glfwDestroyWindow(p_window);
	glfwTerminate();
}

void Renderer::Engine::load_obj_scene(const std::string &s_obj_file,
				      const std::string &s_base_dir)
{
	const bool b_ret = tinyobj::LoadObj(
		&S_scene.S_attrib, &S_scene.v_shapes, &S_scene.v_materials,
		nullptr, s_obj_file.c_str(), s_base_dir.c_str());
	if (!b_ret) {
		std::cerr << "Failed to load/parse .obj file.\n";
		exit(EXIT_FAILURE);
	}

	S_scene.p_RTCscene = rtcNewScene(p_RTCdevice);

	for (size_t s = 0; s < S_scene.v_shapes.size(); s++) {
		const tinyobj::mesh_t &S_mesh = S_scene.v_shapes[s].mesh;
		size_t i_num_vertices = S_scene.S_attrib.vertices.size() / 3;
		size_t i_num_triangles = S_mesh.indices.size() / 3;

		RTCGeometry p_geom =
			rtcNewGeometry(p_RTCdevice, RTC_GEOMETRY_TYPE_TRIANGLE);

		Vertex *p_vertices = (Vertex *)rtcSetNewGeometryBuffer(
			p_geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
			sizeof(Vertex), i_num_vertices);

		for (size_t i = 0; i < i_num_vertices; i++) {
			p_vertices[i].x = S_scene.S_attrib.vertices[3 * i + 0];
			p_vertices[i].y = S_scene.S_attrib.vertices[3 * i + 1];
			p_vertices[i].z = S_scene.S_attrib.vertices[3 * i + 2];
		}

		Triangle *p_triangles = (Triangle *)rtcSetNewGeometryBuffer(
			p_geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
			sizeof(Triangle), i_num_triangles);

		for (size_t i = 0; i < i_num_triangles; i++) {
			p_triangles[i].v0 =
				S_mesh.indices[3 * i + 0].vertex_index;
			p_triangles[i].v1 =
				S_mesh.indices[3 * i + 1].vertex_index;
			p_triangles[i].v2 =
				S_mesh.indices[3 * i + 2].vertex_index;
		}

		GeometryUserData *p_user_data = new GeometryUserData;
		p_user_data->mesh_ptr = &S_mesh;
		rtcSetGeometryUserData(p_geom, p_user_data);

		rtcCommitGeometry(p_geom);
		rtcAttachGeometryByID(S_scene.p_RTCscene, p_geom,
				      static_cast<unsigned>(s));
		rtcReleaseGeometry(p_geom);
	}

	rtcCommitScene(S_scene.p_RTCscene);
}

void Renderer::Engine::render_loop()
{
	static int frame_count = 0;
	static std::vector<float> acc_buffer(i_width * i_height * 3, 0.0f);

	GLuint texture_id;
	std::vector<float> v_framebuffer(i_width * i_height * 3, 0.0f);

	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, i_width, i_height, 0, GL_RGB,
		     GL_FLOAT, nullptr);

	m_texture_id = texture_id;
	std::fill(std::execution::par_unseq, v_framebuffer.begin(),
		  v_framebuffer.end(), 0.0f);

	static double last_time = glfwGetTime();
	while (!glfwWindowShouldClose(p_window)) {
		glfwPollEvents();

		// std::memset(v_framebuffer.data(), 0,
		// 	    v_framebuffer.size() * sizeof(float));

		render_frame(v_framebuffer);

		frame_count++;
		for (int i = 0; i < i_width * i_height * 3; i++) {
			acc_buffer[i] = (acc_buffer[i] * (frame_count - 1) +
					 v_framebuffer[i]) /
					frame_count;
		}

		glBindTexture(GL_TEXTURE_2D, m_texture_id);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, i_width, i_height,
				GL_RGB, GL_FLOAT, acc_buffer.data());

		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m_texture_id);
		glBegin(GL_QUADS);
		{
			glTexCoord2f(0.0f, 0.0f);
			glVertex2f(-1.0f, -1.0f);

			glTexCoord2f(1.0f, 0.0f);
			glVertex2f(1.0f, -1.0f);

			glTexCoord2f(1.0f, 1.0f);
			glVertex2f(1.0f, 1.0f);

			glTexCoord2f(0.0f, 1.0f);
			glVertex2f(-1.0f, 1.0f);
		}
		glEnd();
		glDisable(GL_TEXTURE_2D);
		glfwSwapBuffers(p_window);

		double current_time = glfwGetTime();
		std::cout << "Sample time: " << current_time - last_time
			  << "s\n";
		last_time = current_time;
	}
}

static inline float ACES_tonemapper(const float x)
{
	static constexpr float a = 2.51f;
	static constexpr float b = 0.03f;
	static constexpr float c = 2.43f;
	static constexpr float d = 0.59f;
	static constexpr float e = 0.14f;
	return glm::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f,
			  1.0f);
}

void Renderer::Engine::render_frame(std::vector<float> &v_framebuffer)
{
	// static std::vector<int32_t> v_pixel_y;
	// if (v_pixel_y.empty()) {
	// 	v_pixel_y.resize(i_height);
	// 	std::iota(v_pixel_y.begin(), v_pixel_y.end(), 0);
	// }

	// std::for_each(
	// 	std::execution::par, v_pixel_y.begin(), v_pixel_y.end(),
	// 	[&](int32_t i_pixel_y) {
	// 		for (int i_pixel_x = 0; i_pixel_x < i_width;
	// 		     i_pixel_x++) {
	// 			const glm::vec3 vec_color{ lighting::trace_ray(
	// 				S_scene, S_camera, p_RTCdevice,
	// 				i_pixel_x, i_pixel_y, i_width,
	// 				i_height) };

	// 			int i_index =
	// 				(i_pixel_y * i_width + i_pixel_x) * 3;
	// 			v_framebuffer[i_index + 0] = vec_color.r;
	// 			v_framebuffer[i_index + 1] = vec_color.g;
	// 			v_framebuffer[i_index + 2] = vec_color.b;
	// 		}
	// 	});

#pragma omp parallel for schedule(dynamic)
	for (int32_t i_pixel_y = 0; i_pixel_y < i_height; i_pixel_y++) {
		for (int32_t i_pixel_x = 0; i_pixel_x < i_width; i_pixel_x++) {
			const glm::vec3 vec_color{ lighting::trace_ray(
				S_scene, S_camera, p_RTCdevice, i_pixel_x,
				i_pixel_y, i_width, i_height) };

			int i_index = (i_pixel_y * i_width + i_pixel_x) * 3;
			// v_framebuffer[i_index + 0] =
			// 	glm::clamp(vec_color.r, 0.0f, 1.0f);
			// v_framebuffer[i_index + 1] =
			// 	glm::clamp(vec_color.g, 0.0f, 1.0f);
			// v_framebuffer[i_index + 2] =
			// 	glm::clamp(vec_color.b, 0.0f, 1.0f);
			v_framebuffer[i_index + 0] =
				ACES_tonemapper(vec_color.r);
			v_framebuffer[i_index + 1] =
				ACES_tonemapper(vec_color.g);
			v_framebuffer[i_index + 2] =
				ACES_tonemapper(vec_color.b);
		}
	}
}
