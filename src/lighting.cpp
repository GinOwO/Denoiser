#include "lighting.h"

#include <cstdint>
#include <cstring>
#include <cfloat>
#include <random>

static glm::vec3 cosine_weighted_sample(const glm::vec3 &normal)
{
	static thread_local std::mt19937 rng{ std::random_device{}() };
	static thread_local std::uniform_real_distribution<float> dist(0.0f,
								       1.0f);

	const float u1 = dist(rng);
	const float u2 = dist(rng);
	const float r = sqrt(u1);
	const float theta = 2.0f * 3.14159265f * u2;
	const float sample_x = r * cos(theta);
	const float sample_y = r * sin(theta);
	const float sample_z = sqrt(1.0f - u1);

	glm::vec3 tangent, bitangent;
	if (std::abs(normal.x) > std::abs(normal.z))
		tangent = glm::normalize(glm::vec3(-normal.y, normal.x, 0.0f));
	else
		tangent = glm::normalize(glm::vec3(0.0f, -normal.z, normal.y));
	bitangent = glm::cross(normal, tangent);

	glm::vec3 sample =
		sample_x * tangent + sample_y * bitangent + sample_z * normal;
	return glm::normalize(sample);
}

static glm::vec3
trace_ray_recursive(const RTCScene &p_scene, const RTCDevice &p_device,
		    const glm::vec3 &ray_origin, const glm::vec3 &ray_direction,
		    int i_depth,
		    const std::vector<tinyobj::material_t> &vec_materials,
		    float f_ambient_strength)
{
	if (i_depth <= 0) {
		return glm::vec3(0.0f);
	}

	RTCRayHit t_ray_hit;
	std::memset(&t_ray_hit, 0, sizeof(t_ray_hit));

	t_ray_hit.ray.org_x = ray_origin.x;
	t_ray_hit.ray.org_y = ray_origin.y;
	t_ray_hit.ray.org_z = ray_origin.z;
	t_ray_hit.ray.dir_x = ray_direction.x;
	t_ray_hit.ray.dir_y = ray_direction.y;
	t_ray_hit.ray.dir_z = ray_direction.z;
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

	glm::vec3 hit_point = ray_origin + ray_direction * t_ray_hit.ray.tfar;
	glm::vec3 normal = glm::normalize(glm::vec3(
		t_ray_hit.hit.Ng_x, t_ray_hit.hit.Ng_y, t_ray_hit.hit.Ng_z));

	RTCGeometry p_geom = rtcGetGeometry(p_scene, t_ray_hit.hit.geomID);
	GeometryUserData *p_user_data =
		(GeometryUserData *)rtcGetGeometryUserData(p_geom);
	const tinyobj::mesh_t *p_mesh = p_user_data->mesh_ptr;
	unsigned int ui_prim_id = t_ray_hit.hit.primID;
	int i_mat_id = 0;
	if (ui_prim_id < p_mesh->material_ids.size()) {
		i_mat_id = p_mesh->material_ids[ui_prim_id];
	}

	glm::vec3 diffuse_color(1.0f, 0.0f, 1.0f);
	glm::vec3 specular_color(0.0f);
	glm::vec3 emissive_color(0.0f);
	if (i_mat_id >= 0 &&
	    i_mat_id < static_cast<int>(vec_materials.size())) {
		const tinyobj::material_t &mat = vec_materials[i_mat_id];
		diffuse_color = glm::vec3(mat.diffuse[0], mat.diffuse[1],
					  mat.diffuse[2]);
		specular_color = glm::vec3(mat.specular[0], mat.specular[1],
					   mat.specular[2]);
		emissive_color = glm::vec3(mat.emission[0], mat.emission[1],
					   mat.emission[2]);
	}

	glm::vec3 direct = lighting::compute_lambert_color(
		normal, hit_point, diffuse_color, p_scene, f_ambient_strength);

	glm::vec3 new_ray_dir = cosine_weighted_sample(normal);
	glm::vec3 indirect = trace_ray_recursive(
		p_scene, p_device, hit_point + 0.001f * normal, new_ray_dir,
		i_depth - 1, vec_materials, f_ambient_strength);

	glm::vec3 reflection_dir = glm::reflect(-ray_direction, normal);
	glm::vec3 specular_radiance = trace_ray_recursive(
		p_scene, p_device, hit_point + 0.001f * normal, reflection_dir,
		i_depth - 1, vec_materials, f_ambient_strength);

	constexpr float f_diffuse_coeff = 0.8f;
	constexpr float f_specular_coeff = 0.5f;

	glm::vec3 final_radiance =
		emissive_color + direct + f_diffuse_coeff * indirect +
		specular_color * f_specular_coeff * specular_radiance;
	return final_radiance;
}

static std::vector<glm::vec2> generate_stratified_offsets(int32_t i_num_samples,
							  float f_light_width,
							  float f_light_height)
{
	static thread_local std::mt19937 rng{ std::random_device{}() };
	std::vector<glm::vec2> offsets(i_num_samples);
	std::uniform_real_distribution<float> dist_x(-f_light_width * 0.5f,
						     f_light_width * 0.5f);
	std::uniform_real_distribution<float> dist_z(-f_light_height * 0.5f,
						     f_light_height * 0.5f);
	for (int32_t i = 0; i < i_num_samples; i++) {
		offsets[i] = glm::vec2(dist_x(rng), dist_z(rng));
	}
	return offsets;
}

static float compute_shadow_factor_adaptive(
	const RTCScene &p_scene, const glm::vec3 &vec_point,
	const glm::vec3 &vec_light_pos, float f_light_width,
	float f_light_height, int32_t i_min_samples = 4,
	int32_t i_max_samples = 64, float f_variance_threshold = 0.005f)
{
	static thread_local std::vector<glm::vec2> precomputed_offsets;
	if (precomputed_offsets.size() != static_cast<size_t>(i_max_samples)) {
		precomputed_offsets = generate_stratified_offsets(
			i_max_samples, f_light_width, f_light_height);
	}

	static thread_local RTCIntersectContext shadow_context;

	float f_sum = 0.0f;
	float f_sum_sq = 0.0f;
	int32_t i_samples_taken = 0;

	for (int32_t i = 0; i < i_max_samples; i++) {
		const glm::vec2 &offset = precomputed_offsets[i];
		glm::vec3 sample_light_pos =
			vec_light_pos + glm::vec3(offset.x, 0.0f, offset.y);
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

		rtcInitIntersectContext(&shadow_context);
		rtcOccluded1(p_scene, &shadow_context, &t_ray);

		float f_sample_val = t_ray.tfar >= 0.0f;

		i_samples_taken++;
		f_sum += f_sample_val;
		f_sum_sq += f_sample_val * f_sample_val;

		if (i_samples_taken >= i_min_samples) {
			float f_mean = f_sum / i_samples_taken;
			float f_variance = (f_sum_sq / i_samples_taken) -
					   (f_mean * f_mean);
			if (f_variance < f_variance_threshold)
				return f_mean;
		}
	}
	return f_sum / static_cast<float>(i_samples_taken);
}

bool lighting::is_in_shadow(const RTCScene &p_scene, const glm::vec3 &vec_point,
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
	static constexpr glm::vec3 vec_light_color(
		252.0f / 255.0f, 249.0f / 255.0f, 217.0f / 255.0f);
	static constexpr float f_light_intensity = 5.0f;
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

	const float f_shadow_factor = compute_shadow_factor_adaptive(
		p_scene, vec_point + 0.001f * vec_normal, vec_light_pos,
		f_light_width, f_light_height, 4, 64, 0.005f);

	const float f_lambert_term = f_n_dot_l * f_light_intensity;
	glm::vec3 vec_lit_color =
		vec_material_color * vec_light_color * f_lambert_term;

	return vec_ambient + (f_shadow_factor * vec_lit_color);
}

glm::vec3 lighting::trace_ray(const Scene &S_scene, const Camera &S_camera,
			      RTCDevice p_device, int32_t i_pixel_x,
			      int32_t i_pixel_y, int32_t i_width,
			      int32_t i_height)
{
	const float f_u =
		static_cast<float>(i_pixel_x) / static_cast<float>(i_width - 1);
	const float f_v = static_cast<float>(i_pixel_y) /
			  static_cast<float>(i_height - 1);
	const glm::vec3 vec_pixel_position =
		S_camera.vec_lower_left_corner +
		S_camera.vec_right * (f_u * S_camera.f_viewport_width) +
		S_camera.vec_up * (f_v * S_camera.f_viewport_height);
	const glm::vec3 vec_ray_direction =
		glm::normalize(vec_pixel_position - S_camera.vec_camera_origin);
	int max_depth = 3;
	return trace_ray_recursive(S_scene.p_RTCscene, p_device,
				   S_camera.vec_camera_origin,
				   vec_ray_direction, max_depth,
				   S_scene.v_materials,
				   S_scene.f_ambient_intensity);
}
