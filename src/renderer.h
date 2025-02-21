#pragma once

#include "common.h"
#include "lighting.h"

#include <embree3/rtcore.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cfloat>

static void embree_error_func(void *, RTCError i_error, const char *psz_str)
{
	std::cerr << "Embree error (" << i_error << "): " << psz_str << "\n";
}

RTCDevice init_embree_device()
{
	RTCDevice p_device = rtcNewDevice(nullptr);
	if (!p_device) {
		std::cerr << "Error: Unable to create Embree device\n";
		exit(EXIT_FAILURE);
	}
	rtcSetDeviceErrorFunction(p_device, embree_error_func, nullptr);
	return p_device;
}

RTCScene load_obj_scene(RTCDevice p_device, const std::string &s_obj_file,
			const std::string &s_base_dir,
			tinyobj::attrib_t &t_attrib,
			std::vector<tinyobj::shape_t> &vec_shapes,
			std::vector<tinyobj::material_t> &vec_materials)
{
	bool b_ret = tinyobj::LoadObj(&t_attrib, &vec_shapes, &vec_materials,
				      nullptr, s_obj_file.c_str(),
				      s_base_dir.c_str());
	if (!b_ret) {
		std::cerr << "Failed to load/parse .obj file.\n";
		exit(EXIT_FAILURE);
	}

	RTCScene p_scene = rtcNewScene(p_device);

	for (size_t s = 0; s < vec_shapes.size(); s++) {
		const tinyobj::mesh_t &t_mesh = vec_shapes[s].mesh;
		size_t i_num_vertices = t_attrib.vertices.size() / 3;
		size_t i_num_triangles = t_mesh.indices.size() / 3;

		RTCGeometry p_geom =
			rtcNewGeometry(p_device, RTC_GEOMETRY_TYPE_TRIANGLE);

		Vertex *p_vertices = (Vertex *)rtcSetNewGeometryBuffer(
			p_geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
			sizeof(Vertex), i_num_vertices);

		for (size_t i = 0; i < i_num_vertices; i++) {
			p_vertices[i].x = t_attrib.vertices[3 * i + 0];
			p_vertices[i].y = t_attrib.vertices[3 * i + 1];
			p_vertices[i].z = t_attrib.vertices[3 * i + 2];
		}

		Triangle *p_triangles = (Triangle *)rtcSetNewGeometryBuffer(
			p_geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
			sizeof(Triangle), i_num_triangles);

		for (size_t i = 0; i < i_num_triangles; i++) {
			p_triangles[i].v0 =
				t_mesh.indices[3 * i + 0].vertex_index;
			p_triangles[i].v1 =
				t_mesh.indices[3 * i + 1].vertex_index;
			p_triangles[i].v2 =
				t_mesh.indices[3 * i + 2].vertex_index;
		}

		GeometryUserData *p_user_data = new GeometryUserData;
		p_user_data->mesh_ptr = &t_mesh;
		rtcSetGeometryUserData(p_geom, p_user_data);

		rtcCommitGeometry(p_geom);
		rtcAttachGeometryByID(p_scene, p_geom,
				      static_cast<unsigned>(s));
		rtcReleaseGeometry(p_geom);
	}

	rtcCommitScene(p_scene);
	return p_scene;
}

void render_frame(RTCScene p_scene, RTCDevice p_device, int i_width,
		  int i_height, const glm::vec3 &vec_camera_origin,
		  const glm::vec3 &vec_lower_left_corner,
		  const glm::vec3 &vec_right, const glm::vec3 &vec_up,
		  float f_viewport_width, float f_viewport_height,
		  const std::vector<tinyobj::material_t> &vec_materials,
		  std::vector<float> &vec_framebuffer)
{
	for (int i_pixel_y = 0; i_pixel_y < i_height; i_pixel_y++) {
		for (int i_pixel_x = 0; i_pixel_x < i_width; i_pixel_x++) {
			glm::vec3 vec_color = trace_ray(
				p_scene, p_device, i_pixel_x, i_pixel_y,
				i_width, i_height, vec_camera_origin,
				vec_lower_left_corner, vec_right, vec_up,
				f_viewport_width, f_viewport_height,
				vec_materials);

			int i_index = (i_pixel_y * i_width + i_pixel_x) * 3;
			vec_framebuffer[i_index + 0] = vec_color.r;
			vec_framebuffer[i_index + 1] = vec_color.g;
			vec_framebuffer[i_index + 2] = vec_color.b;
		}
	}
}

void render_loop(RTCScene p_scene, RTCDevice p_device, GLFWwindow *p_window,
		 int i_width, int i_height, const glm::vec3 &vec_camera_origin,
		 const glm::vec3 &vec_lower_left_corner,
		 const glm::vec3 &vec_right, const glm::vec3 &vec_up,
		 float f_viewport_width, float f_viewport_height,
		 const std::vector<tinyobj::material_t> &vec_materials)
{
	std::vector<float> vec_framebuffer(i_width * i_height * 3, 0.0f);

	while (!glfwWindowShouldClose(p_window)) {
		std::fill(vec_framebuffer.begin(), vec_framebuffer.end(), 0.0f);

		render_frame(p_scene, p_device, i_width, i_height,
			     vec_camera_origin, vec_lower_left_corner,
			     vec_right, vec_up, f_viewport_width,
			     f_viewport_height, vec_materials, vec_framebuffer);

		glClear(GL_COLOR_BUFFER_BIT);
		glDrawPixels(i_width, i_height, GL_RGB, GL_FLOAT,
			     vec_framebuffer.data());
		glfwSwapBuffers(p_window);
		glfwPollEvents();
	}
}