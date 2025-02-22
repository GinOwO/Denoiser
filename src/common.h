#pragma once

#include <embree3/rtcore.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <cstdint>

struct Vertex {
	float x, y, z;
};

struct Triangle {
	uint32_t v0, v1, v2;
};

struct GeometryUserData {
	const tinyobj::mesh_t *mesh_ptr;
};

struct Scene {
	RTCScene p_RTCscene;
	tinyobj::attrib_t S_attrib;
	std::vector<tinyobj::shape_t> v_shapes;
	std::vector<tinyobj::material_t> v_materials;
	float f_ambient_intensity;
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

struct SurfaceInfo {
	glm::vec3 color;
	glm::vec3 albedo;
	glm::vec3 normal;
};