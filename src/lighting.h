#include "common.h"

#include <embree3/rtcore.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <cstring>
#include <vector>
#include <cfloat>

bool is_in_shadow(RTCScene p_scene, const glm::vec3 &vec_point,
		  const glm::vec3 &vec_light_dir, float f_dist_to_light)
{
	RTCRayHit t_shadow_ray;
	std::memset(&t_shadow_ray, 0, sizeof(t_shadow_ray));

	t_shadow_ray.ray.org_x = vec_point.x;
	t_shadow_ray.ray.org_y = vec_point.y;
	t_shadow_ray.ray.org_z = vec_point.z;

	t_shadow_ray.ray.dir_x = vec_light_dir.x;
	t_shadow_ray.ray.dir_y = vec_light_dir.y;
	t_shadow_ray.ray.dir_z = vec_light_dir.z;

	t_shadow_ray.ray.tnear = 0.001f;
	t_shadow_ray.ray.tfar = f_dist_to_light - 0.001f;
	t_shadow_ray.ray.mask = -1;
	t_shadow_ray.ray.flags = 0;
	t_shadow_ray.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	t_shadow_ray.hit.primID = RTC_INVALID_GEOMETRY_ID;

	RTCIntersectContext t_context;
	rtcInitIntersectContext(&t_context);
	rtcOccluded1(p_scene, &t_context, &t_shadow_ray.ray);

	return (t_shadow_ray.ray.tfar < 0.0f);
}

glm::vec3 compute_lambert_color(const glm::vec3 &vec_normal,
				const glm::vec3 &vec_point,
				const glm::vec3 &vec_material_color,
				RTCScene p_scene)
{
	static glm::vec3 vec_light_pos(-278.0f, 548.0f, -279.6f);
	static glm::vec3 vec_light_color(1.0f, 1.0f, 1.0f);
	static float f_light_intensity = 25.0f;

	glm::vec3 vec_light_dir = vec_light_pos - vec_point;
	float f_dist_to_light = glm::length(vec_light_dir);
	vec_light_dir = glm::normalize(vec_light_dir);

	float f_n_dot_l = glm::dot(vec_normal, vec_light_dir);
	if (f_n_dot_l <= 0.0f) {
		return glm::vec3(0.0f);
	}

	bool b_occluded = is_in_shadow(p_scene, vec_point + 0.001f * vec_normal,
				       vec_light_dir, f_dist_to_light);
	if (b_occluded) {
		return glm::vec3(0.0f);
	}

	float f_lambert_term = f_n_dot_l * f_light_intensity;
	glm::vec3 vec_lit_color =
		vec_material_color * vec_light_color * f_lambert_term;
	return vec_lit_color;
}

glm::vec3 trace_ray(RTCScene p_scene, RTCDevice p_device, int i_pixel_x,
		    int i_pixel_y, int i_width, int i_height,
		    const glm::vec3 &vec_camera_origin,
		    const glm::vec3 &vec_lower_left_corner,
		    const glm::vec3 &vec_right, const glm::vec3 &vec_up,
		    float f_viewport_width, float f_viewport_height,
		    const std::vector<tinyobj::material_t> &vec_materials)
{
	float f_u =
		static_cast<float>(i_pixel_x) / static_cast<float>(i_width - 1);
	float f_v = static_cast<float>(i_pixel_y) /
		    static_cast<float>(i_height - 1);

	glm::vec3 vec_pixel_position = vec_lower_left_corner +
				       vec_right * (f_u * f_viewport_width) +
				       vec_up * (f_v * f_viewport_height);

	glm::vec3 vec_ray_direction =
		glm::normalize(vec_pixel_position - vec_camera_origin);

	RTCRayHit t_ray_hit;
	std::memset(&t_ray_hit, 0, sizeof(t_ray_hit));

	t_ray_hit.ray.org_x = vec_camera_origin.x;
	t_ray_hit.ray.org_y = vec_camera_origin.y;
	t_ray_hit.ray.org_z = vec_camera_origin.z;

	t_ray_hit.ray.dir_x = vec_ray_direction.x;
	t_ray_hit.ray.dir_y = vec_ray_direction.y;
	t_ray_hit.ray.dir_z = vec_ray_direction.z;

	t_ray_hit.ray.tnear = 0.001f;
	t_ray_hit.ray.tfar = FLT_MAX;
	t_ray_hit.ray.mask = -1;
	t_ray_hit.ray.flags = 0;

	t_ray_hit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	t_ray_hit.hit.primID = RTC_INVALID_GEOMETRY_ID;

	RTCIntersectContext t_context;
	rtcInitIntersectContext(&t_context);
	rtcIntersect1(p_scene, &t_context, &t_ray_hit);

	if (t_ray_hit.hit.geomID == RTC_INVALID_GEOMETRY_ID) {
		return glm::vec3(0.0f);
	}

	RTCGeometry p_geom = rtcGetGeometry(p_scene, t_ray_hit.hit.geomID);
	GeometryUserData *p_user_data =
		(GeometryUserData *)rtcGetGeometryUserData(p_geom);
	const tinyobj::mesh_t *p_mesh = p_user_data->mesh_ptr;

	unsigned int ui_prim_id = t_ray_hit.hit.primID;
	int i_mat_id = 0;
	if (ui_prim_id < p_mesh->material_ids.size()) {
		i_mat_id = p_mesh->material_ids[ui_prim_id];
	}

	glm::vec3 vec_material_color(1.0f, 0.0f, 1.0f);
	if (i_mat_id >= 0 &&
	    i_mat_id < static_cast<int>(vec_materials.size())) {
		const tinyobj::material_t &t_mat = vec_materials[i_mat_id];
		vec_material_color = glm::vec3(
			t_mat.diffuse[0], t_mat.diffuse[1], t_mat.diffuse[2]);
	}

	glm::vec3 vec_intersect_pt = glm::vec3(t_ray_hit.ray.org_x,
					       t_ray_hit.ray.org_y,
					       t_ray_hit.ray.org_z) +
				     (t_ray_hit.ray.tfar * vec_ray_direction);

	glm::vec3 vec_normal = glm::normalize(glm::vec3(
		t_ray_hit.hit.Ng_x, t_ray_hit.hit.Ng_y, t_ray_hit.hit.Ng_z));

	glm::vec3 vec_lambert_color = compute_lambert_color(
		vec_normal, vec_intersect_pt, vec_material_color, p_scene);

	return vec_lambert_color;
}