#include "lighting.h"

#include <cstdint>
#include <cstring>
#include <cfloat>

static float compute_shadow_factor(RTCScene p_scene, const glm::vec3 &vec_point,
				   const glm::vec3 &vec_light_pos,
				   float f_light_width, float f_light_height,
				   int32_t i_num_samples = 16)
{
	float f_shadow_sum = 0.0f;
	for (int32_t i = 0; i < i_num_samples; i++) {
		float f_rand_x = (static_cast<float>(std::rand()) /
					  static_cast<float>(RAND_MAX) -
				  0.5f) *
				 f_light_width;
		float f_rand_z = (static_cast<float>(std::rand()) /
					  static_cast<float>(RAND_MAX) -
				  0.5f) *
				 f_light_height;
		glm::vec3 sample_light_pos =
			vec_light_pos + glm::vec3(f_rand_x, 0.0f, f_rand_z);
		glm::vec3 sample_dir = sample_light_pos - vec_point;
		float f_sample_dist = glm::length(sample_dir);
		sample_dir = glm::normalize(sample_dir);

		RTCRay t_ray;
		std::memset(&t_ray, 0, sizeof(t_ray));
		t_ray.org_x = vec_point.x;
		t_ray.org_y = vec_point.y;
		t_ray.org_z = vec_point.z;
		t_ray.dir_x = sample_dir.x;
		t_ray.dir_y = sample_dir.y;
		t_ray.dir_z = sample_dir.z;
		t_ray.tnear = 0.001f;
		t_ray.tfar = f_sample_dist - 0.001f;
		t_ray.mask = -1;
		t_ray.flags = 0;
		RTCIntersectContext t_context;
		rtcInitIntersectContext(&t_context);
		rtcOccluded1(p_scene, &t_context, &t_ray);
		if (t_ray.tfar >= 0.0f)
			f_shadow_sum += 1.0f;
	}
	return f_shadow_sum / static_cast<float>(i_num_samples);
}

bool lighting::is_in_shadow(RTCScene p_scene, const glm::vec3 &vec_point,
			    const glm::vec3 &vec_light_dir,
			    float f_dist_to_light)
{
	RTCRayHit S_shadow_ray;
	std::memset(&S_shadow_ray, 0, sizeof(S_shadow_ray));

	S_shadow_ray.ray.org_x = vec_point.x;
	S_shadow_ray.ray.org_y = vec_point.y;
	S_shadow_ray.ray.org_z = vec_point.z;

	S_shadow_ray.ray.dir_x = vec_light_dir.x;
	S_shadow_ray.ray.dir_y = vec_light_dir.y;
	S_shadow_ray.ray.dir_z = vec_light_dir.z;

	S_shadow_ray.ray.tnear = 0.001f;
	S_shadow_ray.ray.tfar = f_dist_to_light - 0.001f;
	S_shadow_ray.ray.mask = -1;
	S_shadow_ray.ray.flags = 0;
	S_shadow_ray.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	S_shadow_ray.hit.primID = RTC_INVALID_GEOMETRY_ID;

	RTCIntersectContext S_context;
	rtcInitIntersectContext(&S_context);
	rtcOccluded1(p_scene, &S_context, &S_shadow_ray.ray);

	return (S_shadow_ray.ray.tfar < 0.0f);
}

glm::vec3 lighting::compute_lambert_color(const glm::vec3 &vec_normal,
					  const glm::vec3 &vec_point,
					  const glm::vec3 &vec_material_color,
					  RTCScene p_scene,
					  float f_ambient_strength)
{
	static constexpr glm::vec3 vec_light_pos(-278.0f, 548.0f, -279.6f);
	static constexpr glm::vec3 vec_light_color(1.0f, 1.0f, 1.0f);
	static constexpr float f_light_intensity = 25.0f;
	static constexpr float f_light_width = 100.0f;
	static constexpr float f_light_height = 100.0f;

	glm::vec3 vec_ambient = vec_material_color * f_ambient_strength;

	glm::vec3 vec_light_dir = vec_light_pos - vec_point;
	float f_dist_to_light = glm::length(vec_light_dir);
	vec_light_dir = glm::normalize(vec_light_dir);

	const float f_n_dot_l = glm::dot(vec_normal, vec_light_dir);
	if (f_n_dot_l <= 0.0f) {
		return vec_ambient;
	}

	const bool b_occluded = is_in_shadow(p_scene,
					     vec_point + 0.001f * vec_normal,
					     vec_light_dir, f_dist_to_light);
	if (b_occluded) {
		return vec_ambient;
	}

	const float f_shadow_factor = compute_shadow_factor(
		p_scene, vec_point + 0.001f * vec_normal, vec_light_pos,
		f_light_width, f_light_height, 16);

	const float f_lambert_term = f_n_dot_l * f_light_intensity;
	glm::vec3 vec_lit_color =
		vec_material_color * vec_light_color * f_lambert_term;

	return vec_ambient + (f_shadow_factor * vec_lit_color);
}

glm::vec3 lighting::trace_ray(Scene &S_scene, Camera &S_camera,
			      RTCDevice p_device, int32_t i_pixel_x,
			      int32_t i_pixel_y, int32_t i_width,
			      int32_t i_height)
{
	const glm::vec3 vec_lower_left_corner = S_camera.vec_lower_left_corner;
	const glm::vec3 vec_right = S_camera.vec_right;
	const glm::vec3 vec_up = S_camera.vec_up;
	const glm::vec3 vec_camera_origin = S_camera.vec_camera_origin;
	const float f_viewport_width = S_camera.f_viewport_width;
	const float f_viewport_height = S_camera.f_viewport_height;
	const float f_u =
		static_cast<float>(i_pixel_x) / static_cast<float>(i_width - 1);
	const float f_v = static_cast<float>(i_pixel_y) /
			  static_cast<float>(i_height - 1);

	RTCScene p_scene = S_scene.p_RTCscene;
	std::vector<tinyobj::material_t> &vec_materials = S_scene.v_materials;

	const glm::vec3 vec_pixel_position =
		vec_lower_left_corner + vec_right * (f_u * f_viewport_width) +
		vec_up * (f_v * f_viewport_height);

	const glm::vec3 vec_ray_direction =
		glm::normalize(vec_pixel_position - vec_camera_origin);

	RTCRayHit S_ray_hit;
	std::memset(&S_ray_hit, 0, sizeof(S_ray_hit));

	S_ray_hit.ray.org_x = vec_camera_origin.x;
	S_ray_hit.ray.org_y = vec_camera_origin.y;
	S_ray_hit.ray.org_z = vec_camera_origin.z;

	S_ray_hit.ray.dir_x = vec_ray_direction.x;
	S_ray_hit.ray.dir_y = vec_ray_direction.y;
	S_ray_hit.ray.dir_z = vec_ray_direction.z;

	S_ray_hit.ray.tnear = 0.001f;
	S_ray_hit.ray.tfar = FLT_MAX;
	S_ray_hit.ray.mask = -1;
	S_ray_hit.ray.flags = 0;

	S_ray_hit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	S_ray_hit.hit.primID = RTC_INVALID_GEOMETRY_ID;

	RTCIntersectContext S_context;
	rtcInitIntersectContext(&S_context);
	rtcIntersect1(p_scene, &S_context, &S_ray_hit);

	if (S_ray_hit.hit.geomID == RTC_INVALID_GEOMETRY_ID) {
		return glm::vec3(0.0f);
	}

	RTCGeometry p_geom = rtcGetGeometry(p_scene, S_ray_hit.hit.geomID);
	GeometryUserData *p_user_data =
		(GeometryUserData *)rtcGetGeometryUserData(p_geom);
	const tinyobj::mesh_t *p_mesh = p_user_data->mesh_ptr;

	const uint32_t ui_prim_id = S_ray_hit.hit.primID;
	int32_t i_mat_id = 0;
	if (ui_prim_id < p_mesh->material_ids.size()) {
		i_mat_id = p_mesh->material_ids[ui_prim_id];
	}

	glm::vec3 vec_material_color(1.0f, 0.0f, 1.0f);
	if (i_mat_id >= 0 &&
	    i_mat_id < static_cast<int32_t>(vec_materials.size())) {
		const tinyobj::material_t &S_mat = vec_materials[i_mat_id];
		vec_material_color = glm::vec3(
			S_mat.diffuse[0], S_mat.diffuse[1], S_mat.diffuse[2]);
	}

	glm::vec3 vec_intersect_pt = glm::vec3(S_ray_hit.ray.org_x,
					       S_ray_hit.ray.org_y,
					       S_ray_hit.ray.org_z) +
				     (S_ray_hit.ray.tfar * vec_ray_direction);

	glm::vec3 vec_normal = glm::normalize(glm::vec3(
		S_ray_hit.hit.Ng_x, S_ray_hit.hit.Ng_y, S_ray_hit.hit.Ng_z));

	glm::vec3 vec_lambert_color = compute_lambert_color(
		vec_normal, vec_intersect_pt, vec_material_color, p_scene,
		S_scene.f_ambient_intensity);

	return vec_lambert_color;
}