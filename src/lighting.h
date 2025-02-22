#pragma once

#include "common.h"

#include <embree3/rtcore.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <vector>

namespace lighting
{
bool is_in_shadow(const RTCScene &p_scene, const glm::vec3 &vec_point,
		  const glm::vec3 &vec_light_dir, float f_dist_to_light);

glm::vec3 compute_lambert_color(const glm::vec3 &vec_normal,
				const glm::vec3 &vec_point,
				const glm::vec3 &vec_material_color,
				RTCScene p_scene, float f_ambient_strength);

SurfaceInfo trace_ray_with_buffers(const Scene &S_scene, const Camera &S_camera,
				   RTCDevice p_device, int32_t i_pixel_x,
				   int32_t i_pixel_y, int32_t i_width,
				   int32_t i_height);

glm::vec3 trace_ray(const Scene &S_scene, const Camera &S_camera,
		    RTCDevice p_device, int32_t i_pixel_x, int32_t i_pixel_y,
		    int32_t i_width, int32_t i_height);

} // namespace lighting