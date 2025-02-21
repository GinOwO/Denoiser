#include <embree3/rtcore.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cfloat>

struct Vertex {
	float x, y, z;
};

struct Triangle {
	unsigned int v0, v1, v2;
};

struct GeometryUserData {
	const tinyobj::mesh_t *meshPtr;
};

static void glfwErrorCallback(int error, const char *description)
{
	std::cerr << "GLFW Error (" << error << "): " << description << "\n";
}

static void embreeErrorFunc(void *, RTCError error, const char *str)
{
	std::cerr << "Embree error (" << error << "): " << str << "\n";
}

int main()
{
	// ------------------- GLFW Initialization -------------------
	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW\n";
		return EXIT_FAILURE;
	}

	GLFWwindow *window = glfwCreateWindow(
		1024, 1024, "Cornell Box Renderer - Face-based Materials",
		nullptr, nullptr);
	if (!window) {
		std::cerr << "Failed to create GLFW window\n";
		glfwTerminate();
		return EXIT_FAILURE;
	}
	glfwMakeContextCurrent(window);

	// ------------------- Embree Setup -------------------
	RTCDevice rtcDevice = rtcNewDevice(nullptr);
	if (!rtcDevice) {
		std::cerr << "Error: Unable to create Embree device\n";
		return EXIT_FAILURE;
	}
	rtcSetDeviceErrorFunction(rtcDevice, embreeErrorFunc, nullptr);

	RTCScene scene = rtcNewScene(rtcDevice);

	// ------------------- Load Cornell Box OBJ -------------------
	std::string inputfile = "/home/gin/Desktop/denoise/src/CornellBox.obj";
	std::string base_dir = "/home/gin/Desktop/denoise/src/";
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
				    inputfile.c_str(), base_dir.c_str());
	if (!err.empty()) {
		std::cerr << "tinyobjloader error: " << err << "\n";
	}
	if (!ret) {
		std::cerr << "Failed to load/parse .obj file.\n";
		return EXIT_FAILURE;
	}

	for (size_t s = 0; s < shapes.size(); s++) {
		const tinyobj::mesh_t &mesh = shapes[s].mesh;

		size_t numVertices = attrib.vertices.size() / 3;
		size_t numTriangles = mesh.indices.size() / 3;

		RTCGeometry geom =
			rtcNewGeometry(rtcDevice, RTC_GEOMETRY_TYPE_TRIANGLE);

		// Allocate vertex buffer
		Vertex *vertices = (Vertex *)rtcSetNewGeometryBuffer(
			geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
			sizeof(Vertex), numVertices);

		for (size_t i = 0; i < numVertices; i++) {
			vertices[i].x = attrib.vertices[3 * i + 0];
			vertices[i].y = attrib.vertices[3 * i + 1];
			vertices[i].z = attrib.vertices[3 * i + 2];
		}

		// Allocate index buffer
		Triangle *triangles = (Triangle *)rtcSetNewGeometryBuffer(
			geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
			sizeof(Triangle), numTriangles);

		for (size_t i = 0; i < numTriangles; i++) {
			triangles[i].v0 = mesh.indices[3 * i + 0].vertex_index;
			triangles[i].v1 = mesh.indices[3 * i + 1].vertex_index;
			triangles[i].v2 = mesh.indices[3 * i + 2].vertex_index;
		}

		// Attach user data to geometry so we can retrieve the mesh pointer in the shader
		auto *userData = new GeometryUserData;
		userData->meshPtr = &mesh;
		rtcSetGeometryUserData(geom, userData);

		rtcCommitGeometry(geom);

		// Attach geometry with unique ID = s
		rtcAttachGeometryByID(scene, geom, static_cast<unsigned>(s));
		rtcReleaseGeometry(geom);
	}

	rtcCommitScene(scene);

	// ------------------- Framebuffer Setup -------------------
	const int width = 1024, height = 1024;
	std::vector<float> framebuffer(width * height * 3, 0.0f);

	// ------------------- Camera Setup -------------------
	glm::vec3 sceneCenter(-278.0f, 274.4f, -279.6f);
	glm::vec3 cameraOrigin(sceneCenter.x, sceneCenter.y, 800.0f);
	glm::vec3 viewDir = glm::normalize(sceneCenter - cameraOrigin);
	glm::vec3 up(0.0f, 1.0f, 0.0f);
	glm::vec3 right = glm::normalize(glm::cross(viewDir, up));

	float fov = 45.0f;
	float theta = glm::radians(fov);
	float focalLength = glm::length(sceneCenter - cameraOrigin);
	float viewportHeight = 2.0f * focalLength * tan(theta / 2.0f);
	float viewportWidth = viewportHeight;

	glm::vec3 lowerLeftCorner = cameraOrigin + viewDir * focalLength -
				    right * (viewportWidth * 0.5f) -
				    up * (viewportHeight * 0.5f);

	// ------------------- Render Loop -------------------
	while (!glfwWindowShouldClose(window)) {
		std::fill(framebuffer.begin(), framebuffer.end(), 0.0f);

		// For each pixel, cast a ray from the camera through the viewport.
		for (int j = 0; j < height; j++) {
			for (int i = 0; i < width; i++) {
				float u = float(i) / float(width - 1);
				float v = float(j) / float(height - 1);
				glm::vec3 pixelPos =
					lowerLeftCorner +
					right * (u * viewportWidth) +
					up * (v * viewportHeight);
				glm::vec3 rayDir =
					glm::normalize(pixelPos - cameraOrigin);

				RTCRayHit rayhit;
				std::memset(&rayhit, 0, sizeof(rayhit));
				rayhit.ray.org_x = cameraOrigin.x;
				rayhit.ray.org_y = cameraOrigin.y;
				rayhit.ray.org_z = cameraOrigin.z;
				rayhit.ray.dir_x = rayDir.x;
				rayhit.ray.dir_y = rayDir.y;
				rayhit.ray.dir_z = rayDir.z;
				rayhit.ray.tnear = 0.001f;
				rayhit.ray.tfar = FLT_MAX;
				rayhit.ray.mask = -1;
				rayhit.ray.flags = 0;
				rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
				rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;

				RTCIntersectContext context;
				rtcInitIntersectContext(&context);
				rtcIntersect1(scene, &context, &rayhit);

				int idx = (j * width + i) * 3;
				if (rayhit.hit.geomID !=
				    RTC_INVALID_GEOMETRY_ID) {
					RTCGeometry thisGeom = rtcGetGeometry(
						scene, rayhit.hit.geomID);
					auto *userData = (GeometryUserData *)
						rtcGetGeometryUserData(
							thisGeom);
					const tinyobj::mesh_t *meshPtr =
						userData->meshPtr;

					unsigned int primID = rayhit.hit.primID;
					int matID = 0;
					if (primID <
					    meshPtr->material_ids.size()) {
						matID = meshPtr->material_ids
								[primID];
					}
					if (matID < 0 ||
					    matID >= (int)materials.size()) {
						framebuffer[idx + 0] = 1.0f;
						framebuffer[idx + 1] = 0.0f;
						framebuffer[idx + 2] = 1.0f;
					} else {
						const tinyobj::material_t &mat =
							materials[matID];
						float r = mat.diffuse[0];
						float g = mat.diffuse[1];
						float b = mat.diffuse[2];
						framebuffer[idx + 0] = r;
						framebuffer[idx + 1] = g;
						framebuffer[idx + 2] = b;
					}
				} else {
					// Background color
					framebuffer[idx + 0] = 0.0f;
					framebuffer[idx + 1] = 0.0f;
					framebuffer[idx + 2] = 0.0f;
				}
			}
		}

		glClear(GL_COLOR_BUFFER_BIT);
		glDrawPixels(width, height, GL_RGB, GL_FLOAT,
			     framebuffer.data());
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// Cleanup
	rtcReleaseScene(scene);
	rtcReleaseDevice(rtcDevice);
	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}
