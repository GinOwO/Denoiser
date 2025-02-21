#pragma once

#include "common.h"

#include <embree3/rtcore.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <vector>

namespace lighting
{
bool is_in_shadow(RTCScene p_scene, const glm::vec3 &vec_point,
		  const glm::vec3 &vec_light_dir, float f_dist_to_light);

glm::vec3 compute_lambert_color(const glm::vec3 &vec_normal,
				const glm::vec3 &vec_point,
				const glm::vec3 &vec_material_color,
				RTCScene p_scene);

glm::vec3 trace_ray(RTCScene p_scene, RTCDevice p_device, int i_pixel_x,
		    int i_pixel_y, int i_width, int i_height,
		    const glm::vec3 &vec_camera_origin,
		    const glm::vec3 &vec_lower_left_corner,
		    const glm::vec3 &vec_right, const glm::vec3 &vec_up,
		    float f_viewport_width, float f_viewport_height,
		    const std::vector<tinyobj::material_t> &v_materials);
} // namespace lighting