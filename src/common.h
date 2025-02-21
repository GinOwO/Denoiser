#pragma once

#include <tiny_obj_loader.h>

struct Vertex {
	float x, y, z;
};

struct Triangle {
	unsigned int v0, v1, v2;
};

struct GeometryUserData {
	const tinyobj::mesh_t *mesh_ptr;
};