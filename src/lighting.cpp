#include "lighting.h"

#include <cstdint>
#include <cstring>
#include <cfloat>
#include <random>

static constexpr int32_t LIGHT_BOUNCE_DEPTH = 3;
static constexpr int32_t SHADOW_SAMPLES = 64;
static constexpr glm::vec3 LIGHT_POS(-278.0f, 548.0f, -279.6f);
static constexpr glm::vec3 LIGHT_COLOR(0xff / 255.0f, 0xbb / 255.0f,
				       0x73 / 255.0f);
static constexpr float LIGHT_INTENSITY = 5.0f;
static constexpr float LIGHT_WIDTH = 200.0f;
static constexpr float LIGHT_HEIGHT = 225.0f;

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

	int32_t grid_size = static_cast<int32_t>(sqrt(i_num_samples));
	float cell_width = f_light_width / grid_size;
	float cell_height = f_light_height / grid_size;

	std::vector<glm::vec2> offsets(i_num_samples);
	std::uniform_real_distribution<float> jitter(0.0f, 1.0f);

	int32_t index = 0;
	for (int32_t i = 0; i < grid_size && index < i_num_samples; i++) {
		for (int32_t j = 0; j < grid_size && index < i_num_samples;
		     j++) {
			float base_x = (i + jitter(rng)) * cell_width -
				       f_light_width * 0.5f;
			float base_z = (j + jitter(rng)) * cell_height -
				       f_light_height * 0.5f;

			offsets[index++] = glm::vec2(base_x, base_z);
		}
	}

	for (int32_t i = offsets.size() - 1; i > 0; i--) {
		int32_t j = std::uniform_int_distribution<int32_t>(0, i)(rng);
		std::swap(offsets[i], offsets[j]);
	}

	return offsets;
}

static float smoothstep(float edge0, float edge1, float x)
{
	float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float compute_shadow_factor(const RTCScene &p_scene,
				   const glm::vec3 &vec_point,
				   const glm::vec3 &vec_light_pos,
				   float f_light_width, float f_light_height,
				   int32_t i_num_samples = 32)
{
	static thread_local std::vector<glm::vec2> precomputed_offsets;
	if (precomputed_offsets.size() != static_cast<size_t>(i_num_samples)) {
		precomputed_offsets = generate_stratified_offsets(
			i_num_samples, f_light_width, f_light_height);
	}

	static thread_local RTCIntersectContext shadow_context;

	float f_shadow_sum = 0.0f;
	for (int32_t i = 0; i < i_num_samples; i++) {
		const glm::vec2 &offset = precomputed_offsets[i];
		glm::vec3 sample_light_pos =
			vec_light_pos + glm::vec3(offset.x, 0.0f, offset.y);
		glm::vec3 sample_dir = sample_light_pos - vec_point;
		float f_sample_dist = glm::length(sample_dir);
		sample_dir = glm::normalize(sample_dir);

		RTCRayHit t_shadow_hit;
		std::memset(&t_shadow_hit, 0, sizeof(t_shadow_hit));
		t_shadow_hit.ray.org_x = vec_point.x;
		t_shadow_hit.ray.org_y = vec_point.y;
		t_shadow_hit.ray.org_z = vec_point.z;
		t_shadow_hit.ray.dir_x = sample_dir.x;
		t_shadow_hit.ray.dir_y = sample_dir.y;
		t_shadow_hit.ray.dir_z = sample_dir.z;
		t_shadow_hit.ray.tnear = 0.001f;
		t_shadow_hit.ray.tfar = f_sample_dist - 0.001f;
		t_shadow_hit.ray.mask = -1;
		t_shadow_hit.ray.flags = 0;
		rtcInitIntersectContext(&shadow_context);
		rtcIntersect1(p_scene, &shadow_context, &t_shadow_hit);

		float sample_shadow = 1.0f;
		if (t_shadow_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
			float hit_distance = t_shadow_hit.ray.tfar;
			sample_shadow =
				1.0f - smoothstep(0.0f, 0.15f * f_sample_dist,
						  f_sample_dist - hit_distance);
		}
		f_shadow_sum += sample_shadow;
	}
	return f_shadow_sum / static_cast<float>(i_num_samples);
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
	glm::vec3 vec_ambient = vec_material_color * f_ambient_strength;

	glm::vec3 vec_light_dir = LIGHT_POS - vec_point;
	float f_dist_to_light = glm::length(vec_light_dir);
	vec_light_dir = glm::normalize(vec_light_dir);

	const float f_n_dot_l = glm::dot(vec_normal, vec_light_dir);
	if (f_n_dot_l <= 0.0f) {
		return vec_ambient;
	}

	const bool b_occluded = is_in_shadow(p_scene,
					     vec_point + 0.001f * vec_normal,
					     vec_light_dir, f_dist_to_light);
	// if (b_occluded) {
	// 	return vec_ambient;
	// }

	const float f_shadow_factor = compute_shadow_factor(
		p_scene, vec_point + 0.001f * vec_normal, LIGHT_POS,
		LIGHT_WIDTH, LIGHT_HEIGHT, SHADOW_SAMPLES);

	const float f_lambert_term = f_n_dot_l * LIGHT_INTENSITY;
	glm::vec3 vec_lit_color = vec_material_color * LIGHT_COLOR *
				  f_lambert_term * f_shadow_factor;

	return glm::clamp(vec_ambient + vec_lit_color, 0.0f, 1.0f);
}

SurfaceInfo lighting::trace_ray_with_buffers(
	const Scene &S_scene, const Camera &S_camera, RTCDevice p_device,
	int32_t i_pixel_x, int32_t i_pixel_y, int32_t i_width, int32_t i_height)
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

	RTCRayHit t_ray_hit;
	std::memset(&t_ray_hit, 0, sizeof(t_ray_hit));

	t_ray_hit.ray.org_x = S_camera.vec_camera_origin.x;
	t_ray_hit.ray.org_y = S_camera.vec_camera_origin.y;
	t_ray_hit.ray.org_z = S_camera.vec_camera_origin.z;
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
	rtcIntersect1(S_scene.p_RTCscene, &t_context, &t_ray_hit);

	SurfaceInfo result;
	if (t_ray_hit.hit.geomID == RTC_INVALID_GEOMETRY_ID) {
		result.color = glm::vec3(0.0f);
		result.albedo = glm::vec3(0.0f);
		result.normal = glm::vec3(0.0f);
		return result;
	}

	RTCGeometry p_geom =
		rtcGetGeometry(S_scene.p_RTCscene, t_ray_hit.hit.geomID);
	GeometryUserData *p_user_data =
		(GeometryUserData *)rtcGetGeometryUserData(p_geom);
	const tinyobj::mesh_t *p_mesh = p_user_data->mesh_ptr;

	int i_mat_id = 0;
	if (t_ray_hit.hit.primID < p_mesh->material_ids.size()) {
		i_mat_id = p_mesh->material_ids[t_ray_hit.hit.primID];
	}

	result.normal = glm::normalize(glm::vec3(
		t_ray_hit.hit.Ng_x, t_ray_hit.hit.Ng_y, t_ray_hit.hit.Ng_z));

	if (i_mat_id >= 0 &&
	    i_mat_id < static_cast<int>(S_scene.v_materials.size())) {
		const tinyobj::material_t &mat = S_scene.v_materials[i_mat_id];
		result.albedo = glm::vec3(mat.diffuse[0], mat.diffuse[1],
					  mat.diffuse[2]);
	} else {
		result.albedo = glm::vec3(1.0f, 0.0f, 1.0f);
	}

	int max_depth = 3;
	result.color = trace_ray_recursive(S_scene.p_RTCscene, p_device,
					   S_camera.vec_camera_origin,
					   vec_ray_direction, max_depth,
					   S_scene.v_materials,
					   S_scene.f_ambient_intensity);

	return result;
}

glm::vec3 lighting::trace_ray(const Scene &S_scene, const Camera &S_camera,
			      RTCDevice p_device, int32_t i_pixel_x,
			      int32_t i_pixel_y, int32_t i_width,
			      int32_t i_height)
{
	return trace_ray_with_buffers(S_scene, S_camera, p_device, i_pixel_x,
				      i_pixel_y, i_width, i_height)
		.color;
}